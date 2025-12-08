#include <iostream>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <cstdlib>
#include <cstdio>

using fd_t = int;

struct SharedData {
    int number;
    int state;
};

int main() {
    std::string input_filename;
    std::cin >> input_filename;

    fd_t input_fd = open(input_filename.c_str(), O_RDONLY);
    if (input_fd == -1) {
        perror("open input");
        return EXIT_FAILURE;
    }

    const char *map_filename = "mapping.bin";
    fd_t map_fd = open(map_filename, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (map_fd == -1) {
        perror("open map");
        close(input_fd);
        return EXIT_FAILURE;
    }

    if (ftruncate(map_fd, sizeof(SharedData)) == -1) {
        perror("ftruncate");
        close(input_fd);
        close(map_fd);
        return EXIT_FAILURE;
    }

    void *addr = mmap(nullptr, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, map_fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap");
        close(input_fd);
        close(map_fd);
        return EXIT_FAILURE;
    }

    SharedData *shared = static_cast<SharedData *>(addr);
    shared->number = 0;
    shared->state = 0;

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        munmap(addr, sizeof(SharedData));
        close(input_fd);
        close(map_fd);
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        close(input_fd);
        munmap(addr, sizeof(SharedData));
        close(map_fd);
        execl("./child", "child", map_filename, (char *)nullptr);
        perror("execl");
        _exit(EXIT_FAILURE);
    }

    FILE *input_file = fdopen(input_fd, "r");
    if (!input_file) {
        perror("fdopen");
        munmap(addr, sizeof(SharedData));
        close(input_fd);
        close(map_fd);
        return EXIT_FAILURE;
    }

    int x;
    bool stop = false;

    while (!stop && fscanf(input_file, "%d", &x) == 1) {
        while (shared->state == 1) {
            usleep(1000);
        }

        shared->number = x;
        shared->state = 1;

        while (shared->state == 1) {
            usleep(1000);
        }

        if (shared->state == 2) {
            stop = true;
        }
    }

    if (!stop) {
        while (shared->state == 1) {
            usleep(1000);
        }
        shared->state = 2;
    }

    fclose(input_file);
    munmap(addr, sizeof(SharedData));
    close(map_fd);

    int status = 0;
    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        return EXIT_FAILURE;
    }

    unlink(map_filename);
    return EXIT_SUCCESS;
}
