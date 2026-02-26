#include "sender.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

Sender::Sender(const std::string& server_ip, int port)
    : server_ip_(server_ip), port_(port), sock_(-1) {}

Sender::~Sender() {
    disconnect();
}

bool Sender::connect_to_server() {
    sock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_ < 0) {
        perror("socket");
        return false;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_); //Converts the port number to big endian (for consistency)
    if (inet_pton(AF_INET, server_ip_.c_str(), &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        return false;
    }

    if (connect(sock_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        return false;
    }

    return true;
}

bool Sender::send_alert(const std::string& alert_json) {
    if (sock_ < 0) {
        return false;
    }

    std::string message = alert_json + "\n";
    ssize_t sent = send(sock_, message.c_str(), message.size(), 0);
    if (sent < 0) {
        perror("send");
        return false;
    }
    return true;
}

void Sender::disconnect() {
    if (sock_ >= 0) {
        close(sock_);
        sock_ = -1;
    }
}