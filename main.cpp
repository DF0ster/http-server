#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <netinet/in.h>
#include <unistd.h>
#include <csignal>
#include "ssl_utils.h"
#include "request_handler.h"
#include "logger.h"

// Configuration Constants
const int PORT = 8080;
const int MAX_THREADS = 10;

std::atomic<bool> running(true);

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
