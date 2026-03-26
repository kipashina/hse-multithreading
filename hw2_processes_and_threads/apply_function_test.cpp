#include "apply_function.hpp"

#include <gtest/gtest.h>

#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

// Пустой вектор
TEST(apply_function_test, empty_vector) {
  std::vector<int> data;

  EXPECT_NO_THROW(ApplyFunction<int>(data, [](int& value) { ++value; }, 4));
  EXPECT_TRUE(data.empty());
}

// Один поток
TEST(apply_function_test, applies_function_in_one_thread) {
  std::vector<int> data(10);
  std::iota(data.begin(), data.end(), 0);

  ApplyFunction<int>(data, [](int& value) { value *= 2; }, 1);

  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(data[i], i * 2);
  }
}

// Многопоток
TEST(apply_function_test, applies_function_in_multiple_threads) {
  std::vector<int> data(1000);
  std::iota(data.begin(), data.end(), 1);

  ApplyFunction<int>(data, [](int& value) { value += 5; }, 4);

  for (int i = 0; i < 1000; ++i) {
    EXPECT_EQ(data[i], i + 6);
  }
}

// Число потоков больше числа элементов
TEST(apply_function_test, thread_count_greater_than_data_size) {
  std::vector<int> data{1, 2, 3};

  ApplyFunction<int>(data, [](int& value) { value *= 10; }, 100);

  EXPECT_EQ(data[0], 10);
  EXPECT_EQ(data[1], 20);
  EXPECT_EQ(data[2], 30);
}

// Некорректном числе потоков
TEST(apply_function_test, non_positive_thread_count_uses_one_thread) {
  std::vector<int> data{1, 2, 3, 4};

  ApplyFunction<int>(data, [](int& value) { ++value; }, 0);

  EXPECT_EQ(data[0], 2);
  EXPECT_EQ(data[1], 3);
  EXPECT_EQ(data[2], 4);
  EXPECT_EQ(data[3], 5);
}

// Строки
TEST(apply_function_test, works_with_strings) {
  std::vector<std::string> data{"a", "bb", "ccc"};

  ApplyFunction<std::string>(
      data,
      [](std::string& value) { value += "!"; },
      3);

  EXPECT_EQ(data[0], "a!");
  EXPECT_EQ(data[1], "bb!");
  EXPECT_EQ(data[2], "ccc!");
}

// Исключение в одном потоке
TEST(apply_function_test, exception_is_propagated_in_single_thread_mode) {
  std::vector<int> data{1, 2, 3};

  EXPECT_THROW(
      ApplyFunction<int>(
          data,
          [](int& value) {
            if (value == 2) {
              throw std::runtime_error("error");
            }
            ++value;
          },
          1),
      std::runtime_error);
}

// Исключение в многопоточном режиме
TEST(apply_function_test, exception_is_propagated_in_multi_thread_mode) {
  std::vector<int> data(100);
  std::iota(data.begin(), data.end(), 0);

  EXPECT_THROW(
      ApplyFunction<int>(
          data,
          [](int& value) {
            if (value == 42) {
              throw std::runtime_error("error");
            }
            value *= 2;
          },
          4),
      std::runtime_error);
}