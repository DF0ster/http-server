#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <unordered_map>
#include <mutex>

extern std::unordered_map<std::string, std::string> database;
extern std::mutex db_mutex;

#endif // DATABASE_H
