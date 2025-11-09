#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>

std::vector<std::string> list_files(const std::string &dir);
bool file_exists(const std::string &path);
std::string join_path(const std::string &a, const std::string &b);
void log_info(const std::string &msg);

#endif
