#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include <cstdio>
#include <string>
#include <sstream>
#include <iostream>

static const char* SERVER_CMD_FIFO = "/tmp/im_server_cmd.fifo";

static volatile sig_atomic_t g_stop = 0;
static void on_sigint(int) { g_stop = 1; }

static bool CreateQueue(const std::string& path) {
    if (mkfifo(path.c_str(), 0666) == 0) return true;
    if (errno == EEXIST) return true;
    std::perror(("mkfifo(" + path + ")").c_str());
    return false;
}

static bool DeleteQueue(const std::string& path) {
    if (unlink(path.c_str()) == 0) return true;
    if (errno == ENOENT) return true;
    std::perror(("unlink(" + path + ")").c_str());
    return false;
}

static int ConnectToQueue(const std::string& path, int flags) {
    int fd = open(path.c_str(), flags);
    if (fd < 0) std::perror(("open(" + path + ")").c_str());
    return fd;
}

static bool Push(int fd, const std::string& s) {
    const char* p = s.c_str();
    size_t left = s.size();
    while (left > 0) {
        ssize_t n = write(fd, p, left);
        if (n > 0) { p += n; left -= (size_t)n; continue; }
        if (n < 0 && errno == EINTR) continue;
        return false;
    }
    return true;
}

static std::string ClientFifoPath(const std::string& login) {
    return "/tmp/im_client_" + login + ".fifo";
}

static std::string GroupFifoPath(const std::string& group) {
    return "/tmp/im_group_" + group + ".fifo";
}

static void PrintHelp() {
    std::cout
        << "Commands:\n"
        << "  /msg <login> <text>\n"
        << "  /create_group <name>\n"
        << "  /delete_group <name>\n"
        << "  /join <name>\n"
        << "  /leave <name>\n"
        << "  /g <name> <text>\n"
        << "  /quit\n"
        << "  /help\n";
}

int main(int argc, char** argv) {
    signal(SIGINT, on_sigint);
    signal(SIGTERM, on_sigint);

    std::string login = (argc >= 2) ? argv[1] : "";
    if (login.empty()) {
        std::cout << "Enter login: ";
        std::getline(std::cin, login);
    }
    if (login.empty()) {
        std::cerr << "Login empty\n";
        return 1;
    }

    std::string myFifo = ClientFifoPath(login);
    DeleteQueue(myFifo);          

    if (!CreateQueue(myFifo)) return 1;

    int fdIn = ConnectToQueue(myFifo, O_RDONLY | O_NONBLOCK);
    if (fdIn < 0) return 1;

    int fdDummyW = open(myFifo.c_str(), O_WRONLY | O_NONBLOCK);
    if (fdDummyW < 0) fdDummyW = -1;

    int fdCmd = open(SERVER_CMD_FIFO, O_WRONLY | O_NONBLOCK);
    if (fdCmd < 0) {
        std::cerr << "Server not running?\n";
        std::perror(("open(" + std::string(SERVER_CMD_FIFO) + ")").c_str());
        close(fdIn);
        if (fdDummyW >= 0) close(fdDummyW);
        DeleteQueue(myFifo);
        return 1;
    }

    Push(fdCmd, "CONNECT " + login + "\n");

    std::cout << "Connected as '" << login << "'. Type /help\n";

    std::string readBuf; 

    while (!g_stop) {
        pollfd fds[2]{};
        fds[0].fd = 0;        

        fds[0].events = POLLIN;
        fds[1].fd = fdIn;     

        fds[1].events = POLLIN;

        int rc = poll(fds, 2, 250);
        if (rc < 0) {
            if (errno == EINTR) continue;
            std::perror("poll");
            break;
        }

        if (fds[1].revents & POLLIN) {
            char buf[4096];
            while (true) {
                ssize_t n = read(fdIn, buf, sizeof(buf));
                if (n > 0) {
                    readBuf.append(buf, buf + n);

                    while (true) {
                        size_t pos = readBuf.find('\n');
                        if (pos == std::string::npos) break;
                        std::string line = readBuf.substr(0, pos + 1);
                        readBuf.erase(0, pos + 1);
                        std::cout << line;
                        std::cout.flush();
                    }
                    continue;
                }
                if (n == 0) break;
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                if (errno == EINTR) continue;
                std::perror("read(in)");
                break;
            }
        }

        if (fds[0].revents & POLLIN) {
            std::string line;
            if (!std::getline(std::cin, line)) { g_stop = 1; break; }
            if (line.empty()) continue;

            if (line == "/help") { PrintHelp(); continue; }
            if (line == "/quit") { Push(fdCmd, "DISCONNECT " + login + "\n"); break; }

            if (line.rfind("/msg ", 0) == 0) {
                std::istringstream iss(line);
                std::string cmd, to;
                iss >> cmd >> to;
                std::string text;
                std::getline(iss, text);
                if (!text.empty() && text[0] == ' ') text.erase(0, 1);

                if (to.empty() || text.empty()) {
                    std::cout << "Usage: /msg <login> <text>\n";
                    continue;
                }
                Push(fdCmd, "SEND " + login + " " + to + " " + text + "\n");
                continue;
            }

            if (line.rfind("/create_group ", 0) == 0) {
                std::istringstream iss(line);
                std::string cmd, g;
                iss >> cmd >> g;
                if (g.empty()) { std::cout << "Usage: /create_group <name>\n"; continue; }
                Push(fdCmd, "CREATEGROUP " + login + " " + g + "\n");
                continue;
            }

            if (line.rfind("/delete_group ", 0) == 0) {
                std::istringstream iss(line);
                std::string cmd, g;
                iss >> cmd >> g;
                if (g.empty()) { std::cout << "Usage: /delete_group <name>\n"; continue; }
                Push(fdCmd, "DELETEGROUP " + login + " " + g + "\n");
                continue;
            }

            if (line.rfind("/join ", 0) == 0) {
                std::istringstream iss(line);
                std::string cmd, g;
                iss >> cmd >> g;
                if (g.empty()) { std::cout << "Usage: /join <name>\n"; continue; }
                Push(fdCmd, "JOINGROUP " + login + " " + g + "\n");
                continue;
            }

            if (line.rfind("/leave ", 0) == 0) {
                std::istringstream iss(line);
                std::string cmd, g;
                iss >> cmd >> g;
                if (g.empty()) { std::cout << "Usage: /leave <name>\n"; continue; }
                Push(fdCmd, "LEAVEGROUP " + login + " " + g + "\n");
                continue;
            }

            if (line.rfind("/g ", 0) == 0) {
                std::istringstream iss(line);
                std::string cmd, g;
                iss >> cmd >> g;
                std::string text;
                std::getline(iss, text);
                if (!text.empty() && text[0] == ' ') text.erase(0, 1);

                if (g.empty() || text.empty()) {
                    std::cout << "Usage: /g <group> <text>\n";
                    continue;
                }

                std::string gfifo = GroupFifoPath(g);
                int fdG = open(gfifo.c_str(), O_WRONLY | O_NONBLOCK);
                if (fdG < 0) {
                    std::cout << "Cannot open group FIFO: " << gfifo << "\n";
                    std::perror(("open(" + gfifo + ")").c_str());
                    continue;
                }

                Push(fdG, "MSG " + login + " " + text + "\n");
                close(fdG);
                continue;
            }

            std::cout << "Unknown command. Type /help\n";
        }
    }

    Push(fdCmd, "DISCONNECT " + login + "\n");
    close(fdCmd);
    close(fdIn);
    if (fdDummyW >= 0) close(fdDummyW);
    DeleteQueue(myFifo);

    std::cout << "Bye\n";
    return 0;
}

