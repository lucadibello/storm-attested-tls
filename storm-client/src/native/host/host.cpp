#include <cstdint>
#include <cstring>
#include <string>

#include <openenclave/bits/result.h>
#include <openenclave/bits/types.h>
#include <openenclave/host.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "atls_server_u.h"

bool check_simulate_opt(int *argc, const char *argv[]) {
  for (int i = 0; i < *argc; i++) {
    if (strcmp(argv[i], "--simulate") == 0) {
      spdlog::info("Running in simulation mode");
      memmove(&argv[i], &argv[i + 1], (*argc - i) * sizeof(char *));
      (*argc)--;
      return true;
    }
  }
  return false;
}

std::string parse_port_argument(const char* arg) {
  const char* port_prefix = "-port:";
  size_t prefix_len = strlen(port_prefix);

  if (strncmp(arg, port_prefix, prefix_len) == 0) {
    return arg + prefix_len;
  }

  return "";
}

void print_usage(const char* program_name) {
  spdlog::error("Usage: {} [--simulate] <enclave_image_path> -port:<port>", program_name);
  spdlog::error("  --simulate    Run in simulation mode");
  spdlog::error("  -port:<port>  Port number for the TLS server (e.g., -port:1234)");
}

int main(int argc, const char *argv[]) {
  oe_enclave_t *atls_server_enclave = nullptr;
  int retval = 0;
  std::string server_port;

  // Set up spdlog with colored console output
  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  auto logger = std::make_shared<spdlog::logger>("host", console_sink);
  logger->set_level(spdlog::level::info);
  spdlog::set_default_logger(logger);

  uint32_t flags = OE_ENCLAVE_FLAG_DEBUG;
  if (check_simulate_opt(&argc, argv)) {
    flags |= OE_ENCLAVE_FLAG_SIMULATE;
  }

  // After check_simulate_opt, argc should be 3: program + enclave path + port
  if (argc != 3) {
    print_usage(argv[0]);
    return 1;
  }

  const char *enclave_path = argv[1];

  // Parse port argument
  server_port = parse_port_argument(argv[2]);
  if (server_port.empty()) {
    spdlog::error("Invalid port argument: {}", argv[2]);
    print_usage(argv[0]);
    return 1;
  }

  spdlog::info("Server port: {}", server_port);
  spdlog::info("Creating enclave from image: {}", enclave_path);

  oe_result_t result = oe_create_atls_server_enclave(enclave_path, OE_ENCLAVE_TYPE_SGX,
                                                     flags, nullptr, 0, &atls_server_enclave);
  if (result != OE_OK) {
    spdlog::error("oe_create_atls_server_enclave() failed: result={} ({})",
                  result, oe_result_str(result));
    return 1;
  }

  spdlog::info("Setting up TLS server on port {}", server_port);
  char port_buffer[server_port.size() + 1];
  std::strcpy(port_buffer, server_port.c_str()); // copy from const char* to char*
  result = ecall_set_up_tls_server(atls_server_enclave, &retval,
                                   port_buffer, true);
  if (result != OE_OK) {
    spdlog::error("ecall_set_up_tls_server() failed: result={} ({})",
                  result, oe_result_str(result));
    spdlog::info("Terminating enclave");
    oe_terminate_enclave(atls_server_enclave);
    return 1;
  }

  spdlog::info("TLS server setup completed successfully");
  spdlog::info("Terminating enclave");
  oe_terminate_enclave(atls_server_enclave);

  return 0;
}