#include "atls_server_u.h"
#include <arpa/inet.h>
#include <cstdio>
#include <mutex>
#include <atomic>

// Forward declaration of the readiness flag defined in host.cpp
extern std::atomic<bool> g_server_ready;

namespace {
std::mutex g_mu;
}

// Signal server ready OCALL
extern "C" void ocall_server_ready() {
    std::lock_guard lk(g_mu);
    g_server_ready = true;
    std::fputs("[HOST] Server signaled ready\n", stderr);
    std::fflush(stderr);
}
