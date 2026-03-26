#pragma once

#include <atomic>
#include <climits>
#include <cstddef>
#include <mutex>
#include <optional>
#include <semaphore>
#include <stdexcept>
#include <vector>

template <class T>
class BufferedChannel {
public:
    explicit BufferedChannel(int size)
        : buffer_(static_cast<std::size_t>(size)),
          capacity_(static_cast<std::size_t>(size)),
          free_slots_(size),
          ready_items_(0) {
        if (size <= 0) {
            throw std::invalid_argument("BufferedChannel size must be positive");
        }
    }

    void Send(const T& value) {
        if (closed_.load(std::memory_order_acquire)) {
            throw std::runtime_error("send to closed channel");
        }

        waiting_senders_.fetch_add(1, std::memory_order_acq_rel);

        if (closed_.load(std::memory_order_acquire)) {
            waiting_senders_.fetch_sub(1, std::memory_order_acq_rel);
            throw std::runtime_error("send to closed channel");
        }

        free_slots_.acquire();
        waiting_senders_.fetch_sub(1, std::memory_order_acq_rel);

        if (closed_.load(std::memory_order_acquire)) {
            free_slots_.release();
            throw std::runtime_error("send to closed channel");
        }

        {
            std::lock_guard lock(mutex_);
            if (closed_) {
                free_slots_.release();
                throw std::runtime_error("send to closed channel");
            }

            buffer_[tail_] = value;
            tail_ = Next(tail_);
            ++count_;
        }

        ready_items_.release();
    }

    std::optional<T> Recv() {
        for (;;) {
            {
                std::lock_guard lock(mutex_);
                if (count_ > 0) {
                    std::optional<T> result = std::move(buffer_[head_]);
                    buffer_[head_].reset();
                    head_ = Next(head_);
                    --count_;
                    free_slots_.release();
                    return result;
                }

                if (closed_) {
                    return std::nullopt;
                }
            }

            waiting_receivers_.fetch_add(1, std::memory_order_acq_rel);

            {
                std::lock_guard lock(mutex_);
                if (count_ > 0) {
                    waiting_receivers_.fetch_sub(1, std::memory_order_acq_rel);

                    std::optional<T> result = std::move(buffer_[head_]);
                    buffer_[head_].reset();
                    head_ = Next(head_);
                    --count_;
                    free_slots_.release();
                    return result;
                }

                if (closed_) {
                    waiting_receivers_.fetch_sub(1, std::memory_order_acq_rel);
                    return std::nullopt;
                }
            }

            ready_items_.acquire();
            waiting_receivers_.fetch_sub(1, std::memory_order_acq_rel);

            {
                std::lock_guard lock(mutex_);
                if (count_ > 0) {
                    std::optional<T> result = std::move(buffer_[head_]);
                    buffer_[head_].reset();
                    head_ = Next(head_);
                    --count_;
                    free_slots_.release();
                    return result;
                }

                if (closed_) {
                    return std::nullopt;
                }
            }
        }
    }

    void Close() {
        bool expected = false;
        if (!closed_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            return;
        }

        const int senders = waiting_senders_.load(std::memory_order_acquire);
        const int receivers = waiting_receivers_.load(std::memory_order_acquire);

        if (senders > 0) {
            free_slots_.release(senders);
        }
        if (receivers > 0) {
            ready_items_.release(receivers);
        }
    }

private:
    std::size_t Next(std::size_t index) const {
        return (index + 1) % capacity_;
    }

private:
    std::vector<std::optional<T>> buffer_;
    const std::size_t capacity_;

    std::size_t head_ = 0;
    std::size_t tail_ = 0;
    std::size_t count_ = 0;

    std::mutex mutex_;

    std::counting_semaphore<INT_MAX> free_slots_;
    std::counting_semaphore<INT_MAX> ready_items_;

    std::atomic<bool> closed_{false};
    std::atomic<int> waiting_senders_{0};
    std::atomic<int> waiting_receivers_{0};
};