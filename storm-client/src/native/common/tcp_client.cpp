// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#include "tcp_client.h"
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

namespace oe_common {

TcpClient::TcpClient(const std::string& host, uint16_t port)
    : sock_fd_(-1), host_(host), port_(port) {
}

TcpClient::~TcpClient() {
    disconnect();
}

bool TcpClient::connect() {
    if (is_connected()) {
        return true; // Already connected
    }

    sock_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd_ < 0) {
        return false;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_);

    if (inet_pton(AF_INET, host_.c_str(), &server_addr.sin_addr) <= 0) {
        close(sock_fd_);
        sock_fd_ = -1;
        return false;
    }

    if (::connect(sock_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(sock_fd_);
        sock_fd_ = -1;
        return false;
    }

    return true;
}

ssize_t TcpClient::send(const void* data, size_t length) {
    if (!is_connected()) {
        return -1;
    }

    return ::send(sock_fd_, data, length, 0);
}

ssize_t TcpClient::receive(void* buffer, size_t max_length) {
    if (!is_connected()) {
        return -1;
    }

    return ::recv(sock_fd_, buffer, max_length, 0);
}

void TcpClient::disconnect() {
    if (sock_fd_ >= 0) {
        close(sock_fd_);
        sock_fd_ = -1;
    }
}

} // namespace oe_common

