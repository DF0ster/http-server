#include "request_handler.h"
#include "logger.h"
#include "database.h"
#include "file_utils.h"
#include <regex>

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
        std::string mime_type = get_mime_type(path);
        std::string file_content = read_file(STATIC_DIR + path);

        if (file_content == "<html><body><h1>500 Internal Server Error</h1><p>Could not read file.</p></body></html>") {
            response = "HTTP/1.1 404 Not Found\nContent-Type: text/html\n\n" + read_file(STATIC_DIR + "/404.html");
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
                response = "HTTP/1.1 404 Not Found\nContent-Type: text/html\n\n" + read_file(STATIC_DIR + "/404.html");
            }
        }
    } else if (method == "DELETE") {
        std::lock_guard<std::mutex> guard(db_mutex);
        if (database.find(path) != database.end()) {
            database.erase(path);
            response = "HTTP/1.1 200 OK\n\nResource deleted";
        } else {
            response = "HTTP/1.1 404 Not Found\nContent-Type: text/html\n\n" + read_file(STATIC_DIR + "/404.html");
        }
    } else {
        response = "HTTP/1.1 400 Bad Request\nContent-Type: text/html\n\n" + read_file(STATIC_DIR + "/400.html");
    }

    int bytes_sent = SSL_write(ssl, response.c_str(), response.size());
    if (bytes_sent < 0) {
        log("Error sending response via SSL");
    }

    SSL_free(ssl);
}
