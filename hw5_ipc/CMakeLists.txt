cmake_minimum_required(VERSION 3.16)
project(mpsc_shm_queue_hw LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(producer producer.cpp)
add_executable(consumer consumer.cpp)
add_executable(init_queue init_queue.cpp)
add_executable(reset_queue reset_queue.cpp)

target_include_directories(producer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(consumer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(init_queue PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(producer PRIVATE pthread rt)
target_link_libraries(consumer PRIVATE pthread rt)
target_link_libraries(init_queue PRIVATE pthread rt)
target_link_libraries(reset_queue PRIVATE rt)