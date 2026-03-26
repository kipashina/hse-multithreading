#include "mpsc_shm_queue.h"

#include <chrono>
#include <ctime>
#include <format>
#include <iostream>
#include <string>
#include <thread>

namespace {
constexpr std::uint16_t k_text_message = 1;
constexpr std::uint16_t k_number_message = 2;
constexpr std::size_t k_capacity = 1 << 16;
constexpr const char* k_queue_path = "/demo_mpsc_queue";

int to_int(const char* s) {
    return std::stoi(s);
}

std::string now() {
    using namespace std::chrono;
    const auto now_time = system_clock::now();
    const auto time_value = system_clock::to_time_t(now_time);
    const auto ms = duration_cast<milliseconds>(now_time.time_since_epoch()) % 1000;

    std::tm local_tm{};
    localtime_r(&time_value, &local_tm);

    return std::format("{:02}:{:02}:{:02}.{:03}",
                       local_tm.tm_hour,
                       local_tm.tm_min,
                       local_tm.tm_sec,
                       static_cast<int>(ms.count()));
}
} // namespace

int main(int argc, char** argv) {
    try {
        const int producer_id = (argc > 1) ? to_int(argv[1]) : 1;
        const int count = (argc > 2) ? to_int(argv[2]) : 5;

        const auto delay =
            (producer_id % 2 == 0) ? std::chrono::milliseconds(500)
                                   : std::chrono::milliseconds(300);

        MpscShmQueue::ProducerNode producer{k_queue_path, k_capacity};

        std::cout << "[" << now() << "] "
                  << "[PRODUCER " << producer_id << "] started, delay = "
                  << delay.count() << " ms\n";

        for (int i = 0; i < count; ++i) {
            {
                std::string text =
                    std::format("producer {} -> text message {}", producer_id, i);

                while (!producer.send_string(k_text_message, text)) {
                    std::cout << "[" << now() << "] "
                              << "[PRODUCER " << producer_id
                              << "] queue is full, waiting to send text...\n";
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }

                std::cout << "[" << now() << "] "
                          << "[PRODUCER " << producer_id
                          << "] sent TEXT   #" << i
                          << ": " << text << '\n';
            }

            {
                int value = producer_id * 100000 + i;

                while (!producer.send_pod(k_number_message, value)) {
                    std::cout << "[" << now() << "] "
                              << "[PRODUCER " << producer_id
                              << "] queue is full, waiting to send number...\n";
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }

                std::cout << "[" << now() << "] "
                          << "[PRODUCER " << producer_id
                          << "] sent NUMBER #" << i
                          << ": " << value << '\n';
            }

            std::this_thread::sleep_for(delay);
        }

        std::cout << "[" << now() << "] "
                  << "[PRODUCER " << producer_id << "] finished\n";

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "producer error: " << ex.what() << '\n';
        return 1;
    }
}