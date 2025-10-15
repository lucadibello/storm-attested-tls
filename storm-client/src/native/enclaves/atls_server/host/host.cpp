#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstring>
#include <sys/stat.h>

#include <openenclave/host.h>
#include <openenclave/trace.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "atls_server_u.h"
#include "enclave_manager.h"
#include "tcp_client.h"
#include "argument_parser.h"

using namespace oe_common;

std::atomic<bool> g_server_ready(false);
FILE* g_enclave_log_file(nullptr);
FILE* g_host_log_file(nullptr);

void print_usage(const char *program_name) {
    spdlog::error("Usage: {} [--simulate] <enclave_image_path> -port:<port>", program_name);
    spdlog::error("  --simulate    Run in simulation mode");
    spdlog::error("  -port:<port>  Port number for the TLS server (e.g., -port:8443)");
}

// === Host calls
void host_customized_log(
    void* context,
    const bool is_enclave,
    const tm* t,
    const long int u_secs,
    const oe_log_level_t level,
    const uint64_t host_thread_id,
    const char* message)
{
    char time[25];
    strftime(time, sizeof(time), "%Y-%m-%dT%H:%M:%S%z", t);

    FILE* log_file = nullptr;
    if (level >= OE_LOG_LEVEL_WARNING)
    {
        log_file = static_cast<FILE *>(context);
    }
    else
    {
        log_file = stderr;
    }

    fprintf(
        log_file,
        "%s.%06ld, %s, %s, %lx, %s",
        time,
        u_secs,
        (is_enclave ? "E" : "H"),
        oe_log_level_strings[level],
        host_thread_id,
        message);
}

const char* extract_log_dir(int *argc, const char *argv[]) {
    for (int i = 0; i < *argc - 1; i++) {
        if (strcmp(argv[i], "--log-dir") == 0) {
            const char* log_dir = argv[i + 1];
            memmove(&argv[i], &argv[i + 2], (*argc - i - 1) * sizeof(char *));
            (*argc) -= 2;
            return log_dir;
        }
    }
    return "log"; // default directory
}

bool create_directory(const char* dir_path) {
    struct stat st = {};
    if (stat(dir_path, &st) == -1) {
        if (mkdir(dir_path, 0755) != 0) {
            fprintf(stderr, "Error: Failed to create log directory\n");
            return false;
        }
    }
    return true;
}

oe_enclave_t *create_enclave(const char *enclave_path, const uint32_t flags) {
    oe_enclave_t *enclave = nullptr;

    printf("Host: Enclave library %s\n", enclave_path);
    const oe_result_t result = oe_create_atls_server_enclave(
        enclave_path,
        OE_ENCLAVE_TYPE_SGX,
        flags,
        nullptr,
        0,
        &enclave);

    if (result != OE_OK) {
        printf(
            "Host: oe_create_remoteattestation_enclave failed. %s",
            oe_result_str(result));
    } else {
        printf("Host: Enclave successfully created.\n");
    }
    return enclave;
}

// Thread function to run the server
void run_server(oe_enclave_t *enclave, const std::string &port) {
    spdlog::info("Server thread: Starting TLS server on port {}", port);

    char port_buffer[port.size() + 1];
    std::strcpy(port_buffer, port.c_str());

    int retval = 0;

    // The enclave will signal readiness via ocall_server_ready()

    if (oe_result_t result = ecall_set_up_tls_server(enclave, &retval, port_buffer, false); result != OE_OK) {
        spdlog::error("Server thread: ecall_set_up_tls_server() failed: result={} ({})",
                      result, oe_result_str(result));
    } else if (retval != 0) {
        spdlog::error("Server thread: Server returned error code: {}", retval);
    } else {
        spdlog::info("Server thread: Server completed successfully");
    }
}

// Run a simple connectivity test using the common TcpClient
bool run_connectivity_test(const int port) {
    spdlog::info("");
    spdlog::info("========================================");
    spdlog::info("Running Connectivity Test");
    spdlog::info("========================================");

    // Wait for server to be signaled ready by the enclave
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
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

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

    if (ssize_t received = client.receive(buffer, sizeof(buffer) - 1); received > 0) {
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

int main(const int argc, const char *argv[]) {
    oe_enclave_t *atls_server_enclave = nullptr;

    // Set up spdlog with colored console output
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    const auto logger = std::make_shared<spdlog::logger>("host", console_sink);
    logger->set_level(spdlog::level::debug);
    spdlog::set_default_logger(logger);

    // create log directories
    constexpr char base_dir[] = "log";
    constexpr char log_dir_enclave[] = "log/enclave";
    constexpr char log_dir_host[] = "log/host";
    if (!create_directory(base_dir) ||
        !create_directory(log_dir_enclave) ||
        !create_directory(log_dir_host))
    {
        spdlog::error("An error occurred while creating log directory");
        return 1;
    }

    // Generate timestamp for log filenames
    const time_t now = time(nullptr);
    const tm* time_info = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", time_info);

    // Create timestamped filename for host logs
    char host_log_filename[256];
    snprintf(host_log_filename, sizeof(host_log_filename), "%s/oe_host_out_%s.txt", log_dir_host, timestamp);

    // set log callback for host
    g_host_log_file = fopen(host_log_filename, "w");
    if (!g_host_log_file) {
        spdlog::error("An error occurred while creating log directory {}", host_log_filename);
        return 1;
    }
    oe_log_set_callback(g_host_log_file, host_customized_log);

    // set log callback on enclave
    // open file for enclave logs
    char enclave_log_filename[256];
    snprintf(enclave_log_filename, sizeof(host_log_filename), "%s/oe_enclave_out_%s.txt", log_dir_enclave, timestamp);

    g_enclave_log_file = fopen(enclave_log_filename, "w");
    if (!g_enclave_log_file) {
        spdlog::error("An error occurred while creating log directory {}", host_log_filename);
        fclose(g_host_log_file); // close other log file
        return 1;
    }

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
    std::string server_port = parser.parse_value_argument("-port:");
    if (server_port.empty()) {
        spdlog::error("Invalid or missing port argument");
        print_usage(argv[0]);
        return 1;
    }

    const int port_number = std::stoi(server_port);
    spdlog::info("Configuration:");
    spdlog::info("  - Enclave: {}", enclave_path);
    spdlog::info("  - Port: {}", server_port);
    spdlog::info("  - Simulation: {}", (flags & OE_ENCLAVE_FLAG_SIMULATE) ? "Yes" : "No");
    spdlog::info("");

    // Use the enclave-specific constructor for atls_server
    // This registers the OCALL table properly
    spdlog::info("Creating enclave...");
    atls_server_enclave = create_enclave(enclave_path.c_str(), flags);
    if (!atls_server_enclave) {
        spdlog::error("✗ Failed to create enclave. Check console for logs");
        return 1;
    }
    spdlog::info("✓ Enclave created successfully");
    spdlog::info("");

    // Set callback for enclave logs
    if (ecall_set_log_callback(atls_server_enclave) != OE_OK) {
        spdlog::error("Failed to set log callback.");
        // close both files + destroy enclave
        fclose(g_enclave_log_file);
        fclose(g_host_log_file);
        EnclaveManager::destroy_enclave(atls_server_enclave);
    }
    spdlog::info("Configured enclave callback successfully. Logging to {}", enclave_log_filename);

    // Start server in a separate thread
    std::thread server_thread(run_server, atls_server_enclave, server_port);

    // Run connectivity test
    // const bool test_passed = run_connectivity_test(port_number);

    // Give server a moment to handle the connection
    std::this_thread::sleep_for(std::chrono::seconds(180));

    //spdlog::info("Waiting for server to complete...");
    // spdlog::info("(Press Ctrl+C to stop if server is in continuous mode)");

    // Wait a bit for the server to process the connection
    // std::this_thread::sleep_for(std::chrono::seconds(3));

    // spdlog::info("");
    // spdlog::info("Terminating enclave...");
    // spdlog::info("✓ Enclave terminated");

    // free enclave + check if success
    if (!EnclaveManager::destroy_enclave(atls_server_enclave)) {
        spdlog::error("Failed to destroy enclave.");
    }

    // Note: server_thread might still be running if the server is in continuous mode
    // In production, you'd want proper cleanup here
    if (server_thread.joinable()) {
        server_thread.detach(); // Detach since server might be blocking
    }

    spdlog::debug("Bye!");

    // close all files
    fclose(g_host_log_file);
    fclose(g_enclave_log_file);
    return 0;
}
