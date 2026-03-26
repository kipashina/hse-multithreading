#pragma once

#include <algorithm>
#include <exception>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#ifdef APPLY_FUNCTION_DEBUG
inline std::mutex g_log_mutex;

#define af_log(x)                                \
  do {                                           \
    std::lock_guard<std::mutex> lock(g_log_mutex); \
    std::cout << x << std::endl;                 \
  } while (0)
#else
#define af_log(x)
#endif

template <typename T>
void ApplyFunction(std::vector<T>& data,
                   const std::function<void(T&)>& transform,
                   const int thread_count = 1) {
  if (data.empty()) {
    af_log("Empty vector");
    return;
  }

  std::size_t workers = 1;
  if (thread_count > 0) {
    workers = static_cast<std::size_t>(thread_count);
  }

  workers = std::min(workers, data.size());

  af_log("Size: " << data.size());
  af_log("Threads: " << workers);

  if (workers == 1) {
    af_log("Main thread: [0, " << data.size() << ")");
    for (auto& item : data) {
      transform(item);
    }
    af_log("Main thread done");
    return;
  }

  std::vector<std::thread> threads;
  threads.reserve(workers);

  std::exception_ptr first_exception;
  std::mutex exception_mutex;

  const std::size_t base_part_size = data.size() / workers;
  const std::size_t remainder = data.size() % workers;

  auto thread_function = [&](const std::size_t thread_index,
                             const std::size_t begin,
                             const std::size_t end) {
    try {
      af_log("Thread " << thread_index << " started: [" << begin << ", " << end << ")");

      for (std::size_t i = begin; i < end; ++i) {
        transform(data[i]);
      }

      af_log("Thread " << thread_index << " finished");
    } catch (...) {
      std::lock_guard<std::mutex> lock(exception_mutex);
      if (!first_exception) {
        first_exception = std::current_exception();
      }
    }
  };

  std::size_t begin = 0;
  for (std::size_t i = 0; i < workers; ++i) {
    const std::size_t current_part_size = base_part_size + (i < remainder ? 1 : 0);
    const std::size_t end = begin + current_part_size;

    af_log("Thread " << i << ": [" << begin << ", " << end << ")");

    threads.emplace_back(thread_function, i, begin, end);
    begin = end;
  }

  for (auto& thread : threads) {
    thread.join();
  }

  if (first_exception) {
    std::rethrow_exception(first_exception);
  }
}