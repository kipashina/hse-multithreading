#include "mpsc_shm_queue.h"

#include <chrono>
#include <ctime>
#include <format>
#include <iostream>
#include <string>
#include <thread>

namespace {
constexpr std::uint16_t k_text_message = 1;
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
        const int expected_text_messages = (argc > 1) ? to_int(argv[1]) : 10;

        MpscShmQueue::ConsumerNode consumer{k_queue_path, k_capacity};

        std::cout << "[" << now() << "] "
                  << "[CONSUMER] started, waiting for "
                  << expected_text_messages << " TEXT messages\n";

        int received = 0;
        int idle_rounds = 0;

        while (received < expected_text_messages) {
            auto msg = consumer.try_read_only(k_text_message);
            if (!msg.has_value()) {
                ++idle_rounds;

                if (idle_rounds % 20 == 0) {
                    std::cout << "[" << now() << "] "
                              << "[CONSUMER] queue is empty, still waiting...\n";
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }

            idle_rounds = 0;

            std::string text(
                reinterpret_cast<const char*>(msg->payload.data()),
                msg->payload.size());

            std::cout << "[" << now() << "] "
                      << "[CONSUMER] got TEXT  "
                      << "(" << (received + 1) << "/" << expected_text_messages << ")"
                      << ": " << text << '\n';

            ++received;

            std::this_thread::sleep_for(std::chrono::milliseconds(150));
        }

        std::cout << "[" << now() << "] "
                  << "[CONSUMER] done\n";

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "consumer error: " << ex.what() << '\n';
        return 1;
    }
}