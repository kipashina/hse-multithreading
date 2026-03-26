cmake_minimum_required(VERSION 3.16)
project(ApplyFunctionHomework CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(GTest REQUIRED)
find_package(benchmark REQUIRED)

enable_testing()

add_executable(apply_function_test
    apply_function_test.cpp
)
target_include_directories(apply_function_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(apply_function_test PRIVATE GTest::gtest_main pthread)

include(GoogleTest)
gtest_discover_tests(apply_function_test)

add_executable(apply_function_benchmark
    apply_function_benchmark.cpp
)
target_include_directories(apply_function_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(apply_function_benchmark PRIVATE benchmark::benchmark benchmark::benchmark_main pthread)

add_executable(apply_function_main
    main.cpp
)
target_include_directories(apply_function_main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(apply_function_main PRIVATE APPLY_FUNCTION_DEBUG)
target_link_libraries(apply_function_main PRIVATE pthread)