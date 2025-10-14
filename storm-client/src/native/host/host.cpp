// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#include <cstdint>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "atls_server_u.h"
#include "enclave_manager.h"
#include "tcp_client.h"
#include "argument_parser.h"

using namespace oe_common;

std::atomic<bool> g_server_ready(false);

void print_usage(const char* program_name) {
  spdlog::error("Usage: {} [--simulate] <enclave_image_path> -port:<port>", program_name);
  spdlog::error("  --simulate    Run in simulation mode");
  spdlog::error("  -port:<port>  Port number for the TLS server (e.g., -port:8443)");
}

// Thread function to run the server
void run_server(oe_enclave_t* enclave, const std::string& port) {
    spdlog::info("Server thread: Starting TLS server on port {}", port);

    char port_buffer[port.size() + 1];
    std::strcpy(port_buffer, port.c_str());

    int retval = 0;

    // Signal that we're starting the server
    g_server_ready = true;

    oe_result_t result = ecall_set_up_tls_server(enclave, &retval, port_buffer, false);

    if (result != OE_OK) {
        spdlog::error("Server thread: ecall_set_up_tls_server() failed: result={} ({})",
                      result, oe_result_str(result));
    } else if (retval != 0) {
        spdlog::error("Server thread: Server returned error code: {}", retval);
    } else {
        spdlog::info("Server thread: Server completed successfully");
    }
}

// Run a simple connectivity test using the common TcpClient
bool run_connectivity_test(int port) {
    spdlog::info("");
    spdlog::info("========================================");
    spdlog::info("Running Connectivity Test");
    spdlog::info("========================================");

    // Wait for server to be ready
    int max_wait = 30; // 3 seconds
    while (!g_server_ready && max_wait > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        max_wait--;
    }

    if (!g_server_ready) {
        spdlog::error("Test: Server failed to start in time");
        return false;
    }

    // Give the server a moment to bind and listen
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    TcpClient client("127.0.0.1", port);

    spdlog::info("Test: Attempting to connect to server...");
    if (!client.connect()) {
        spdlog::error("Test: ✗ Failed to connect to server");
        return false;
    }

    spdlog::info("Test: ✓ Successfully connected to server!");
    spdlog::info("Test: Note - TLS handshake will fail (expected, this is just TCP connectivity)");

    // Keep connection open briefly to let server see it
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Receive and display message from server
    char buffer[1024];
    spdlog::info("Test: Waiting to receive message...");
    ssize_t received = client.receive(buffer, sizeof(buffer) - 1);

    if (received > 0) {
        buffer[received] = '\0';
        spdlog::info("Test: ✓ Received {} bytes: '{}'", received, buffer);
    } else {
        spdlog::warn("Test: No data received");
    }

    client.disconnect();
    spdlog::info("Test: Disconnected from server");

    spdlog::info("");
    spdlog::info("========================================");
    spdlog::info("Connectivity Test PASSED ✓");
    spdlog::info("========================================");
    spdlog::info("");

    return true;
}

int main(int argc, const char *argv[]) {
  oe_enclave_t *atls_server_enclave = nullptr;
  std::string server_port;

  // Set up spdlog with colored console output
  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  auto logger = std::make_shared<spdlog::logger>("host", console_sink);
  logger->set_level(spdlog::level::info);
  spdlog::set_default_logger(logger);

  spdlog::info("");
  spdlog::info("========================================");
  spdlog::info("Attested TLS Server Demo");
  spdlog::info("========================================");
  spdlog::info("");

  // Use ArgumentParser to parse command-line arguments
  ArgumentParser parser(argc, argv);

  uint32_t flags = OE_ENCLAVE_FLAG_DEBUG;
  if (parser.check_and_remove_flag("--simulate")) {
    spdlog::info("Running in simulation mode");
    flags |= OE_ENCLAVE_FLAG_SIMULATE;
  }

  // After removing flags, should have: program + enclave path + port
  if (parser.get_argc() != 3) {
    print_usage(argv[0]);
    return 1;
  }

  const std::string enclave_path = parser.get_arg(1);

  // Parse port argument
  server_port = parser.parse_value_argument("-port:");
  if (server_port.empty()) {
    spdlog::error("Invalid or missing port argument");
    print_usage(argv[0]);
    return 1;
  }

  int port_number = std::stoi(server_port);
  spdlog::info("Configuration:");
  spdlog::info("  - Enclave: {}", enclave_path);
  spdlog::info("  - Port: {}", server_port);
  spdlog::info("  - Simulation: {}", (flags & OE_ENCLAVE_FLAG_SIMULATE) ? "Yes" : "No");
  spdlog::info("");

  // Use the enclave-specific constructor for atls_server
  // This registers the OCALL table properly
  spdlog::info("Creating enclave...");
  oe_result_t result = oe_create_atls_server_enclave(
      enclave_path.c_str(),
      OE_ENCLAVE_TYPE_AUTO,
      flags,
      nullptr,
      0,
      &atls_server_enclave);

  if (result != OE_OK || !atls_server_enclave) {
    spdlog::error("✗ Failed to create enclave: {} ({})",
                  oe_result_str(result), result);
    return 1;
  }
  spdlog::info("✓ Enclave created successfully");
  spdlog::info("");

  // Start server in a separate thread
  std::thread server_thread(run_server, atls_server_enclave, server_port);

  // Run connectivity test
  bool test_passed = run_connectivity_test(port_number);

  // Give server a moment to handle the connection
  std::this_thread::sleep_for(std::chrono::seconds(2));

  spdlog::info("Waiting for server to complete...");
  spdlog::info("(Press Ctrl+C to stop if server is in continuous mode)");

  // Wait a bit for the server to process the connection
  std::this_thread::sleep_for(std::chrono::seconds(3));

  spdlog::info("");
  spdlog::info("Terminating enclave...");
  EnclaveManager::destroy_enclave(atls_server_enclave);
  spdlog::info("✓ Enclave terminated");

  // Note: server_thread might still be running if the server is in continuous mode
  // In production, you'd want proper cleanup here
  if (server_thread.joinable()) {
    server_thread.detach(); // Detach since server might be blocking
  }

  spdlog::info("");
  spdlog::info("========================================");
  spdlog::info("Demo Complete");
  spdlog::info("Result: {}", test_passed ? "SUCCESS ✓" : "FAILED ✗");
  spdlog::info("========================================");

  return test_passed ? 0 : 1;
}