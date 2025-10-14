// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#pragma once

#include <string>
#include <cstdint>
#include <sys/socket.h>
#include <netinet/in.h>

namespace oe_common {

/**
 * Simple TCP client for testing and connectivity verification.
 * Can be used by any host to test connections to the enclave server.
 */
class TcpClient {
public:
    TcpClient(const std::string& host, uint16_t port);
    ~TcpClient();

    // Disable copy
    TcpClient(const TcpClient&) = delete;
    TcpClient& operator=(const TcpClient&) = delete;

    /**
     * Connect to the server.
     * @return true if successful, false otherwise
     */
    bool connect();

    /**
     * Send data to the server.
     * @param data Data buffer to send
     * @param length Length of data to send
     * @return Number of bytes sent, or -1 on error
     */
    ssize_t send(const void* data, size_t length);

    /**
     * Receive data from the server.
     * @param buffer Buffer to receive data into
     * @param max_length Maximum bytes to receive
     * @return Number of bytes received, 0 if connection closed, -1 on error
     */
    ssize_t receive(void* buffer, size_t max_length);

    /**
     * Disconnect from the server.
     */
    void disconnect();

    /**
     * Check if connected.
     * @return true if connected, false otherwise
     */
    bool is_connected() const { return sock_fd_ >= 0; }

private:
    int sock_fd_;
    std::string host_;
    uint16_t port_;
};

} // namespace oe_common

