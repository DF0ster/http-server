#include <iostream>
#include <string>
#include <unordered_map>
#include <sstream>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <netinet/in.h>
#include <unistd.h>

const int PORT = 8080;

std::unordered_map<std::string, std::string> database;
std::mutex db_mutex;

void handle_request(int new_socket) {
    char buffer[1024] = {0};
    read(new_socket, buffer, 1024);
    std::string request(buffer);
    
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
    } else if (method == "POST") {
        std::lock_guard<std::mutex> guard(db_mutex);
        std::string body;
        std::getline(request_stream, body); // To consume the blank line
        std::getline(request_stream, body);
        database[path] = body;
        response = "HTTP/1.1 201 Created\n\nResource created";
    } else if (method == "PUT") {
        std::lock_guard<std::mutex> guard(db_mutex);
        std::string body;
        std::getline(request_stream, body); // To consume the blank line
        std::getline(request_stream, body);
        if (database.find(path) != database.end()) {
            database[path] = body;
            response = "HTTP/1.1 200 OK\n\nResource updated";
        } else {
            response = "HTTP/1.1 404 Not Found\n\nResource not found";
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

    send(new_socket, response.c_str(), response.size(), 0);
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

    while (true) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        std::thread(handle_request, new_socket).detach();
    }

    return 0;
}
