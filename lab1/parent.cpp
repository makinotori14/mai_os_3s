#include <iostream>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <cstdlib>

using fd_t = int;

int main() {
    std::string filename;
    std::cin >> filename;

    fd_t filefd = open(filename.c_str(), O_RDONLY);
    if (filefd == -1) {
        perror("open");
        return EXIT_FAILURE;
    }

    fd_t pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        close(filefd);
        return EXIT_FAILURE;
    }

    pid_t pid = fork();

    if (pid == -1) {
        close(filefd);
        close(pipefd[0]);
        close(pipefd[1]);
        perror("fork");
        return EXIT_FAILURE;
    }

    if (pid == 0) { 
        int dupr = dup2(filefd, STDIN_FILENO);
        if (dupr == -1) {
            close(filefd);
            close(pipefd[0]);
            close(pipefd[1]);
            perror("dup2_1");
            _exit(EXIT_FAILURE);
        }
        
        dupr = dup2(pipefd[1], STDOUT_FILENO);
        if (dupr == -1) {
            close(filefd);
            close(pipefd[0]);
            close(pipefd[1]);
            perror("dup2_2");
            _exit(EXIT_FAILURE);
        }

        close(filefd);
        close(pipefd[0]);
        close(pipefd[1]);

        execl("./child", "child", (char*)nullptr);

        perror("child exec failed");
        _exit(EXIT_FAILURE);
    } else {
        close(filefd);
        close(pipefd[1]);

    char buf[1024];
    while (true) {
        ssize_t n = read(pipefd[0], buf, sizeof(buf));
        if (n <= 0) {
            break;
        }
        write(STDOUT_FILENO, buf, n);
    }

        close(pipefd[0]);

        int status = 0;
        pid_t wres = waitpid(pid, &status, 0);
        if (wres == -1) {
            perror("waitpid");
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }
}