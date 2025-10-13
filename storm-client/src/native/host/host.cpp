#include <cstdint>
#include <cstring>
#include <ctime>
#include <iostream>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <openenclave/bits/result.h>
#include <openenclave/bits/types.h>
#include <openenclave/host.h>
#include <openenclave/trace.h>

#include "atls_server_u.h"

using namespace std;

bool check_simulate_opt(int *argc, const char *argv[]) {
  for (int i = 0; i < *argc; i++) {
    if (strcmp(argv[i], "--simulate") == 0) {
      cout << "Running in simulation mode" << endl;
      memmove(&argv[i], &argv[i + 1], (*argc - i) * sizeof(char *));
      (*argc)--;
      return true;
    }
  }
  return false;
}

int main(int argc, const char *argv[]) {
  oe_result_t result;
  int ret = 0;
  oe_enclave_t *atls_server_enclave = NULL;
  FILE *out_file = NULL;

  // Declare variables at the beginning to avoid goto issues
  time_t now;
  struct tm *timeinfo;
  char timestamp[20];
  char host_log_filename[256];
  char enclave_log_filename[256];
  uint32_t flags;
  const char *enclave_path;

  int retval = 0;
  char port[] = "1234";

  flags = OE_ENCLAVE_FLAG_DEBUG;
  if (check_simulate_opt(&argc, argv)) {
    flags |= OE_ENCLAVE_FLAG_SIMULATE;
  }

  // After check_simulate_opt, argc should be 2: program + enclave path
  if (argc != 2) {
    cerr << "Usage: " << argv[0] << " [--simulate] <enclave_image_path>"
         << endl;
    cerr << "  --simulate    Run in simulation mode" << endl;
    ret = 1;
    goto exit;
  }

  // save path to variable
  enclave_path = argv[1];

  cout << "Host: Host logs will be written to: " << host_log_filename << endl;
  cout << "Host: Enclave logs will be written to: " << enclave_log_filename
       << endl;
  cout << "Host: create enclave for image:" << enclave_path << endl;
  result = oe_create_atls_server_enclave(enclave_path, OE_ENCLAVE_TYPE_SGX,
                                         flags, NULL, 0, &atls_server_enclave);
  if (result != OE_OK) {
    fprintf(stderr, "oe_create_example_enclave(): result=%u (%s)\n", result,
            oe_result_str(result));
    ret = 1;
    goto exit;
  }

  // request the enclave to print hello world
  result = ecall_set_up_tls_server(atls_server_enclave, &retval, port, true);
  if (result != OE_OK) {
    fprintf(stderr, "ecall_set_up_tls_server(): result=%u (%s)\n", result,
            oe_result_str(result));
    ret = 1;
    goto exit;
  }

exit:
  cout << "Host: terminate the enclave" << endl;
  if (atls_server_enclave)
    oe_terminate_enclave(atls_server_enclave);
  if (out_file)
    fclose(out_file);
  return ret;
}
