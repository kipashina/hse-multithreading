#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

class ShmQueueError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

inline void check_syscall(bool ok, const char* what) {
    if (!ok) {
        throw ShmQueueError(std::format("{} failed", what));
    }
}

inline void check_fd(int fd, const char* what) {
    if (fd == -1) {
        throw ShmQueueError(std::format("{} failed", what));
    }
}

class SharedMemoryRegion {
public:
    SharedMemoryRegion(std::string path, std::size_t size, bool create)
        : path_(std::move(path)), size_(size) {
        int flags = create ? (O_CREAT | O_RDWR) : O_RDWR;
        fd_ = shm_open(path_.c_str(), flags, 0666);
        check_fd(fd_, "shm_open");

        if (create) {
            if (ftruncate(fd_, static_cast<off_t>(size_)) == -1) {
                close(fd_);
                throw ShmQueueError("ftruncate failed");
            }
        }

        ptr_ = mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
        if (ptr_ == MAP_FAILED) {
            close(fd_);
            throw ShmQueueError("mmap failed");
        }
    }

    SharedMemoryRegion(const SharedMemoryRegion&) = delete;
    SharedMemoryRegion& operator=(const SharedMemoryRegion&) = delete;

    SharedMemoryRegion(SharedMemoryRegion&& other) noexcept
        : path_(std::move(other.path_)),
          size_(other.size_),
          fd_(other.fd_),
          ptr_(other.ptr_) {
        other.fd_ = -1;
        other.ptr_ = nullptr;
        other.size_ = 0;
    }

    SharedMemoryRegion& operator=(SharedMemoryRegion&& other) noexcept {
        if (this != &other) {
            cleanup();
            path_ = std::move(other.path_);
            size_ = other.size_;
            fd_ = other.fd_;
            ptr_ = other.ptr_;
            other.fd_ = -1;
            other.ptr_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    ~SharedMemoryRegion() {
        cleanup();
    }

    void* data() const {
        return ptr_;
    }

    std::size_t size() const {
        return size_;
    }

    static void unlink(const std::string& path) {
        shm_unlink(path.c_str());
    }

private:
    void cleanup() {
        if (ptr_ && ptr_ != MAP_FAILED) {
            munmap(ptr_, size_);
            ptr_ = nullptr;
        }
        if (fd_ != -1) {
            close(fd_);
            fd_ = -1;
        }
    }

private:
    std::string path_;
    std::size_t size_{};
    int fd_{-1};
    void* ptr_{nullptr};
};

class MpscShmQueue {
public:
    static constexpr std::uint64_t k_magic = 0x4D50534351554555ULL; // "MPSCQUEU"
    static constexpr std::uint32_t k_version = 1;
    static constexpr std::uint16_t k_padding_type = 0xFFFF;
    static constexpr std::size_t k_alignment = 8;

    static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free);

    struct Message {
        std::uint16_t type{};
        std::vector<std::byte> payload;
    };

private:
    struct alignas(64) SharedHeader {
        std::uint64_t magic{};
        std::uint32_t version{};
        std::uint32_t capacity{};

        alignas(64) std::atomic<std::uint64_t> head{0};
        alignas(64) std::atomic<std::uint64_t> reserve_tail{0};

        std::uint8_t reserved[64]{};
    };

    struct alignas(8) RecordHeader {
        std::atomic<std::uint32_t> committed{0};
        std::uint16_t type{};
        std::uint16_t flags{};
        std::uint32_t payload_size{};
        std::uint32_t total_size{};
    };

    static constexpr std::size_t align_up(std::size_t value, std::size_t alignment) {
        return (value + alignment - 1) / alignment * alignment;
    }

    static constexpr std::size_t buffer_offset() {
        return align_up(sizeof(SharedHeader), 64);
    }

public:
    class ProducerNode {
    public:
        ProducerNode(const std::string& shm_path, std::size_t capacity)
            : region_(shm_path, buffer_offset() + capacity, true) {
            header_ = reinterpret_cast<SharedHeader*>(region_.data());
            buffer_ = reinterpret_cast<std::byte*>(region_.data()) + buffer_offset();
            capacity_ = capacity;

            initialize_or_validate();
        }

        bool send(std::uint16_t type, const void* data, std::uint32_t size) {
            if (size + sizeof(RecordHeader) > capacity_) {
                return false;
            }

            const std::uint32_t total_size =
                static_cast<std::uint32_t>(align_up(sizeof(RecordHeader) + size, k_alignment));

            while (true) {
                const std::uint64_t head =
                    header_->head.load(std::memory_order_acquire);
                std::uint64_t tail =
                    header_->reserve_tail.load(std::memory_order_relaxed);

                const std::size_t index = static_cast<std::size_t>(tail % capacity_);
                const std::size_t remain = capacity_ - index;

                const std::size_t padding = (remain < total_size) ? remain : 0;
                const std::size_t reservation = padding + total_size;

                if (tail - head + reservation > capacity_) {
                    return false;
                }

                if (!header_->reserve_tail.compare_exchange_weak(
                        tail,
                        tail + reservation,
                        std::memory_order_acq_rel,
                        std::memory_order_relaxed)) {
                    continue;
                }

                std::size_t write_index = index;

                if (padding != 0) {
                    if (padding >= sizeof(RecordHeader)) {
                        auto* pad = reinterpret_cast<RecordHeader*>(buffer_ + write_index);
                        pad->committed.store(0, std::memory_order_relaxed);
                        pad->type = k_padding_type;
                        pad->flags = 0;
                        pad->payload_size = 0;
                        pad->total_size = static_cast<std::uint32_t>(padding);
                        pad->committed.store(1, std::memory_order_release);
                    }
                    write_index = 0;
                }

                auto* record = reinterpret_cast<RecordHeader*>(buffer_ + write_index);
                record->committed.store(0, std::memory_order_relaxed);
                record->type = type;
                record->flags = 0;
                record->payload_size = size;
                record->total_size = total_size;

                if (size != 0) {
                    std::memcpy(
                        reinterpret_cast<std::byte*>(record) + sizeof(RecordHeader),
                        data,
                        size);
                }

                record->committed.store(1, std::memory_order_release);
                return true;
            }
        }

        template <typename T>
        bool send_pod(std::uint16_t type, const T& value) {
            static_assert(std::is_trivially_copyable_v<T>);
            return send(type, &value, static_cast<std::uint32_t>(sizeof(T)));
        }

        bool send_string(std::uint16_t type, std::string_view sv) {
            return send(type, sv.data(), static_cast<std::uint32_t>(sv.size()));
        }

    private:
        void initialize_or_validate() {
            if (header_->magic == k_magic) {
                validate_header();
                return;
            }

            header_->magic = k_magic;
            header_->version = k_version;
            header_->capacity = static_cast<std::uint32_t>(capacity_);
            header_->head.store(0, std::memory_order_relaxed);
            header_->reserve_tail.store(0, std::memory_order_relaxed);

            std::memset(buffer_, 0, capacity_);
        }

        void validate_header() const {
            if (header_->magic != k_magic) {
                throw ShmQueueError("bad shared memory magic");
            }
            if (header_->version != k_version) {
                throw ShmQueueError("protocol version mismatch");
            }
            if (header_->capacity != capacity_) {
                throw ShmQueueError("shared memory capacity mismatch");
            }
        }

    private:
        SharedMemoryRegion region_;
        SharedHeader* header_{};
        std::byte* buffer_{};
        std::size_t capacity_{};
    };

    class ConsumerNode {
    public:
        ConsumerNode(const std::string& shm_path, std::size_t capacity)
            : region_(shm_path, buffer_offset() + capacity, false) {
            header_ = reinterpret_cast<SharedHeader*>(region_.data());
            buffer_ = reinterpret_cast<std::byte*>(region_.data()) + buffer_offset();
            capacity_ = capacity;

            validate_header();
        }

        std::optional<Message> try_read() {
            while (true) {
                std::uint64_t head = header_->head.load(std::memory_order_relaxed);
                const std::size_t index = static_cast<std::size_t>(head % capacity_);
                const std::size_t remain = capacity_ - index;

                if (remain < sizeof(RecordHeader)) {
                    header_->head.store(head + remain, std::memory_order_release);
                    continue;
                }

                auto* record = reinterpret_cast<RecordHeader*>(buffer_ + index);
                if (record->committed.load(std::memory_order_acquire) == 0) {
                    return std::nullopt;
                }

                const std::uint16_t type = record->type;
                const std::uint32_t payload_size = record->payload_size;
                const std::uint32_t total_size = record->total_size;

                if (total_size == 0 || total_size > capacity_) {
                    throw ShmQueueError("corrupted record: bad total size");
                }

                if (type == k_padding_type) {
                    record->committed.store(0, std::memory_order_release);
                    header_->head.store(head + total_size, std::memory_order_release);
                    continue;
                }

                Message msg;
                msg.type = type;
                msg.payload.resize(payload_size);

                if (payload_size != 0) {
                    std::memcpy(
                        msg.payload.data(),
                        reinterpret_cast<std::byte*>(record) + sizeof(RecordHeader),
                        payload_size);
                }

                record->committed.store(0, std::memory_order_release);
                header_->head.store(head + total_size, std::memory_order_release);

                return msg;
            }
        }

        std::optional<Message> try_read_only(std::uint16_t wanted_type) {
            while (true) {
                auto msg = try_read();
                if (!msg.has_value()) {
                    return std::nullopt;
                }
                if (msg->type == wanted_type) {
                    return msg;
                }
            }
        }

    private:
        void validate_header() const {
            if (header_->magic != k_magic) {
                throw ShmQueueError("bad shared memory magic");
            }
            if (header_->version != k_version) {
                throw ShmQueueError("protocol version mismatch");
            }
            if (header_->capacity != capacity_) {
                throw ShmQueueError("shared memory capacity mismatch");
            }
        }

    private:
        SharedMemoryRegion region_;
        SharedHeader* header_{};
        std::byte* buffer_{};
        std::size_t capacity_{};
    };
};