#include "atls_server_u.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <atomic>

// Forward declaration of the readiness flag defined in host.cpp
extern std::atomic<bool> g_server_ready;

namespace {
std::mutex g_mu;
}

extern "C" void ocall_helloworld() {
  std::lock_guard<std::mutex> lk(g_mu);
  std::fputs("[HOST] hello from ocall_helloworld()\n", stderr);
  std::fflush(stderr);
}

// Socket creation OCALL
extern "C" int ocall_socket(const int domain, const int type, const int protocol) {
    return socket(domain, type, protocol);
}

// Bind OCALL
extern "C" int ocall_bind(const int sockfd, const void* addr, const size_t addrlen) {
    return bind(sockfd, static_cast<const sockaddr *>(addr), static_cast<socklen_t>(addrlen));
}

// Listen OCALL
extern "C" int ocall_listen(const int sockfd, const int backlog) {
    return listen(sockfd, backlog);
}

// Accept OCALL
extern "C" int ocall_accept(
    const int sockfd,
    void* addr,
    const size_t addrlen_in,
    size_t* addrlen_out) {

    auto len = static_cast<socklen_t>(addrlen_in);
    const int client_fd = accept(sockfd, static_cast<struct sockaddr *>(addr), &len);

    if (addrlen_out) {
        *addrlen_out = static_cast<size_t>(len);
    }

    return client_fd;
}

// Connect OCALL
extern "C" int ocall_connect(const int sockfd, const void* addr, const size_t addrlen) {
    return connect(sockfd, static_cast<const sockaddr *>(addr), static_cast<socklen_t>(addrlen));
}

// Receive OCALL
extern "C" size_t ocall_recv(const int sockfd, void* buf, const size_t len, const int flags) {
    ssize_t result = recv(sockfd, buf, len, flags);
    if (result < 0) {
        return 0;
    }
    return static_cast<size_t>(result);
}

// Send OCALL
extern "C" size_t ocall_send(const int sockfd, const void* buf, const size_t len, const int flags) {
    ssize_t result = send(sockfd, buf, len, flags);
    if (result < 0) {
        return 0;
    }
    return static_cast<size_t>(result);
}

// Close OCALL
extern "C" int ocall_close(const int fd) {
    return close(fd);
}

// Set socket options OCALL
extern "C" int ocall_setsockopt(
    const int sockfd,
    const int level,
    const int optname,
    const void* optval,
    const size_t optlen) {

    return setsockopt(sockfd, level, optname, optval, static_cast<socklen_t>(optlen));
}

// Get address info OCALL
extern "C" int ocall_getaddrinfo(
    const char* node,
    const char* service,
    const void* hints,
    const size_t hints_size,
    void* ai,
    const size_t ai_size_in,
    size_t* ai_size_out) {
    addrinfo* result = nullptr;
    const addrinfo* hints_ptr = nullptr;

    if (hints && hints_size >= sizeof(addrinfo)) {
        hints_ptr = static_cast<const addrinfo *>(hints);
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
        memcpy(ai, result, sizeof(addrinfo));

        auto* ai_out = static_cast<struct addrinfo *>(ai);
        char* extra = static_cast<char *>(ai) + sizeof(struct addrinfo);

        if (result->ai_addr) {
            memcpy(extra, result->ai_addr, result->ai_addrlen);
            ai_out->ai_addr = reinterpret_cast<sockaddr *>(extra);
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
extern "C" void ocall_freeaddrinfo(void* ai) {
    // Memory is managed by enclave, nothing to do here
    (void)ai;
}

// Signal server ready OCALL
extern "C" void ocall_server_ready() {
    std::lock_guard<std::mutex> lk(g_mu);
    g_server_ready = true;
    std::fputs("[HOST] Server signaled ready\n", stderr);
    std::fflush(stderr);
}
