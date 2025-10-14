// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#include "atls_server_u.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <mutex>

namespace {
std::mutex g_mu;
}

extern "C" void ocall_helloworld() {
  std::lock_guard<std::mutex> lk(g_mu);
  std::fputs("[HOST] hello from ocall_helloworld()\n", stderr);
  std::fflush(stderr);
}

// Socket creation OCALL
int ocall_socket(int domain, int type, int protocol) {
    return socket(domain, type, protocol);
}

// Bind OCALL
int ocall_bind(int sockfd, const void* addr, size_t addrlen) {
    return bind(sockfd, (const struct sockaddr*)addr, (socklen_t)addrlen);
}

// Listen OCALL
int ocall_listen(int sockfd, int backlog) {
    return listen(sockfd, backlog);
}

// Accept OCALL
int ocall_accept(
    int sockfd,
    void* addr,
    size_t addrlen_in,
    size_t* addrlen_out) {

    socklen_t len = (socklen_t)addrlen_in;
    int client_fd = accept(sockfd, (struct sockaddr*)addr, &len);

    if (addrlen_out) {
        *addrlen_out = (size_t)len;
    }

    return client_fd;
}

// Connect OCALL
int ocall_connect(int sockfd, const void* addr, size_t addrlen) {
    return connect(sockfd, (const struct sockaddr*)addr, (socklen_t)addrlen);
}

// Receive OCALL
size_t ocall_recv(int sockfd, void* buf, size_t len, int flags) {
    ssize_t result = recv(sockfd, buf, len, flags);
    if (result < 0) {
        return 0;
    }
    return (size_t)result;
}

// Send OCALL
size_t ocall_send(int sockfd, const void* buf, size_t len, int flags) {
    ssize_t result = send(sockfd, buf, len, flags);
    if (result < 0) {
        return 0;
    }
    return (size_t)result;
}

// Close OCALL
int ocall_close(int fd) {
    return close(fd);
}

// Set socket options OCALL
int ocall_setsockopt(
    int sockfd,
    int level,
    int optname,
    const void* optval,
    size_t optlen) {

    return setsockopt(sockfd, level, optname, optval, (socklen_t)optlen);
}

// Get address info OCALL
int ocall_getaddrinfo(
    const char* node,
    const char* service,
    const void* hints,
    size_t hints_size,
    void* ai,
    size_t ai_size_in,
    size_t* ai_size_out) {

    struct addrinfo* result = nullptr;
    const struct addrinfo* hints_ptr = nullptr;

    if (hints && hints_size >= sizeof(struct addrinfo)) {
        hints_ptr = (const struct addrinfo*)hints;
    }

    int ret = getaddrinfo(node, service, hints_ptr, &result);

    if (ret != 0 || !result) {
        if (ai_size_out) {
            *ai_size_out = 0;
        }
        return ret;
    }

    // Calculate required size
    size_t required_size = sizeof(struct addrinfo);
    if (result->ai_addr) {
        required_size += result->ai_addrlen;
    }
    if (result->ai_canonname) {
        required_size += strlen(result->ai_canonname) + 1;
    }

    if (ai_size_out) {
        *ai_size_out = required_size;
    }

    // Copy to output buffer if space available
    if (ai && ai_size_in >= required_size) {
        memcpy(ai, result, sizeof(struct addrinfo));

        struct addrinfo* ai_out = (struct addrinfo*)ai;
        char* extra = (char*)ai + sizeof(struct addrinfo);

        if (result->ai_addr) {
            memcpy(extra, result->ai_addr, result->ai_addrlen);
            ai_out->ai_addr = (struct sockaddr*)extra;
            extra += result->ai_addrlen;
        }

        if (result->ai_canonname) {
            size_t name_len = strlen(result->ai_canonname) + 1;
            memcpy(extra, result->ai_canonname, name_len);
            ai_out->ai_canonname = extra;
        }

        ai_out->ai_next = nullptr;
    }

    freeaddrinfo(result);
    return ret;
}

// Free address info OCALL
void ocall_freeaddrinfo(void* ai) {
    // Memory is managed by enclave, nothing to do here
    (void)ai;
}
