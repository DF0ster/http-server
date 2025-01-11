#include "logger.h"
#include <iostream>
#include <mutex>
#include <ctime>

std::mutex log_mutex;

void log(const std::string& message) {
    std::lock_guard<std::mutex> guard(log_mutex);
    std::time_t now = std::time(0);
    std::tm *ltm = std::localtime(&now);
    std::cout << "[" << 1900 + ltm->tm_year << "-"
              << 1 + ltm->tm_mon << "-"
              << ltm->tm_mday << " "
              << ltm->tm_hour << ":"
              << ltm->tm_min << ":"
              << ltm->tm_sec << "] " << message << std::endl;
}
