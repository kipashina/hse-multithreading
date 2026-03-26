cmake_minimum_required(VERSION 3.16)
project(FutexMutexHomework CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(futex_mutex_main main.cpp)
target_link_libraries(futex_mutex_main PRIVATE pthread)