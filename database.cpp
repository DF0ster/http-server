#include "database.h"

std::unordered_map<std::string, std::string> database;
std::mutex db_mutex;
