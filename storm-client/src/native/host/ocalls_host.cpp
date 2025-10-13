// host/ocalls_host.cpp
#include "example_u.h"
#include <cstdio>
#include <mutex>

namespace {
std::mutex g_mu;
}

extern "C" void ocall_helloworld() {
  std::lock_guard<std::mutex> lk(g_mu);
  std::fputs("[HOST] hello from ocall_helloworld()\n", stderr);
  std::fflush(stderr);
}
