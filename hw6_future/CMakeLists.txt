cmake_minimum_required(VERSION 3.16)
project(thread_pool_hw LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(thread_pool_demo
    main.cpp
)

target_include_directories(thread_pool_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(thread_pool_demo PRIVATE pthread)