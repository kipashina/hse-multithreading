#include <iostream>
#include <sys/mman.h>

int main() {
    const char* path = "/demo_mpsc_queue";
    if (shm_unlink(path) == 0) {
        std::cout << "Removed shared memory: " << path << '\n';
    } else {
        std::cout << "Shared memory did not exist or was already removed: " << path << '\n';
    }
    return 0;
}