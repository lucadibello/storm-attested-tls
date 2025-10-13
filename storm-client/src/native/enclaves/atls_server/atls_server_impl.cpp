// Placeholder: here, the enclave will implement all the ECALL functions defined
// in `atls_server.edl`!

#include <atls_server_t.h>
#include <cstdio>

int ecall_set_up_tls_server(char *port, bool keep_server_up) {
  printf("Hello from the enclave");
  return 1;
}
