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
#include <fstream>
#include <csignal>
#include <openssl/ssl.h>
#include <openssl/err.h>

// Configuration Constants
const int PORT = 8080;
const int MAX_THREADS = 10;
const std::string STATIC_DIR = "./static"; // Directory for static files
const std::string CERT_FILE = "server.crt"; // SSL certificate file
const std::string KEY_FILE = "server.key"; // SSL key file

std::unordered_map<std::string, std::string> database;
std::mutex db_mutex;
std::atomic<bool> running(true);

// Function to initialize SSL context
SSL_CTX* init_ssl_context() {
    SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_certificate_file(ctx, CERT_FILE.c_str(), SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, KEY_FILE.c_str(), SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    return ctx;
}

// Function to read file content
std::string read_file(const std::string& file_path) {
    std::ifstream file(file_path);
    if (file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    } else {
        return "<html><body><h1>500 Internal Server Error</h1><p>Could not read file.</p></body></html>";
    }
}

// Function to determine the MIME type based on file extension
std::string get_mime_type(const std::string& file_path) {
    if (file_path.ends_with(".html")) return "text/html";
    if (file_path.ends_with(".css")) return "text/css";
    if (file_path.ends_with(".js")) return "application/javascript";
    if (file_path.ends_with(".png")) return "image/png";
    if (file_path.ends_with(".jpg") || file_path.ends_with(".jpeg")) return "image/jpeg";
    return "text/plain";
}

// Function to log messages with timestamps
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

// Function to handle client requests
void handle_request(SSL* ssl) {
    char buffer[1024] = {0};
    int bytes_read = SSL_read(ssl, buffer, 1024);
    if (bytes_read < 0) {
        log("Error reading from SSL socket");
        SSL_free(ssl);
        return;
    }
    std::string request(buffer, bytes_read);

    log("Received request:\n" + request);

    std::istringstream request_stream(request);
    std::string method, path, protocol;
    request_stream >> method >> path >> protocol;

    std::string response;

    if (method == "GET") {
        if (path == "/") {
            path = "/index.html"; // Default to index.html
        }
        std::string full_path = STATIC_DIR + path;
        std::string mime_type = get_mime_type(full_path);
        std::string file_content = read_file(full_path);

        if (file_content == "<html><body><h1>500 Internal Server Error</h1><p>Could not read file.</p></body></html>") {
            response = "HTTP/1.1 404 Not Found\nContent-Type: text/html\n\n" + read_file("404.html");
        } else {
            response = "HTTP/1.1 200 OK\nContent-Type: " + mime_type + "\n\n" + file_content;
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
                response = "HTTP/1.1 404 Not Found\nContent-Type: text/html\n\n" + read_file("404.html");
            }
        }
    } else if (method == "DELETE") {
        std::lock_guard<std::mutex> guard(db_mutex);
        if (database.find(path) != database.end()) {
            database.erase(path);
            response = "HTTP/1.1 200 OK\n\nResource deleted";
        } else {
            response = "HTTP/1.1 404 Not Found\nContent-Type: text/html\n\n" + read_file("404.html");
        }
    } else {
        response = "HTTP/1.1 400 Bad Request\nContent-Type: text/html\n\n" + read_file("400.html");
    }

    int bytes_sent = SSL_write(ssl, response.c_str(), response.size());
    if (bytes_sent < 0) {
        log("Error sending response via SSL");
    }

    SSL_free(ssl);
}

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    if (signal == SIGINT) {
        log("Received SIGINT, shutting down...");
        running = false;
    }
}

int main() {
    signal(SIGINT, signal_handler); // Register signal handler

    SSL_library_init();
    SSL_CTX *ctx = init_ssl_context();

    int server_fd;
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
        thread_pool.emplace_back([server_fd, &address, &addrlen, ctx]() {
            while (running) {
                int new_socket;
                if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                    if (!running) break;
                    perror("accept");
                    exit(EXIT_FAILURE);
                }

                SSL *ssl = SSL_new(ctx);
                SSL_set_fd(ssl, new_socket);

                if (SSL_accept(ssl) <= 0) {
                    ERR_print_errors_fp(stderr);
                    SSL_free(ssl);
                    close(new_socket);
                    continue;
                }

                handle_request(ssl);
            }
        });
    }

    for (auto& thread : thread_pool) {
        thread.join();
    }

    close(server_fd);
    log("Server shutdown gracefully.");
    SSL_CTX_free(ctx);
    EVP_cleanup();
    return 0;
}
