#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

const char *SERVER_IP = "127.0.0.1";
const int PORT = 9000;
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

int main()
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("socket");
        return 1;
    }
    sockaddr_in serv{};
    serv.sin_family = AF_INET;
    serv.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &serv.sin_addr);

    if (connect(sock, (sockaddr *)&serv, sizeof(serv)) < 0)
    {
        perror("connect");
        return 1;
    }

    std::string line;
    if (recv_line(sock, line) <= 0)
    {
        std::cerr << "No welcome\n";
        return 1;
    }
    std::cout << line << std::endl;
    std::string user, pass;
    std::cout << "Username: ";
    std::getline(std::cin, user);
    std::cout << "Password: ";
    std::getline(std::cin, pass);
    std::string auth = "AUTH " + user + " " + pass + "\n";
    send_all(sock, auth.c_str(), auth.size());
    if (recv_line(sock, line) <= 0)
    {
        std::cerr << "Auth error\n";
        return 1;
    }
    if (line != "AUTH_OK")
    {
        std::cerr << "Auth failed\n";
        return 1;
    }
    std::cout << "Authenticated\n";

    while (true)
    {
        std::cout << "> ";
        std::string cmd;
        std::getline(std::cin, cmd);
        if (cmd.empty())
            continue;
        std::string cmdline = cmd + "\n";
        send_all(sock, cmdline.c_str(), cmdline.size());

        if (cmd == "LIST")
        {
            while (true)
            {
                if (recv_line(sock, line) <= 0)
                    break;
                if (line == "END")
                    break;
                std::cout << line << std::endl;
            }
        }
        else if (cmd.rfind("DOWNLOAD ", 0) == 0)
        {
            std::string fname = cmd.substr(9);
            if (recv_line(sock, line) <= 0)
            {
                std::cout << "server error\n";
                break;
            }
            if (line.rfind("SIZE ", 0) != 0)
            {
                std::cout << line << std::endl;
                continue;
            }
            size_t size = std::stoul(line.substr(5));
            std::string outpath = "downloads/" + fname;
            std::ofstream ofs(outpath, std::ios::binary);
            size_t received = 0;
            char buf[8192];
            while (received < size)
            {
                ssize_t r = recv(sock, buf, std::min(sizeof(buf), size - received), 0);
                if (r <= 0)
                    break;
                ofs.write(buf, r);
                received += r;
            }
            ofs.close();
            std::cout << "Downloaded " << fname << " (" << received << " bytes)\n";
        }
        else if (cmd.rfind("UPLOAD ", 0) == 0)
        {
            std::string fname = cmd.substr(7);

            std::ifstream ifs(fname, std::ios::binary);

            if (!ifs)
            {
                std::cout << "Local file not found: " << fname << std::endl;
                continue;
            }

            ifs.seekg(0, std::ios::end);
            size_t size = ifs.tellg();
            ifs.seekg(0);

            std::string header = "SIZE " + std::to_string(size) + "\n";
            send_all(sock, header.c_str(), header.size());

            char buf[8192];
            while (ifs)
            {
                ifs.read(buf, sizeof(buf));
                std::streamsize s = ifs.gcount();
                if (s > 0)
                    send_all(sock, buf, s);
            }
            ifs.close();

            if (recv_line(sock, line) <= 0)
                break;
            std::cout << line << std::endl;
        }
        else if (cmd == "EXIT")
        {
            break;
        }
        else
        {
            if (recv_line(sock, line) > 0)
                std::cout << line << std::endl;
        }
    }

    close(sock);
    return 0;
}
