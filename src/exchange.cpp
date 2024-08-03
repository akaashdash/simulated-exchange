#include "exchange.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <unistd.h>
#include <thread>

Exchange::Exchange(std::string name, int port) 
    : name_{name}
    , port_{port} {

}

Exchange::~Exchange() {
    Stop();
}

void Exchange::Start() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) throw std::runtime_error("Socket creation failed");

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_);

    if (bind(server_fd, (struct sockaddr*) &server_addr, sizeof(server_addr))  == -1) {
        close(server_fd);
        throw std::runtime_error("Socket binding failed");
    }

    if (listen(server_fd, 5)  == -1) {
        close(server_fd);
        throw std::runtime_error("Socket listening failed");
    }

    std::cout << "Exchange started on port " << port_ << std::endl;

    while (true) {
        int client_fd = accept(server_fd, (struct sockaddr*)nullptr, nullptr);
        if (client_fd != -1) std::thread(HandleClient, client_fd).detach();
    }

    // TODO: close all clients
    close(server_fd);
}

void Exchange::Stop() {
    // TODO: close all clients
    // TODO: close server
}

void Exchange::HandleClient(int client_fd) {
    char buffer[BUFFER_SIZE] = {0};
    recv(client_fd, buffer, BUFFER_SIZE, 0);

    // handshake

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        if (recv(client_fd, buffer, BUFFER_SIZE, 0) <= 0) break;

        // handle message
    }
}