#include "apply_function.hpp"

#include <iostream>
#include <string>
#include <vector>

template <typename T>
void print_vector(const std::vector<T>& data, const std::string& title) {
  std::cout << title << ": ";
  for (const auto& value : data) {
    std::cout << value << " ";
  }
  std::cout << std::endl;
}

void func_x10(int& value) {
  value *= 10;
}

int main() {
  std::vector<int> data{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

  print_vector(data, "Before");

  ApplyFunction<int>(data, func_x10, 3);

  print_vector(data, "After");

  return 0;
}