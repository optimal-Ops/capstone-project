#include "utils.h"
#include <dirent.h>
#include <sys/stat.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include <chrono>
#include <ctime>

static std::mutex log_mtx;
static const std::string LOG_DIR = "server/logs";

std::vector<std::string> list_files(const std::string &dir)
{
    std::vector<std::string> files;
    DIR *d = opendir(dir.c_str());
    if (!d)
        return files;
    struct dirent *entry;
    while ((entry = readdir(d)) != nullptr)
    {
        if (entry->d_type == DT_REG)
            files.push_back(entry->d_name);
    }
    closedir(d);
    return files;
}

bool file_exists(const std::string &path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

std::string join_path(const std::string &a, const std::string &b)
{
    if (a.empty())
        return b;
    if (a.back() == '/')
        return a + b;
    return a + "/" + b;
}

void log_info(const std::string &msg)
{
    std::lock_guard<std::mutex> lk(log_mtx);
    std::ofstream ofs(LOG_DIR + "/server.log", std::ios::app);
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    ofs << std::ctime(&t) << " : " << msg << std::endl;
}
