#include "mpsc_shm_queue.h"

#include <cstdint>
#include <iostream>

namespace {
constexpr std::size_t k_capacity = 1 << 16;
constexpr const char* k_queue_path = "/demo_mpsc_queue";
}

int main() {
    try {
        MpscShmQueue::ProducerNode init_node{k_queue_path, k_capacity};

        std::cout << "[INIT] queue created and initialized\n";
        std::cout << "[INIT] path = " << k_queue_path << '\n';
        std::cout << "[INIT] capacity = " << k_capacity << " bytes\n";

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "init_queue error: " << ex.what() << '\n';
        return 1;
    }
}