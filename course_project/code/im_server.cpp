#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include <cstdio>
#include <cstring>
#include <ctime>

#include <string>
#include <vector>
#include <iostream>
#include <sstream>

static const char* SERVER_CMD_FIFO = "/tmp/im_server_cmd.fifo";

static volatile sig_atomic_t g_stop = 0;
static void on_sigint(int) { g_stop = 1; }

static void Log(const std::string& msg) {
    std::time_t t = std::time(nullptr);
    char timebuf[64];
    std::strftime(timebuf, sizeof(timebuf), "%F %T", std::localtime(&t));
    std::cerr << "[" << timebuf << "] " << msg << "\n";
}

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

static bool Push(int fd, const std::string& s) {

    const char* p = s.c_str();
    size_t left = s.size();

    while (left > 0) {
        ssize_t n = write(fd, p, left);
        if (n > 0) {
            p += n;
            left -= (size_t)n;
            continue;
        }
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

struct Client {
    std::string login;       

    std::string fifoPath;    

    int fdWrite;             

};

struct Group {
    std::string name;              

    std::string fifoPath;          

    int fdRead;                    

    int fdDummyWrite;              

    std::vector<std::string> members; 

    std::string readBuf;           

};

static int FindClientIndex(const std::vector<Client>& clients, const std::string& login) {
    for (int i = 0; i < (int)clients.size(); ++i) {
        if (clients[i].login == login) return i;
    }
    return -1;
}

static int FindGroupIndex(const std::vector<Group>& groups, const std::string& name) {
    for (int i = 0; i < (int)groups.size(); ++i) {
        if (groups[i].name == name) return i;
    }
    return -1;
}

static bool IsMember(const Group& g, const std::string& login) {
    for (const auto& m : g.members) {
        if (m == login) return true;
    }
    return false;
}

static void SendToClient(std::vector<Client>& clients,
                         const std::string& login,
                         std::string msgLine)
{
    int idx = FindClientIndex(clients, login);
    if (idx < 0) return; 

    if (msgLine.empty() || msgLine.back() != '\n') msgLine.push_back('\n');

    Client& c = clients[idx];

    if (c.fdWrite < 0) {
        c.fdWrite = open(c.fifoPath.c_str(), O_WRONLY | O_NONBLOCK);
        if (c.fdWrite < 0) {

            return;
        }
    }

    if (!Push(c.fdWrite, msgLine)) {

        close(c.fdWrite);
        c.fdWrite = -1;
    }
}

static void BroadcastToGroup(std::vector<Client>& clients,
                             const Group& g,
                             const std::string& from,
                             const std::string& text)
{
    for (const auto& member : g.members) {
        SendToClient(clients, member,
            "[group:" + g.name + "] " + from + ": " + text);
    }
}

static void RemoveClientFromAllGroups(std::vector<Group>& groups, const std::string& login) {
    for (auto& g : groups) {
        std::vector<std::string> newList;
        for (auto& m : g.members) {
            if (m != login) newList.push_back(m);
        }
        g.members = std::move(newList);
    }
}

static void HandleCommand(const std::string& rawLine,
                          std::vector<Client>& clients,
                          std::vector<Group>& groups)
{

    std::string line = rawLine;
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
        line.pop_back();
    if (line.empty()) return;

    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    if (cmd == "CONNECT") {
        std::string login; iss >> login;
        if (login.empty()) return;

        if (FindClientIndex(clients, login) >= 0) {
            SendToClient(clients, login, "SERVER: already connected");
            return;
        }

        Client c;
        c.login = login;
        c.fifoPath = ClientFifoPath(login);
        c.fdWrite = -1;

        CreateQueue(c.fifoPath);

        clients.push_back(c);

        Log("CONNECT " + login);
        SendToClient(clients, login, "SERVER: connected as '" + login + "'");
        return;
    }

    if (cmd == "DISCONNECT") {
        std::string login; iss >> login;
        if (login.empty()) return;

        int idx = FindClientIndex(clients, login);
        if (idx >= 0) {
            Log("DISCONNECT " + login);
            if (clients[idx].fdWrite >= 0) close(clients[idx].fdWrite);

            clients[idx] = clients.back();
            clients.pop_back();
        }

        RemoveClientFromAllGroups(groups, login);
        return;
    }

    if (cmd == "SEND") {
        std::string from, to;
        iss >> from >> to;

        std::string text;
        std::getline(iss, text);
        if (!text.empty() && text[0] == ' ') text.erase(0, 1);

        if (from.empty() || to.empty() || text.empty()) return;

        if (FindClientIndex(clients, to) < 0) {
            SendToClient(clients, from, "SERVER: user '" + to + "' not connected");
            return;
        }

        SendToClient(clients, to, "[pm] " + from + ": " + text);
        SendToClient(clients, from, "SERVER: delivered to '" + to + "'");
        Log("SEND " + from + "->" + to + " '" + text + "'");
        return;
    }

    if (cmd == "CREATEGROUP") {
        std::string from, group;
        iss >> from >> group;
        if (from.empty() || group.empty()) return;

        if (FindGroupIndex(groups, group) >= 0) {
            SendToClient(clients, from, "SERVER: group already exists");
            return;
        }

        Group g;
        g.name = group;
        g.fifoPath = GroupFifoPath(group);
        g.fdRead = -1;
        g.fdDummyWrite = -1;

        if (!CreateQueue(g.fifoPath)) {
            SendToClient(clients, from, "SERVER: cannot create group fifo");
            return;
        }

        g.fdRead = open(g.fifoPath.c_str(), O_RDONLY | O_NONBLOCK);
        if (g.fdRead < 0) {
            std::perror(("open(" + g.fifoPath + ")").c_str());
            DeleteQueue(g.fifoPath);
            SendToClient(clients, from, "SERVER: cannot open group fifo for read");
            return;
        }

        g.fdDummyWrite = open(g.fifoPath.c_str(), O_WRONLY | O_NONBLOCK);
        if (g.fdDummyWrite < 0) g.fdDummyWrite = -1;

        g.members.push_back(from);

        groups.push_back(g);

        Log("CREATEGROUP " + group + " by " + from);
        SendToClient(clients, from, "SERVER: group created '" + group + "'");
        return;
    }

    if (cmd == "DELETEGROUP") {
        std::string from, group;
        iss >> from >> group;
        if (from.empty() || group.empty()) return;

        int gi = FindGroupIndex(groups, group);
        if (gi < 0) {
            SendToClient(clients, from, "SERVER: group not found");
            return;
        }

        Log("DELETEGROUP " + group);

        if (groups[gi].fdRead >= 0) close(groups[gi].fdRead);
        if (groups[gi].fdDummyWrite >= 0) close(groups[gi].fdDummyWrite);
        DeleteQueue(groups[gi].fifoPath);

        groups[gi] = groups.back();
        groups.pop_back();

        SendToClient(clients, from, "SERVER: group deleted '" + group + "'");
        return;
    }

    if (cmd == "JOINGROUP") {
        std::string from, group;
        iss >> from >> group;
        if (from.empty() || group.empty()) return;

        int gi = FindGroupIndex(groups, group);
        if (gi < 0) {
            SendToClient(clients, from, "SERVER: group not found");
            return;
        }

        if (!IsMember(groups[gi], from)) {
            groups[gi].members.push_back(from);
        }

        Log("JOINGROUP " + from + " -> " + group);
        SendToClient(clients, from, "SERVER: joined group '" + group + "'");
        return;
    }

    if (cmd == "LEAVEGROUP") {
        std::string from, group;
        iss >> from >> group;
        if (from.empty() || group.empty()) return;

        int gi = FindGroupIndex(groups, group);
        if (gi < 0) {
            SendToClient(clients, from, "SERVER: group not found");
            return;
        }

        std::vector<std::string> newList;
        for (auto& m : groups[gi].members) {
            if (m != from) newList.push_back(m);
        }
        groups[gi].members = std::move(newList);

        Log("LEAVEGROUP " + from + " <- " + group);
        SendToClient(clients, from, "SERVER: left group '" + group + "'");
        return;
    }

    Log("UNKNOWN CMD: " + cmd);
}

static void HandleGroupReadable(Group& g, std::vector<Client>& clients) {
    char buf[4096];

    while (true) {
        ssize_t n = read(g.fdRead, buf, sizeof(buf));
        if (n > 0) {
            g.readBuf.append(buf, buf + n);

            while (true) {
                size_t pos = g.readBuf.find('\n');
                if (pos == std::string::npos) break;

                std::string oneLine = g.readBuf.substr(0, pos);
                g.readBuf.erase(0, pos + 1);

                if (oneLine.empty()) continue;

                std::istringstream iss(oneLine);
                std::string tag, from;
                iss >> tag >> from;
                std::string text;
                std::getline(iss, text);
                if (!text.empty() && text[0] == ' ') text.erase(0, 1);

                if (tag == "MSG" && !from.empty() && !text.empty()) {
                    Log("GROUPMSG [" + g.name + "] " + from + ": " + text);
                    BroadcastToGroup(clients, g, from, text);
                }
            }
            continue;
        }

        if (n == 0) break; 

        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
        if (errno == EINTR) continue;

        std::perror("read(group)");
        break;
    }
}

int main() {

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, on_sigint);
    signal(SIGTERM, on_sigint);

    Log("Server start...");

    if (!CreateQueue(SERVER_CMD_FIFO)) return 1;

    int fd_cmd_r = open(SERVER_CMD_FIFO, O_RDONLY | O_NONBLOCK);
    if (fd_cmd_r < 0) {
        std::perror("open(cmd_r)");
        return 1;
    }

    int fd_cmd_dummy_w = open(SERVER_CMD_FIFO, O_WRONLY | O_NONBLOCK);
    if (fd_cmd_dummy_w < 0) fd_cmd_dummy_w = -1;

    std::vector<Client> clients;
    std::vector<Group> groups;

    std::string cmdBuf; 

    while (!g_stop) {

        std::vector<pollfd> fds;
        fds.push_back(pollfd{fd_cmd_r, POLLIN, 0});

        std::vector<int> groupIndexForPoll;
        for (int i = 0; i < (int)groups.size(); ++i) {
            if (groups[i].fdRead < 0) continue;
            fds.push_back(pollfd{groups[i].fdRead, POLLIN, 0});
            groupIndexForPoll.push_back(i);
        }

        int rc = poll(fds.data(), fds.size(), 500);
        if (rc < 0) {
            if (errno == EINTR) continue;
            std::perror("poll");
            break;
        }
        if (rc == 0) continue;

        if (fds[0].revents & POLLIN) {
            char buf[4096];
            while (true) {
                ssize_t n = read(fd_cmd_r, buf, sizeof(buf));
                if (n > 0) {
                    cmdBuf.append(buf, buf + n);

                    while (true) {
                        size_t pos = cmdBuf.find('\n');
                        if (pos == std::string::npos) break;
                        std::string oneCmd = cmdBuf.substr(0, pos + 1);
                        cmdBuf.erase(0, pos + 1);
                        HandleCommand(oneCmd, clients, groups);
                    }
                    continue;
                }
                if (n == 0) break;
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                if (errno == EINTR) continue;
                std::perror("read(cmd)");
                break;
            }
        }

        for (size_t p = 1; p < fds.size(); ++p) {
            if (!(fds[p].revents & POLLIN)) continue;
            int gi = groupIndexForPoll[p - 1];
            if (gi >= 0 && gi < (int)groups.size()) {
                HandleGroupReadable(groups[gi], clients);
            }
        }
    }

    Log("Server stop... cleaning");

    for (auto& c : clients) {
        if (c.fdWrite >= 0) close(c.fdWrite);
    }
    for (auto& g : groups) {
        if (g.fdRead >= 0) close(g.fdRead);
        if (g.fdDummyWrite >= 0) close(g.fdDummyWrite);
        DeleteQueue(g.fifoPath);
    }

    close(fd_cmd_r);
    if (fd_cmd_dummy_w >= 0) close(fd_cmd_dummy_w);
    DeleteQueue(SERVER_CMD_FIFO);
    return 0;
}

