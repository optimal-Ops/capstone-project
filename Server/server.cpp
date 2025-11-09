#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <pthread.h>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

#include "utils.h"

const int PORT = 9000;
const int BACKLOG = 10;
const std::string FILE_DIR = "files";

std::mutex file_mtx; 
ssize_t recv_line(int sock, std::string &out)
{
    out.clear();
    char c;
    while (true)
    {
        ssize_t r = recv(sock, &c, 1, 0);
        if (r <= 0)
            return r;
        if (c == '\n')
            break;
        out.push_back(c);
    }
    return out.size();
}

bool check_credentials(const std::string &user, const std::string &pass)
{
    std::ifstream ifs("users.txt");
    std::string line;
    while (std::getline(ifs, line))
    {
        if (line.empty())
            continue;
        auto pos = line.find(':');
        if (pos == std::string::npos)
            continue;
        std::string u = line.substr(0, pos);
        std::string p = line.substr(pos + 1);
        if (u == user && p == pass)
            return true;
    }
    return false;
}

void send_all(int sock, const char *buf, size_t len)
{
    size_t total = 0;
    while (total < len)
    {
        ssize_t s = send(sock, buf + total, len - total, 0);
        if (s <= 0)
            break;
        total += s;
    }
}

void handle_list(int client_sock)
{
    auto files = list_files(FILE_DIR);
    std::string resp;
    for (auto &f : files)
        resp += f + "\n";
    resp += "END\n";
    send_all(client_sock, resp.c_str(), resp.size());
}

void handle_download(int client_sock, const std::string &filename)
{
    std::string path = join_path(FILE_DIR, filename);
    if (!file_exists(path))
    {
        std::string msg = "ERR NOFILE\n";
        send_all(client_sock, msg.c_str(), msg.size());
        return;
    }
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs)
    {
        send_all(client_sock, "ERR READ\n", 9);
        return;
    }
    ifs.seekg(0, std::ios::end);
    size_t size = ifs.tellg();
    ifs.seekg(0);
    std::string header = "SIZE " + std::to_string(size) + "\n";
    send_all(client_sock, header.c_str(), header.size());
    char buf[8192];
    while (ifs)
    {
        ifs.read(buf, sizeof(buf));
        std::streamsize s = ifs.gcount();
        if (s > 0)
            send_all(client_sock, buf, s);
    }
    log_info("Sent file: " + filename);
}

void handle_upload(int client_sock, const std::string &filename)
{
    std::string sizeLine;
    if (recv_line(client_sock, sizeLine) <= 0)
        return;
    if (sizeLine.rfind("SIZE ", 0) != 0)
    {
        send_all(client_sock, "ERR SIZE\n", 9);
        return;
    }
    size_t size = std::stoul(sizeLine.substr(5));
    std::string path = join_path(FILE_DIR, filename);
    std::string tmp = path + ".tmp";
    {
        std::lock_guard<std::mutex> lk(file_mtx);
        std::ofstream ofs(tmp, std::ios::binary);
        size_t received = 0;
        char buf[8192];
        while (received < size)
        {
            ssize_t r = recv(client_sock, buf, std::min(sizeof(buf), size - received), 0);
            if (r <= 0)
            {
                ofs.close();
                remove(tmp.c_str());
                return;
            }
            ofs.write(buf, r);
            received += r;
        }
        ofs.close();
        rename(tmp.c_str(), path.c_str());
    }
    send_all(client_sock, "UPLOAD_OK\n", 10);
    log_info("Received file: " + filename);
}

void *client_thread(void *arg)
{
    int client_sock = *((int *)arg);
    delete (int *)arg;

    std::string line;
    send_all(client_sock, "WELCOME\n", 8);
    if (recv_line(client_sock, line) <= 0)
    {
        close(client_sock);
        return nullptr;
    }
    if (line.rfind("AUTH ", 0) != 0)
    {
        send_all(client_sock, "AUTH_FAIL\n", 10);
        close(client_sock);
        return nullptr;
    }
    std::istringstream iss(line);
    std::string cmd, user, pass;
    iss >> cmd >> user >> pass;
    if (!check_credentials(user, pass))
    {
        send_all(client_sock, "AUTH_FAIL\n", 10);
        close(client_sock);
        return nullptr;
    }
    send_all(client_sock, "AUTH_OK\n", 8);
    log_info("User " + user + " authenticated");

    while (true)
    {
        if (recv_line(client_sock, line) <= 0)
            break;
        if (line == "LIST")
        {
            handle_list(client_sock);
        }
        else if (line.rfind("DOWNLOAD ", 0) == 0)
        {
            std::string fname = line.substr(9);
            handle_download(client_sock, fname);
        }
        else if (line.rfind("UPLOAD ", 0) == 0)
        {
            std::string fname = line.substr(7);
            handle_upload(client_sock, fname);
        }
        else if (line == "EXIT")
        {
            break;
        }
        else
        {
            send_all(client_sock, "UNKNOWN\n", 8);
        }
    }

    close(client_sock);
    log_info("Client disconnected");
    return nullptr;
}

int main()
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        return 1;
    }
    if (listen(server_fd, BACKLOG) < 0)
    {
        perror("listen");
        return 1;
    }
    std::cout << "Server listening on port " << PORT << std::endl;
    log_info("Server started");

    while (true)
    {
        sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        int client_sock = accept(server_fd, (sockaddr *)&client_addr, &len);
        if (client_sock < 0)
            continue;
        std::cout << "Client connected\n";
        int *pclient = new int;
        *pclient = client_sock;
        pthread_t tid;
        pthread_create(&tid, nullptr, client_thread, pclient);
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}
