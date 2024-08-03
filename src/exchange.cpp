#include "exchange.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <unistd.h>
#include <thread>

Exchange::Exchange(int port) 
    : port_{port}
    , running_{false} {

}

Exchange::~Exchange() {
    Stop();
}

void Exchange::Start() {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) throw std::runtime_error("Socket creation failed");

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_);

    if (bind(server_sock, (struct sockaddr*) &server_addr, sizeof(server_addr))  == -1) {
        close(server_sock);
        throw std::runtime_error("Socket binding failed");
    }

    if (listen(server_sock, 5)  == -1) {
        close(server_sock);
        throw std::runtime_error("Socket listening failed");
    }

    std::cout << "Exchange started on port " << port_ << std::endl;
    running_ = true;
    
    while (running_) {
        int client_sock = accept(server_sock, (struct sockaddr*)nullptr, nullptr);
        if (client_sock != -1) std::thread(HandleClient, client_sock).detach();
    }

    close(server_sock);
}

void Exchange::Stop() {
    running_ = false;
}

void Exchange::HandleClient(int client_sock) {
    char buffer[BUFFER_SIZE] = {0};
    recv(client_sock, buffer, BUFFER_SIZE, 0);

    // handshake

    while (running_) {
        memset(buffer, 0, BUFFER_SIZE);
        if (recv(client_sock, buffer, BUFFER_SIZE, 0) <= 0) break;

        // handle message
    }

    close(client_sock);
}