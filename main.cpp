#include <iostream>
#include <string>
#include <unordered_map>
#include <sstream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <netinet/in.h>
#include <unistd.h>
#include <regex>
#include <ctime>

// Configuration Constants
const int PORT = 8080;
const int MAX_THREADS = 10;

std::unordered_map<std::string, std::string> database;
std::mutex db_mutex;

void log(const std::string& message) {
    std::lock_guard<std::mutex> guard(db_mutex);
    std::time_t now = std::time(0);
    std::tm *ltm = std::localtime(&now);
    std::cout << "[" << 1900 + ltm->tm_year << "-"
              << 1 + ltm->tm_mon << "-"
              << ltm->tm_mday << " "
              << ltm->tm_hour << ":"
              << ltm->tm_min << ":"
              << ltm->tm_sec << "] " << message << std::endl;
}

void handle_request(int new_socket) {
    char buffer[1024] = {0};
    int bytes_read = read(new_socket, buffer, 1024);
    if (bytes_read < 0) {
        log("Error reading from socket");
        close(new_socket);
        return;
    }
    std::string request(buffer, bytes_read);

    log("Received request:\n" + request);

    std::istringstream request_stream(request);
    std::string method, path, protocol;
    request_stream >> method >> path >> protocol;

    std::string response;

    if (method == "GET") {
        std::lock_guard<std::mutex> guard(db_mutex);
        if (database.find(path) != database.end()) {
            response = "HTTP/1.1 200 OK\n\n" + database[path];
        } else {
            response = "HTTP/1.1 404 Not Found\n\nResource not found";
        }
    } else if (method == "POST" || method == "PUT") {
        std::lock_guard<std::mutex> guard(db_mutex);
        std::string body;
        if (std::regex_search(request, std::regex("\r\n\r\n"))) {
            body = request.substr(request.find("\r\n\r\n") + 4);
        }
        if (method == "POST") {
            database[path] = body;
            response = "HTTP/1.1 201 Created\n\nResource created";
        } else {
            if (database.find(path) != database.end()) {
                database[path] = body;
                response = "HTTP/1.1 200 OK\n\nResource updated";
            } else {
                response = "HTTP/1.1 404 Not Found\n\nResource not found";
            }
        }
    } else if (method == "DELETE") {
        std::lock_guard<std::mutex> guard(db_mutex);
        if (database.find(path) != database.end()) {
            database.erase(path);
            response = "HTTP/1.1 200 OK\n\nResource deleted";
        } else {
            response = "HTTP/1.1 404 Not Found\n\nResource not found";
        }
    } else {
        response = "HTTP/1.1 400 Bad Request\n\nInvalid method";
    }

    int bytes_sent = send(new_socket, response.c_str(), response.size(), 0);
    if (bytes_sent < 0) {
        log("Error sending response");
    }

    close(new_socket);
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    log("Server started on port " + std::to_string(PORT));

    std::vector<std::thread> thread_pool;
    for (int i = 0; i < MAX_THREADS; ++i) {
        thread_pool.emplace_back([server_fd, &address, &addrlen]() {
            while (true) {
                int new_socket;
                if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                    perror("accept");
                    exit(EXIT_FAILURE);
                }
                handle_request(new_socket);
            }
        });
    }

    for (auto& thread : thread_pool) {
        thread.join();
    }

    return 0;
}
