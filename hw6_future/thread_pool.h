#pragma once

#include <condition_variable>
#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace sc {

// =========================
// Shared state
// =========================

template <class T>
class SharedState {
public:
    SharedState() = default;

    SharedState(const SharedState&) = delete;
    SharedState& operator=(const SharedState&) = delete;

    void set_value(T value) {
        {
            std::lock_guard lock(mutex_);
            if (ready_) {
                throw std::logic_error("SharedState already satisfied");
            }
            value_.emplace(std::move(value));
            ready_ = true;
        }
        cv_.notify_all();
    }

    void set_exception(std::exception_ptr ex) {
        {
            std::lock_guard lock(mutex_);
            if (ready_) {
                throw std::logic_error("SharedState already satisfied");
            }
            exception_ = ex;
            ready_ = true;
        }
        cv_.notify_all();
    }

    T get() {
        wait();

        if (exception_) {
            std::rethrow_exception(exception_);
        }

        return std::move(*value_);
    }

    void wait() {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this] { return ready_; });
    }

    bool is_ready() const {
        std::lock_guard lock(mutex_);
        return ready_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool ready_ = false;
    std::optional<T> value_;
    std::exception_ptr exception_;
};

template <>
class SharedState<void> {
public:
    SharedState() = default;

    SharedState(const SharedState&) = delete;
    SharedState& operator=(const SharedState&) = delete;

    void set_value() {
        {
            std::lock_guard lock(mutex_);
            if (ready_) {
                throw std::logic_error("SharedState already satisfied");
            }
            ready_ = true;
        }
        cv_.notify_all();
    }

    void set_exception(std::exception_ptr ex) {
        {
            std::lock_guard lock(mutex_);
            if (ready_) {
                throw std::logic_error("SharedState already satisfied");
            }
            exception_ = ex;
            ready_ = true;
        }
        cv_.notify_all();
    }

    void get() {
        wait();

        if (exception_) {
            std::rethrow_exception(exception_);
        }
    }

    void wait() {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this] { return ready_; });
    }

    bool is_ready() const {
        std::lock_guard lock(mutex_);
        return ready_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool ready_ = false;
    std::exception_ptr exception_;
};

// =========================
// Future
// =========================

template <class T>
class Future {
public:
    Future() = default;

    explicit Future(std::shared_ptr<SharedState<T>> state)
        : state_(std::move(state)) {}

    Future(const Future&) = delete;
    Future& operator=(const Future&) = delete;

    Future(Future&&) noexcept = default;
    Future& operator=(Future&&) noexcept = default;

    T get() {
        ensure_valid();
        return state_->get();
    }

    void wait() const {
        ensure_valid();
        state_->wait();
    }

    bool is_ready() const {
        ensure_valid();
        return state_->is_ready();
    }

    bool valid() const noexcept {
        return static_cast<bool>(state_);
    }

private:
    void ensure_valid() const {
        if (!state_) {
            throw std::logic_error("Invalid future");
        }
    }

private:
    std::shared_ptr<SharedState<T>> state_;
};

// =========================
// ThreadPool
// =========================

class ThreadPool {
public:
    explicit ThreadPool(std::size_t thread_count) {
        if (thread_count == 0) {
            throw std::invalid_argument("thread_count must be > 0");
        }

        workers_.reserve(thread_count);
        for (std::size_t i = 0; i < thread_count; ++i) {
            workers_.emplace_back([this] {
                worker_loop();
            });
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    ~ThreadPool() {
        shutdown();
    }

    template <class Func, class... Args>
    auto submit(Func&& func, Args&&... args)
        -> Future<std::invoke_result_t<std::decay_t<Func>, std::decay_t<Args>...>>
    {
        using Result = std::invoke_result_t<std::decay_t<Func>, std::decay_t<Args>...>;

        auto state = std::make_shared<SharedState<Result>>();

        auto bound_task = [state,
                           func = std::forward<Func>(func),
                           ... args = std::forward<Args>(args)]() mutable {
            try {
                if constexpr (std::is_void_v<Result>) {
                    std::invoke(std::move(func), std::move(args)...);
                    state->set_value();
                } else {
                    Result result = std::invoke(std::move(func), std::move(args)...);
                    state->set_value(std::move(result));
                }
            } catch (...) {
                state->set_exception(std::current_exception());
            }
        };

        {
            std::lock_guard lock(mutex_);
            if (stopping_) {
                throw std::logic_error("Submit on stopped ThreadPool");
            }
            tasks_.push(std::move(bound_task));
        }

        cv_.notify_one();
        return Future<Result>{std::move(state)};
    }

private:
    void worker_loop() {
        while (true) {
            std::function<void()> task;

            {
                std::unique_lock lock(mutex_);
                cv_.wait(lock, [this] {
                    return stopping_ || !tasks_.empty();
                });

                if (stopping_ && tasks_.empty()) {
                    return;
                }

                task = std::move(tasks_.front());
                tasks_.pop();
            }

            task();
        }
    }

    void shutdown() noexcept {
        {
            std::lock_guard lock(mutex_);
            if (stopping_) {
                return;
            }
            stopping_ = true;
        }

        cv_.notify_all();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;

    std::mutex mutex_;
    std::condition_variable cv_;
    bool stopping_ = false;
};

} // namespace sc