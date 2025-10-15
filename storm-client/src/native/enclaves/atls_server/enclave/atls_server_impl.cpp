#include <cassert>
#include <openenclave/enclave.h>
#include <openenclave/tracee.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <openssl_utility.h>
#include <oe_enclave_utility.h>
#include <openenclave/log.h>
#include <openenclave/attestation/verifier.h>

#include "atls_server_t.h"
#include "consts.h"

// Custom logging macro that sends logs through OCALL
#define ENCLAVE_LOG(fmt, ...) do { \
    char log_buf[256]; \
    int written = snprintf(log_buf, sizeof(log_buf), fmt, ##__VA_ARGS__); \
    if (written > 0) { \
        ocall_transfer_logs_to_file(log_buf, (written < sizeof(log_buf)) ? written : sizeof(log_buf) - 1); \
    } \
    printf(fmt, ##__VA_ARGS__); \
    fflush(stdout); \
} while(0)

// Certificate verification callback with attestation
static int verify_callback(int preverify_ok, X509_STORE_CTX* ctx) {
    (void)preverify_ok;

    ENCLAVE_LOG("[TLS Server] Certificate verification callback invoked\n");

    // Get the certificate being verified
    X509* cert = X509_STORE_CTX_get_current_cert(ctx);
    int depth = X509_STORE_CTX_get_error_depth(ctx);

    ENCLAVE_LOG("[TLS Server] Verifying certificate at depth %d\n", depth);

    // Only verify the leaf certificate (depth 0)
    if (depth != 0) {
        return 1; // Accept intermediate/root certificates
    }

    // Get DER-encoded certificate
    unsigned char* cert_buf = nullptr;
    int cert_len = i2d_X509(cert, &cert_buf);

    if (cert_len < 0) {
        ENCLAVE_LOG("[TLS Server] Failed to encode certificate to DER\n");
        return 0;
    }

    // Verify the certificate with attestation evidence
    oe_result_t result = oe_verify_attestation_certificate(
        cert_buf,
        cert_len,
        nullptr,
        nullptr);

    OPENSSL_free(cert_buf);

    if (result != OE_OK) {
        ENCLAVE_LOG("[TLS Server] Certificate attestation verification failed: %s\n", oe_result_str(result));
        return 0;
    }

    ENCLAVE_LOG("[TLS Server] Certificate verified successfully with attestation\n");
    return 1;
}

// Create listening socket
static int create_listener_socket(int port, int& server_socket) {
    int ret = -1;
    const int reuse = 1;
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        ENCLAVE_LOG(TLS_SERVER "socket creation failed\n");
        goto exit;
    }

    if (setsockopt(
            server_socket,
            SOL_SOCKET,
            SO_REUSEADDR,
            (const void*)&reuse,
            sizeof(reuse)) < 0) {
        ENCLAVE_LOG(TLS_SERVER "setsockopt failed\n");
        goto exit;
    }

    if (bind(server_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ENCLAVE_LOG(TLS_SERVER "Unable to bind socket to port %d\n", port);
        goto exit;
    }

    if (listen(server_socket, 20) < 0) {
        ENCLAVE_LOG(TLS_SERVER "Unable to listen on socket\n");
        goto exit;
    }

    ENCLAVE_LOG(TLS_SERVER "Listening on port %d\n", port);
    ret = 0;

exit:
    return ret;
}

// Handle client connections
static int handle_communication_until_done(
    int& server_socket_fd,
    int& client_socket_fd,
    SSL_CTX*& ssl_server_ctx,
    SSL*& ssl_session,
    bool keep_server_up) {

    int ret = -1;
    struct sockaddr_in addr;
    socklen_t len;
    int ssl_accept_ret;
    int ssl_error;

waiting_for_connection_request:

    // Reset SSL session and client socket for new connection
    if (client_socket_fd >= 0) {
        close(client_socket_fd);
        client_socket_fd = -1;
    }
    if (ssl_session != nullptr) {
        SSL_free(ssl_session);
        ssl_session = nullptr;
    }

    ENCLAVE_LOG(TLS_SERVER "Waiting for client connection\n");

    len = sizeof(addr);
    client_socket_fd = accept(server_socket_fd, (struct sockaddr*)&addr, &len);

    if (client_socket_fd < 0) {
        ENCLAVE_LOG(TLS_SERVER "Unable to accept client request\n");
        goto exit;
    }

    ENCLAVE_LOG(TLS_SERVER "Client connected\n");

    // Create a new SSL structure for connection
    ssl_session = SSL_new(ssl_server_ctx);
    if (ssl_session == nullptr) {
        ENCLAVE_LOG(TLS_SERVER "Unable to create new SSL connection state object\n");
        ERR_print_errors_fp(stderr);
        goto exit;
    }

    SSL_set_fd(ssl_session, client_socket_fd);

    // Perform TLS handshake
    ENCLAVE_LOG(TLS_SERVER "Performing TLS handshake\n");
    ssl_accept_ret = SSL_accept(ssl_session);
    if (ssl_accept_ret <= 0) {
        ssl_error = SSL_get_error(ssl_session, ssl_accept_ret);
        ENCLAVE_LOG(TLS_SERVER "SSL handshake failed with error: %d\n", ssl_error);
        ERR_print_errors_fp(stderr);
        goto exit;
    }

    ENCLAVE_LOG(TLS_SERVER "TLS handshake completed successfully\n");

    // Read from client
    ENCLAVE_LOG(TLS_SERVER "Reading from client...\n");
    if (openssl_utility::read_from_session_peer(
            ssl_session, CLIENT_PAYLOAD, CLIENT_PAYLOAD_SIZE) != 0) {
        ENCLAVE_LOG(TLS_SERVER "Read from client failed\n");
        goto exit;
    }

    // Write to client
    ENCLAVE_LOG(TLS_SERVER "Writing to client...\n");
    if (openssl_utility::write_to_session_peer(
            ssl_session, SERVER_PAYLOAD, strlen(SERVER_PAYLOAD)) != 0) {
        ENCLAVE_LOG(TLS_SERVER "Write to client failed\n");
        goto exit;
    }

    ENCLAVE_LOG(TLS_SERVER "Client communication completed successfully\n");

    if (keep_server_up) {
        goto waiting_for_connection_request;
    }

    ret = 0;

exit:
    return ret;
}

void enclave_customized_log(
    void* context,
    const oe_log_level_t level,
    const uint64_t thread_id,
    const char* message)
{
    char modified_log[200];

    sprintf(
        modified_log,
        "E, %s, %lx, %s",
        oe_log_level_strings[level],
        thread_id,
        message);

    OE_UNUSED(context);

    const oe_result_t result = ocall_transfer_logs_to_file(modified_log, strlen(modified_log));
    OE_UNUSED(result);
}

// ECALL to set log handler
extern "C" void ecall_set_log_callback()
{
    oe_enclave_log_set_callback(nullptr, enclave_customized_log);
}

// Main ECALL: Set up and run the TLS server
extern "C" int ecall_set_up_tls_server(char* port, bool keep_server_up) {
    int ret = OE_FAILURE;
    int server_socket_fd = -1;
    int client_socket_fd = -1;
    int port_number = 0;
    char* endptr = nullptr;
    long port_num;
    const char* subject_name = "CN=Attested TLS Server,O=Università della Svizzera italiana (USI),OU=Faculty of Informatics,L=Lugano,ST=Ticino,C=CH";

    X509* certificate = nullptr;
    EVP_PKEY* pkey = nullptr;
    SSL_CTX* ssl_server_ctx = nullptr;
    SSL* ssl_session = nullptr;

    // Validate and parse port number
    port_num = strtol(port, &endptr, 10);
    if (*endptr != '\0' || port_num <= 0 || port_num > 65535) {
        ENCLAVE_LOG("[TLS Server] Invalid port number: %s\n", port);
        return OE_INVALID_PARAMETER;
    }
    port_number = (int)port_num;

    ENCLAVE_LOG("[TLS Server] Starting attested TLS server on port %d\n", port_number);

    // Load required OE modules
    ret = oe_enclave_utility::load_oe_modules();
    if (ret != OE_OK) {
        ENCLAVE_LOG("[TLS Server] Loading required Open Enclave modules failed\n");
        return ret;
    }

    // Initialize OpenSSL
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    // Create SSL context
    ssl_server_ctx = SSL_CTX_new(TLS_server_method());
    if (ssl_server_ctx == nullptr) {
        ENCLAVE_LOG("[TLS Server] Unable to create SSL context\n");
        ERR_print_errors_fp(stderr);
        goto exit;
    }

    // Initialize SSL context with proper settings
    ret = openssl_utility::initialize_ssl_context(ssl_server_ctx);
    if (ret != OE_OK) {
        ENCLAVE_LOG("[TLS Server] Unable to initialize SSL context\n");
        goto exit;
    }

    // Set certificate verification callback
    SSL_CTX_set_verify(ssl_server_ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, verify_callback);

    // Initialize verifier for attestation
    oe_verifier_initialize();

    // Generate attested certificate and private key
    ENCLAVE_LOG("[TLS Server] Generating attested certificate...\n");

    ret = openssl_utility::generate_certificate_and_pkey(&certificate, &pkey, subject_name);
    if (ret != OE_OK) {
        ENCLAVE_LOG("[TLS Server] Failed to generate attested certificate: %s\n", oe_result_str(static_cast<oe_result_t>(ret)));
        goto exit;
    }

    // Set certificate and private key in SSL context
    if (SSL_CTX_use_certificate(ssl_server_ctx, certificate) <= 0) {
        ENCLAVE_LOG("[TLS Server] Failed to set certificate\n");
        ERR_print_errors_fp(stderr);
        ret = OE_FAILURE;
        goto exit;
    }

    if (SSL_CTX_use_PrivateKey(ssl_server_ctx, pkey) <= 0) {
        ENCLAVE_LOG("[TLS Server] Failed to set private key\n");
        ERR_print_errors_fp(stderr);
        ret = OE_FAILURE;
        goto exit;
    }

    // Verify that private key matches certificate
    if (!SSL_CTX_check_private_key(ssl_server_ctx)) {
        ENCLAVE_LOG("[TLS Server] Private key does not match certificate\n");
        ERR_print_errors_fp(stderr);
        ret = OE_FAILURE;
        goto exit;
    }

    ENCLAVE_LOG("[TLS Server] Certificate and private key configured successfully\n");

    // Create and bind listening socket
    if (create_listener_socket(port_number, server_socket_fd) != 0) {
        ENCLAVE_LOG("[TLS Server] Unable to create listener socket\n");
        ret = OE_FAILURE;
        goto exit;
    }

    // Signal to host that server is ready
    ocall_server_ready();

    // Handle client connections
    ret = handle_communication_until_done(
        server_socket_fd,
        client_socket_fd,
        ssl_server_ctx,
        ssl_session,
        keep_server_up);

    if (ret != 0) {
        ENCLAVE_LOG("[TLS Server] Communication error: %d\n", ret);
        goto exit;
    }

    ret = OE_OK;

exit:
    // Cleanup
    if (client_socket_fd >= 0) {
        close(client_socket_fd);
    }
    if (server_socket_fd >= 0) {
        close(server_socket_fd);
    }
    if (ssl_session != nullptr) {
        SSL_shutdown(ssl_session);
        SSL_free(ssl_session);
    }
    if (ssl_server_ctx != nullptr) {
        SSL_CTX_free(ssl_server_ctx);
    }
    if (certificate != nullptr) {
        X509_free(certificate);
    }
    if (pkey != nullptr) {
        EVP_PKEY_free(pkey);
    }

    oe_verifier_shutdown();

    ENCLAVE_LOG("[TLS Server] Server shutdown complete\n");
    return ret;
}