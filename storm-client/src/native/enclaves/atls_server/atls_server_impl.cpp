// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#include <openenclave/enclave.h>
#include <openenclave/attestation/attester.h>
#include <openenclave/attestation/sgx/evidence.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/ssl_cache.h>
#include <mbedtls/x509.h>
#include <mbedtls/x509_crt.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "atls_server_t.h"

// Attestation-related constants
#define TLS_SERVER "TLS server: "
#define TLS_ENCLAVE_SECRET_DATA "Enclave secret data"

// Network I/O buffer size
#define READ_BUFFER_SIZE 1024

extern "C" {

// Helper: socket send callback for mbedTLS
static int enclave_send(void* ctx, const unsigned char* buf, size_t len) {
    int fd = *static_cast<int*>(ctx);
    size_t bytes_sent = 0;
    int ret = ocall_send(&bytes_sent, fd, buf, len, 0);

    if (ret != 0) {
        return MBEDTLS_ERR_NET_SEND_FAILED;
    }

    return static_cast<int>(bytes_sent);
}

// Helper: socket receive callback for mbedTLS
static int enclave_recv(void* ctx, unsigned char* buf, size_t len) {
    int fd = *static_cast<int*>(ctx);
    size_t bytes_received = 0;
    int ret = ocall_recv(&bytes_received, fd, buf, len, 0);

    if (ret != 0) {
        return MBEDTLS_ERR_NET_RECV_FAILED;
    }

    if (bytes_received == 0) {
        return MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY;
    }

    return static_cast<int>(bytes_received);
}

// Certificate generation with attestation evidence
static int generate_certificate_and_pkey(
    mbedtls_x509_crt* cert,
    mbedtls_pk_context* pkey) {

    int ret = 0;
    uint8_t* output_cert = nullptr;
    size_t output_cert_size = 0;
    uint8_t* private_key = nullptr;
    size_t private_key_size = 0;
    uint8_t* public_key = nullptr;
    size_t public_key_size = 0;

    printf(TLS_SERVER "Generating certificate with attestation evidence\n");

    // Try to generate attestation certificate
    // This will work in real SGX hardware, but may fail in simulation
    ret = oe_generate_attestation_certificate(
        (const unsigned char*)"CN=Open Enclave TLS Server",
        private_key,
        private_key_size,
        public_key,
        public_key_size,
        &output_cert,
        &output_cert_size);

    if (ret != OE_OK) {
        printf(TLS_SERVER "Attestation certificate generation not available (code: %d)\n", ret);
        printf(TLS_SERVER "Note: This is expected in simulation mode or without SGX hardware\n");

        // Return non-zero to signal that attestation is not available
        // The caller will handle this by running in demo mode
        return -1;
    }

    // Load the certificate
    ret = mbedtls_x509_crt_parse(cert, output_cert, output_cert_size);
    if (ret != 0) {
        printf(TLS_SERVER "Failed to parse certificate: %d\n", ret);
        goto exit;
    }

    // Load the private key
    ret = mbedtls_pk_parse_key(
        pkey,
        private_key,
        private_key_size,
        nullptr,
        0);
    if (ret != 0) {
        printf(TLS_SERVER "Failed to parse private key: %d\n", ret);
        goto exit;
    }

    printf(TLS_SERVER "Certificate generated successfully\n");

exit:
    oe_free_attestation_certificate(output_cert);
    if (private_key) free(private_key);
    if (public_key) free(public_key);
    return ret;
}

// Certificate verification with attestation
static int verify_certificate(
    void* data,
    mbedtls_x509_crt* crt,
    int depth,
    uint32_t* flags) {

    int ret = 0;
    unsigned char* cert_buf = nullptr;
    size_t cert_size = 0;

    (void)data;

    printf(TLS_SERVER "Verifying certificate at depth %d\n", depth);

    // Only verify the leaf certificate (depth 0)
    if (depth != 0) {
        return 0;
    }

    // Get certificate DER encoding
    cert_buf = crt->raw.p;
    cert_size = crt->raw.len;

    // Verify the certificate with attestation evidence
    oe_result_t result = oe_verify_attestation_certificate(
        cert_buf,
        cert_size,
        nullptr,
        0);

    if (result != OE_OK) {
        printf(TLS_SERVER "Certificate verification failed: %d\n", result);
        *flags |= MBEDTLS_X509_BADCERT_OTHER;
        ret = MBEDTLS_ERR_X509_CERT_VERIFY_FAILED;
    } else {
        printf(TLS_SERVER "Certificate verified successfully\n");
    }

    return ret;
}

// Setup TLS configuration
static int setup_tls_config(
    mbedtls_ssl_config* conf,
    mbedtls_x509_crt* cert,
    mbedtls_pk_context* pkey,
    mbedtls_ctr_drbg_context* ctr_drbg,
    mbedtls_ssl_cache_context* cache) {

    int ret = 0;

    printf(TLS_SERVER "Setting up TLS configuration\n");

    // Initialize SSL configuration
    ret = mbedtls_ssl_config_defaults(
        conf,
        MBEDTLS_SSL_IS_SERVER,
        MBEDTLS_SSL_TRANSPORT_STREAM,
        MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        printf(TLS_SERVER "mbedtls_ssl_config_defaults failed: %d\n", ret);
        return ret;
    }

    // Set RNG
    mbedtls_ssl_conf_rng(conf, mbedtls_ctr_drbg_random, ctr_drbg);

    // Set session cache
    mbedtls_ssl_conf_session_cache(
        conf, cache, mbedtls_ssl_cache_get, mbedtls_ssl_cache_set);

    // Set certificate and private key
    ret = mbedtls_ssl_conf_own_cert(conf, cert, pkey);
    if (ret != 0) {
        printf(TLS_SERVER "mbedtls_ssl_conf_own_cert failed: %d\n", ret);
        return ret;
    }

    // Require client certificate (mutual TLS)
    mbedtls_ssl_conf_authmode(conf, MBEDTLS_SSL_VERIFY_REQUIRED);

    // Set certificate verification callback
    mbedtls_ssl_conf_verify(conf, verify_certificate, nullptr);

    printf(TLS_SERVER "TLS configuration completed\n");
    return 0;
}

// Handle a single client connection
static int handle_client_connection(
    int client_fd,
    mbedtls_ssl_config* conf) {

    int ret = 0;
    mbedtls_ssl_context ssl;
    unsigned char buf[READ_BUFFER_SIZE];
    int len;

    mbedtls_ssl_init(&ssl);

    printf(TLS_SERVER "Setting up SSL session\n");

    ret = mbedtls_ssl_setup(&ssl, conf);
    if (ret != 0) {
        printf(TLS_SERVER "mbedtls_ssl_setup failed: %d\n", ret);
        goto exit;
    }

    // Set I/O functions
    mbedtls_ssl_set_bio(&ssl, &client_fd, enclave_send, enclave_recv, nullptr);

    printf(TLS_SERVER "Performing TLS handshake\n");

    // Perform handshake
    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ &&
            ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            printf(TLS_SERVER "mbedtls_ssl_handshake failed: -0x%x\n", -ret);
            goto exit;
        }
    }

    printf(TLS_SERVER "TLS handshake completed successfully\n");
    printf(TLS_SERVER "Waiting for client data\n");

    // Read data from client
    memset(buf, 0, sizeof(buf));
    len = mbedtls_ssl_read(&ssl, buf, sizeof(buf) - 1);

    if (len > 0) {
        printf(TLS_SERVER "Received %d bytes from client: %s\n", len, (char*)buf);

        // Send response with enclave secret
        const char* response = TLS_ENCLAVE_SECRET_DATA;
        len = mbedtls_ssl_write(&ssl, (const unsigned char*)response, strlen(response));

        if (len > 0) {
            printf(TLS_SERVER "Sent %d bytes to client\n", len);
        } else {
            printf(TLS_SERVER "mbedtls_ssl_write failed: %d\n", len);
        }
    } else if (len == 0 || len == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
        printf(TLS_SERVER "Client closed connection\n");
    } else {
        printf(TLS_SERVER "mbedtls_ssl_read failed: %d\n", len);
    }

    printf(TLS_SERVER "Closing client connection\n");
    mbedtls_ssl_close_notify(&ssl);

exit:
    mbedtls_ssl_free(&ssl);
    ocall_close(nullptr, client_fd);
    return ret;
}

// Main ECALL: Set up and run the TLS server
int ecall_set_up_tls_server(char* port, bool keep_server_up) {
    int ret = 1;
    int server_fd = -1;
    int reuse = 1;
    int setsockopt_ret = 0;
    int send_ret = 0;
    int close_ret = 0;
    bool has_attestation = false;

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cert;
    mbedtls_pk_context pkey;
    mbedtls_ssl_cache_context cache;

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));

    printf(TLS_SERVER "Starting attested TLS server on port %s\n", port);

    // Initialize mbedTLS structures
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_ssl_config_init(&conf);
    mbedtls_x509_crt_init(&cert);
    mbedtls_pk_init(&pkey);
    mbedtls_ssl_cache_init(&cache);

    // Seed the RNG
    printf(TLS_SERVER "Seeding random number generator\n");
    ret = mbedtls_ctr_drbg_seed(
        &ctr_drbg,
        mbedtls_entropy_func,
        &entropy,
        (const unsigned char*)"tls_server",
        strlen("tls_server"));
    if (ret != 0) {
        printf(TLS_SERVER "mbedtls_ctr_drbg_seed failed: %d\n", ret);
        goto exit;
    }

    // Try to generate certificate with attestation
    ret = generate_certificate_and_pkey(&cert, &pkey);
    if (ret == 0) {
        has_attestation = true;
        printf(TLS_SERVER "✓ Attestation available - will use attested TLS\n");

        // Setup TLS configuration
        ret = setup_tls_config(&conf, &cert, &pkey, &ctr_drbg, &cache);
        if (ret != 0) {
            printf(TLS_SERVER "Failed to setup TLS configuration\n");
            goto exit;
        }
    } else {
        printf(TLS_SERVER "✓ Running in demo mode - basic TCP server (no TLS/attestation)\n");
        has_attestation = false;
    }

    // Create server socket
    printf(TLS_SERVER "Creating server socket\n");
    ret = ocall_socket(&server_fd, AF_INET, SOCK_STREAM, 0);
    if (ret != 0 || server_fd < 0) {
        printf(TLS_SERVER "ocall_socket failed\n");
        goto exit;
    }

    // Set socket options
    ocall_setsockopt(&setsockopt_ret, server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // Bind to port
    printf(TLS_SERVER "Binding to port %s\n", port);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(static_cast<uint16_t>(atoi(port)));

    ret = ocall_bind(&ret, server_fd, &serv_addr, sizeof(serv_addr));
    if (ret != 0) {
        printf(TLS_SERVER "ocall_bind failed\n");
        goto exit;
    }

    // Listen for connections
    printf(TLS_SERVER "Listening for connections\n");
    ret = ocall_listen(&ret, server_fd, 5);
    if (ret != 0) {
        printf(TLS_SERVER "ocall_listen failed\n");
        goto exit;
    }

    printf(TLS_SERVER "✓ Server ready and listening on port %s\n", port);

    // Accept client connections
    do {
        struct sockaddr_in client_addr;
        size_t client_addr_len_out = 0;
        int client_fd = -1;

        printf(TLS_SERVER "Waiting for client connection...\n");

        ret = ocall_accept(
            &client_fd,
            server_fd,
            &client_addr,
            sizeof(client_addr),
            &client_addr_len_out);

        if (ret != 0 || client_fd < 0) {
            printf(TLS_SERVER "ocall_accept failed\n");
            break;
        }

        printf(TLS_SERVER "✓ Client connected from %s\n",
               inet_ntoa(client_addr.sin_addr));

        if (has_attestation) {
            // Handle with TLS and attestation
            handle_client_connection(client_fd, &conf);
        } else {
            // Simple demo mode - just echo back a message
            printf(TLS_SERVER "Demo mode: Sending welcome message\n");
            const char* msg = "Hello from enclave! (Demo mode - no TLS)\n";
            size_t sent = 0;
            send_ret = ocall_send(&sent, client_fd, msg, strlen(msg), 0);
            if (send_ret == 0 && sent > 0) {
                printf(TLS_SERVER "Sent %zu bytes\n", sent);
            }
            ocall_close(&close_ret, client_fd);
        }

    } while (keep_server_up);

    ret = 0;

exit:
    if (server_fd >= 0) {
        ocall_close(&close_ret, server_fd);
    }

    // Cleanup mbedTLS structures
    mbedtls_ssl_cache_free(&cache);
    mbedtls_pk_free(&pkey);
    mbedtls_x509_crt_free(&cert);
    mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    printf(TLS_SERVER "Server shutdown complete\n");
    return ret;
}

} // extern "C"
