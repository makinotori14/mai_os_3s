// im_server_student.cpp
// Простой сервер чата на FIFO (именованные pipe'ы).
// Идея:
// 1) Есть один общий FIFO команд сервера: /tmp/im_server_cmd.fifo
//    Все клиенты пишут туда команды: CONNECT, SEND, CREATEGROUP, JOINGROUP, ...
// 2) У каждого клиента есть личный FIFO: /tmp/im_client_<login>.fifo
//    Сервер пишет туда сообщения.
// 3) У каждой группы есть FIFO: /tmp/im_group_<group>.fifo
//    Клиенты пишут туда сообщения группы, сервер читает и рассылает всем участникам.

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

// -------------------- Константы путей --------------------
static const char* SERVER_CMD_FIFO = "/tmp/im_server_cmd.fifo";

// -------------------- Глобальный флаг остановки --------------------
static volatile sig_atomic_t g_stop = 0;
static void on_sigint(int) { g_stop = 1; }

// -------------------- Вспомогательные функции логирования --------------------
static void Log(const std::string& msg) {
    std::time_t t = std::time(nullptr);
    char timebuf[64];
    std::strftime(timebuf, sizeof(timebuf), "%F %T", std::localtime(&t));
    std::cerr << "[" << timebuf << "] " << msg << "\n";
}

// -------------------- CreateQueue / DeleteQueue --------------------
static bool CreateQueue(const std::string& path) {
    // mkfifo создаёт FIFO-файл.
    // 0666 => rw-rw-rw- (но umask может урезать)
    if (mkfifo(path.c_str(), 0666) == 0) return true;

    // Если уже существует — это не страшно
    if (errno == EEXIST) return true;

    std::perror(("mkfifo(" + path + ")").c_str());
    return false;
}

static bool DeleteQueue(const std::string& path) {
    // unlink удаляет файл FIFO
    if (unlink(path.c_str()) == 0) return true;
    if (errno == ENOENT) return true; // уже нет
    std::perror(("unlink(" + path + ")").c_str());
    return false;
}

// -------------------- Push (write) --------------------
static bool Push(int fd, const std::string& s) {
    // Пишем полностью всю строку (write может записать не всё за раз)
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

// -------------------- Пути FIFO клиента/группы --------------------
static std::string ClientFifoPath(const std::string& login) {
    return "/tmp/im_client_" + login + ".fifo";
}
static std::string GroupFifoPath(const std::string& group) {
    return "/tmp/im_group_" + group + ".fifo";
}

// -------------------- Простые "студенческие" структуры --------------------
// Вместо unordered_map/unordered_set — обычные vector и поиск в них.

struct Client {
    std::string login;       // логин клиента
    std::string fifoPath;    // путь к личному FIFO
    int fdWrite;             // fd для записи в FIFO клиента (сервер->клиент)
};

struct Group {
    std::string name;              // имя группы
    std::string fifoPath;          // путь к FIFO группы
    int fdRead;                    // сервер читает этот FIFO
    int fdDummyWrite;              // фиктивный writer (чтобы не ловить EOF)
    std::vector<std::string> members; // логины участников
    std::string readBuf;           // буфер для "кусочных" read()
};

// -------------------- Поиск клиента/группы --------------------
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

// -------------------- Отправка сообщения клиенту --------------------
static void SendToClient(std::vector<Client>& clients,
                         const std::string& login,
                         std::string msgLine)
{
    int idx = FindClientIndex(clients, login);
    if (idx < 0) return; // нет такого клиента

    // гарантируем \n
    if (msgLine.empty() || msgLine.back() != '\n') msgLine.push_back('\n');

    Client& c = clients[idx];

    // если fdWrite не открыт — пробуем открыть
    if (c.fdWrite < 0) {
        c.fdWrite = open(c.fifoPath.c_str(), O_WRONLY | O_NONBLOCK);
        if (c.fdWrite < 0) {
            // Клиент, вероятно, ещё не открыл read-end
            return;
        }
    }

    // Пишем
    if (!Push(c.fdWrite, msgLine)) {
        // если сломалось — закрываем и забудем (в следующий раз попробуем заново)
        close(c.fdWrite);
        c.fdWrite = -1;
    }
}

// -------------------- Рассылка в группу --------------------
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

// -------------------- Удаление клиента из всех групп --------------------
static void RemoveClientFromAllGroups(std::vector<Group>& groups, const std::string& login) {
    for (auto& g : groups) {
        std::vector<std::string> newList;
        for (auto& m : g.members) {
            if (m != login) newList.push_back(m);
        }
        g.members = std::move(newList);
    }
}

// -------------------- Обработка команд из командного FIFO --------------------
static void HandleCommand(const std::string& rawLine,
                          std::vector<Client>& clients,
                          std::vector<Group>& groups)
{
    // Убираем \r\n
    std::string line = rawLine;
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
        line.pop_back();
    if (line.empty()) return;

    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    // -------- CONNECT <login> --------
    if (cmd == "CONNECT") {
        std::string login; iss >> login;
        if (login.empty()) return;

        // если уже есть — просто сообщим
        if (FindClientIndex(clients, login) >= 0) {
            SendToClient(clients, login, "SERVER: already connected");
            return;
        }

        Client c;
        c.login = login;
        c.fifoPath = ClientFifoPath(login);
        c.fdWrite = -1;

        // сервер может создать FIFO клиента (если клиента не успел)
        CreateQueue(c.fifoPath);

        clients.push_back(c);

        Log("CONNECT " + login);
        SendToClient(clients, login, "SERVER: connected as '" + login + "'");
        return;
    }

    // -------- DISCONNECT <login> --------
    if (cmd == "DISCONNECT") {
        std::string login; iss >> login;
        if (login.empty()) return;

        int idx = FindClientIndex(clients, login);
        if (idx >= 0) {
            Log("DISCONNECT " + login);
            if (clients[idx].fdWrite >= 0) close(clients[idx].fdWrite);

            // удаляем из vector (простой способ: swap с последним и pop_back)
            clients[idx] = clients.back();
            clients.pop_back();
        }

        RemoveClientFromAllGroups(groups, login);
        return;
    }

    // -------- SEND <from> <to> <text...> --------
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

    // -------- CREATEGROUP <from> <group> --------
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

        // создаём FIFO группы
        if (!CreateQueue(g.fifoPath)) {
            SendToClient(clients, from, "SERVER: cannot create group fifo");
            return;
        }

        // сервер открывает FIFO группы на чтение
        g.fdRead = open(g.fifoPath.c_str(), O_RDONLY | O_NONBLOCK);
        if (g.fdRead < 0) {
            std::perror(("open(" + g.fifoPath + ")").c_str());
            DeleteQueue(g.fifoPath);
            SendToClient(clients, from, "SERVER: cannot open group fifo for read");
            return;
        }

        // dummy writer, чтобы не ловить EOF
        g.fdDummyWrite = open(g.fifoPath.c_str(), O_WRONLY | O_NONBLOCK);
        if (g.fdDummyWrite < 0) g.fdDummyWrite = -1;

        // создатель сразу участник
        g.members.push_back(from);

        groups.push_back(g);

        Log("CREATEGROUP " + group + " by " + from);
        SendToClient(clients, from, "SERVER: group created '" + group + "'");
        return;
    }

    // -------- DELETEGROUP <from> <group> --------
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

    // -------- JOINGROUP <from> <group> --------
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

    // -------- LEAVEGROUP <from> <group> --------
    if (cmd == "LEAVEGROUP") {
        std::string from, group;
        iss >> from >> group;
        if (from.empty() || group.empty()) return;

        int gi = FindGroupIndex(groups, group);
        if (gi < 0) {
            SendToClient(clients, from, "SERVER: group not found");
            return;
        }

        // удаляем из members
        std::vector<std::string> newList;
        for (auto& m : groups[gi].members) {
            if (m != from) newList.push_back(m);
        }
        groups[gi].members = std::move(newList);

        Log("LEAVEGROUP " + from + " <- " + group);
        SendToClient(clients, from, "SERVER: left group '" + group + "'");
        return;
    }

    // неизвестная команда
    Log("UNKNOWN CMD: " + cmd);
}

// -------------------- Чтение сообщений из FIFO группы --------------------
static void HandleGroupReadable(Group& g, std::vector<Client>& clients) {
    char buf[4096];

    while (true) {
        ssize_t n = read(g.fdRead, buf, sizeof(buf));
        if (n > 0) {
            g.readBuf.append(buf, buf + n);

            // режем по строкам
            while (true) {
                size_t pos = g.readBuf.find('\n');
                if (pos == std::string::npos) break;

                std::string oneLine = g.readBuf.substr(0, pos);
                g.readBuf.erase(0, pos + 1);

                if (oneLine.empty()) continue;

                // ожидаем формат: MSG <from> <text...>
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

        if (n == 0) break; // EOF (обычно при отсутствии writers, если нет dummy_w)
        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
        if (errno == EINTR) continue;

        std::perror("read(group)");
        break;
    }
}

// -------------------- main --------------------
int main() {
    // Игнорируем SIGPIPE, чтобы сервер не падал если write в закрытый FIFO клиента
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, on_sigint);
    signal(SIGTERM, on_sigint);

    Log("Server start...");

    // 1) CreateQueue для командного FIFO
    if (!CreateQueue(SERVER_CMD_FIFO)) return 1;

    // 2) открываем FIFO команд на чтение
    int fd_cmd_r = open(SERVER_CMD_FIFO, O_RDONLY | O_NONBLOCK);
    if (fd_cmd_r < 0) {
        std::perror("open(cmd_r)");
        return 1;
    }

    // 3) dummy writer, чтобы не ловить EOF
    int fd_cmd_dummy_w = open(SERVER_CMD_FIFO, O_WRONLY | O_NONBLOCK);
    if (fd_cmd_dummy_w < 0) fd_cmd_dummy_w = -1;

    std::vector<Client> clients;
    std::vector<Group> groups;

    std::string cmdBuf; // буфер для командного FIFO

    while (!g_stop) {
        // pollfd: первый — cmd FIFO, остальные — group FIFO
        std::vector<pollfd> fds;
        fds.push_back(pollfd{fd_cmd_r, POLLIN, 0});

        // Чтобы знать, какой pollfd соответствует какой группе:
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

        // 1) читаем команды
        if (fds[0].revents & POLLIN) {
            char buf[4096];
            while (true) {
                ssize_t n = read(fd_cmd_r, buf, sizeof(buf));
                if (n > 0) {
                    cmdBuf.append(buf, buf + n);

                    // выделяем строки команд
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

        // 2) читаем сообщения групп
        for (size_t p = 1; p < fds.size(); ++p) {
            if (!(fds[p].revents & POLLIN)) continue;
            int gi = groupIndexForPoll[p - 1];
            if (gi >= 0 && gi < (int)groups.size()) {
                HandleGroupReadable(groups[gi], clients);
            }
        }
    }

    Log("Server stop... cleaning");

    // cleanup
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
