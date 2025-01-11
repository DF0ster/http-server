cmake_minimum_required(VERSION 3.10)

# Project name
project(HttpServer)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/main.cpp
    src/request_handler.cpp
    src/database.cpp
    src/logger.cpp
    src/ssl_utils.cpp
)

# Add executable
add_executable(HttpServer ${SOURCES})

# Link OpenSSL libraries
find_package(OpenSSL REQUIRED)
target_link_libraries(HttpServer OpenSSL::SSL OpenSSL::Crypto)

# Threads library
find_package(Threads REQUIRED)
target_link_libraries(HttpServer Threads::Threads)
