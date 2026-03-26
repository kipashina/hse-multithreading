#include "thread_pool.h"

#include <chrono>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

int main() {
    using namespace std::chrono_literals;

    sc::ThreadPool pool(4);
    std::mutex cout_mutex;

    auto print_line = [&cout_mutex](const std::string& text) {
        std::lock_guard lock(cout_mutex);
        std::cout << text << '\n';
    };

    auto thread_id_str = [] {
        return std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()));
    };

    auto f1 = pool.submit([&print_line, &thread_id_str](int a, int b) {
        print_line("task f1 in thread " + thread_id_str());
        return a + b;
    }, 10, 32);

    auto f2 = pool.submit([&print_line, &thread_id_str](std::string s) {
        print_line("task f2 in thread " + thread_id_str());
        return s + " world";
    }, std::string("hello"));

    auto f3 = pool.submit([&print_line, &thread_id_str] {
        print_line("task f3 in thread " + thread_id_str());
        std::this_thread::sleep_for(300ms);
        print_line("void task done");
    });

    auto f4 = pool.submit([&print_line, &thread_id_str] {
        print_line("task f4 in thread " + thread_id_str());
        throw std::runtime_error("task failed");
        return 0;
    });

    int v1 = f1.get();
    {
        std::lock_guard lock(cout_mutex);
        std::cout << "f1 = " << v1 << '\n';
    }

    std::string v2 = f2.get();
    {
        std::lock_guard lock(cout_mutex);
        std::cout << "f2 = " << v2 << '\n';
    }

    f3.get();

    try {
        int v4 = f4.get();
        std::lock_guard lock(cout_mutex);
        std::cout << "f4 = " << v4 << '\n';
    } catch (const std::exception& e) {
        std::lock_guard lock(cout_mutex);
        std::cout << "caught exception: " << e.what() << '\n';
    }

    std::vector<sc::Future<int>> futures;
    futures.reserve(10);

    for (int i = 0; i < 10; ++i) {
        futures.push_back(pool.submit([i, &print_line, &thread_id_str] {
            print_line("square(" + std::to_string(i) + ") in thread " + thread_id_str());
            return i * i;
        }));
    }

    std::vector<int> results;
    results.reserve(futures.size());
    for (auto& future : futures) {
        results.push_back(future.get());
    }

    {
        std::lock_guard lock(cout_mutex);
        for (int x : results) {
            std::cout << x << ' ';
        }
        std::cout << '\n';
    }

    return 0;
}