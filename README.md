## Instructions

### Compile the Server:

Ensure you have OpenSSL installed on your system. Then, compile the project using the following command:

`g++ -o server main.cpp request_handler.cpp database.cpp logger.cpp ssl_utils.cpp -lssl -lcrypto -pthread`

### Run the Server:

`./server`

### HTTPS Setup:

Use self-signed certificates for HTTPS support. You can create self-signed certificates using OpenSSL:

`openssl req -x509 -nodes -days 365 -newkey rsa:2048 -keyout server.key -out server.crt`

### Custom Configuration

Modify configurations as needed (e.g., port number, static directory).
