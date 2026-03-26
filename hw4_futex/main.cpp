#include "futex_mutex.h"

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

int main() {
    FutexMutex mutex;

    constexpr int k_threads = 4;
    std::vector<std::jthread> threads;
    threads.reserve(k_threads);

    for (int thread_index = 0; thread_index < k_threads; ++thread_index) {
        threads.emplace_back([&, thread_index]() {
            std::cout << "thread " << thread_index << " tries to lock\n";

            mutex.lock();
            std::cout << "thread " << thread_index << " acquired lock\n";

            std::this_thread::sleep_for(500ms);

            std::cout << "thread " << thread_index << " unlocks\n";
            mutex.unlock();
        });
    }

    threads.clear();

    std::cout << "done\n";
    return 0;
}