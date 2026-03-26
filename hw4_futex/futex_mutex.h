#pragma once

#include <atomic>
#include <cerrno>
#include <climits>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

class FutexMutex {
public:
    FutexMutex() noexcept = default;

    FutexMutex(const FutexMutex&) = delete;
    FutexMutex& operator=(const FutexMutex&) = delete;

    void lock() noexcept {
        int expected = k_unlocked;
        if (state_.compare_exchange_strong(
                expected,
                k_locked_no_waiters,
                std::memory_order_acquire,
                std::memory_order_relaxed)) {
            return;
        }

        lock_slow();
    }

    bool try_lock() noexcept {
        int expected = k_unlocked;
        return state_.compare_exchange_strong(
            expected,
            k_locked_no_waiters,
            std::memory_order_acquire,
            std::memory_order_relaxed);
    }

    void unlock() noexcept {
        const int old = state_.fetch_sub(1, std::memory_order_release);

        if (old != k_locked_no_waiters) {
            state_.store(k_unlocked, std::memory_order_release);
            futex_wake_one();
        }
    }

private:
    static constexpr int k_unlocked = 0;
    static constexpr int k_locked_no_waiters = 1;
    static constexpr int k_locked_with_waiters = 2;

    alignas(4) std::atomic<int> state_{k_unlocked};

    static int futex_wait(std::atomic<int>* addr, int expected) noexcept {
        return static_cast<int>(syscall(
            SYS_futex,
            reinterpret_cast<int*>(addr),
            FUTEX_WAIT_PRIVATE,
            expected,
            nullptr,
            nullptr,
            0));
    }

    void futex_wake_one() noexcept {
        static_cast<void>(syscall(
            SYS_futex,
            reinterpret_cast<int*>(&state_),
            FUTEX_WAKE_PRIVATE,
            1,
            nullptr,
            nullptr,
            0));
    }

    void lock_slow() noexcept {
        int current = state_.exchange(k_locked_with_waiters, std::memory_order_acquire);

        while (current != k_unlocked) {
            futex_wait(&state_, k_locked_with_waiters);
            current = state_.exchange(k_locked_with_waiters, std::memory_order_acquire);
        }
    }
};