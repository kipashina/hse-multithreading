cmake_minimum_required(VERSION 3.16)
project(coroutine_dfs CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS context coroutine)

add_executable(coroutine_dfs main.cpp)

target_link_libraries(coroutine_dfs
    PRIVATE
    Boost::context
    Boost::coroutine
)