#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <cstdlib>

using fd_t = int;

struct SharedData {
    int number;
    int state;
};

int main(int argc, char *argv[]) {
    if (argc < 2) {
        return EXIT_FAILURE;
    }

    const char *map_filename = argv[1];
    fd_t map_fd = open(map_filename, O_RDWR);
    if (map_fd == -1) {
        perror("open map");
        return EXIT_FAILURE;
    }

    void *addr = mmap(nullptr, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, map_fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap");
        close(map_fd);
        return EXIT_FAILURE;
    }

    SharedData *shared = static_cast<SharedData *>(addr);

    while (true) {
        while (shared->state == 0) {
            usleep(1000);
        }

        if (shared->state == 2) {
            break;
        }

        int x = shared->number;

        if (x < 0) {
            shared->state = 2;
            break;
        }

        if (x <= 1) {
            shared->state = 0;
            continue;
        }

        bool prime = true;
        for (int d = 2; 1ll * d * d <= x; ++d) {
            if (x % d == 0) {
                prime = false;
                break;
            }
        }

        if (prime) {
            shared->state = 2;
            break;
        } else {
            std::cout << x << std::endl;
            shared->state = 0;
        }
    }

    munmap(addr, sizeof(SharedData));
    close(map_fd);
    return EXIT_SUCCESS;
}
