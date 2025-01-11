### Instructions

1. **Compile the Server**:
Ensure you have OpenSSL installed on your system. Then, compile the project using the following commands:

`mkdir build`

`cd build`

`cmake ..`

`make`

3. **Run the Server**:

`./HttpServer`

5. **HTTPS Setup**:
Use self-signed certificates for HTTPS support. You can create self-signed certificates using OpenSSL:

`openssl req -x509 -nodes -days 365 -newkey rsa:2048 -keyout server.key -out server.crt`

7. **Custom Configuration**:
Modify configurations as needed (e.g., port number, static directory).
