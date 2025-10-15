#include <openenclave/enclave.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/ssl_cache.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/platform.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <mbedtls_utility.h>
#include <oe_enclave_utility.h>
#include <openenclave/attestation/verifier.h>

#include "atls_server_t.h"

// Attestation-related constants
#define LOG_PREFIX_TLS_SERVER "TLS server: "
#define TLS_ENCLAVE_SECRET_DATA "Enclave secret data"

// Network I/O buffer size
#define READ_BUFFER_SIZE 1024
#define MAX_ERROR_BUFF_SIZE 256

static void my_debug(
    void* ctx,
    const int level,
    const char* file,
    const int line,
    const char* str)
{
    ((void)level);

    mbedtls_fprintf(static_cast<FILE *>(ctx), "%s:%04d: %s", file, line, str);
    fflush(static_cast<FILE *>(ctx));
}

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
    const int fd = *static_cast<int*>(ctx);
    size_t bytes_received = 0;

    if (const int ret = ocall_recv(&bytes_received, fd, buf, len, 0); ret != 0) return MBEDTLS_ERR_NET_RECV_FAILED;
    if (bytes_received == 0) return MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY;

    return static_cast<int>(bytes_received);
}

// Certificate verification with attestation
static int verify_certificate(
    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    void* data,
    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    mbedtls_x509_crt* crt,
    const int depth,
    uint32_t* flags) {

    int ret = 0;
    unsigned char* cert_buf = nullptr;
    size_t cert_size = 0;

    (void)data;

    printf(LOG_PREFIX_TLS_SERVER "Verifying certificate at depth %d\n", depth);

    // Only verify the leaf certificate (depth 0)
    if (depth != 0) {
        return 0;
    }

    // Get certificate DER encoding
    cert_buf = crt->raw.p;
    cert_size = crt->raw.len;

    // Verify the certificate with attestation evidence
    const oe_result_t result = oe_verify_attestation_certificate(
        cert_buf,
        cert_size,
        nullptr,
        nullptr);

    if (result != OE_OK) {
        printf(LOG_PREFIX_TLS_SERVER "Certificate verification failed: %d\n", result);
        *flags |= MBEDTLS_X509_BADCERT_OTHER;
        ret = MBEDTLS_ERR_X509_CERT_VERIFY_FAILED;
    } else {
        printf(LOG_PREFIX_TLS_SERVER "Certificate verified successfully\n");
    }

    return ret;
}

static int setup_tls_config(
    mbedtls_ssl_config* ssl_conf,
    mbedtls_x509_crt* server_cert,
    mbedtls_pk_context* pkey,
    mbedtls_ctr_drbg_context* ctr_drbg,
    mbedtls_ssl_cache_context* cache) {

    int ret = 0;
    printf(LOG_PREFIX_TLS_SERVER "Setting up TLS configuration\n");

    // Initialize SSL configuration
    ret = mbedtls_ssl_config_defaults(
        ssl_conf,
        MBEDTLS_SSL_IS_SERVER,
        MBEDTLS_SSL_TRANSPORT_STREAM,
        MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        printf(LOG_PREFIX_TLS_SERVER "mbedtls_ssl_config_defaults failed: %d\n", ret);
        return ret;
    }

    // Set SSL RNG
    mbedtls_ssl_conf_rng(ssl_conf, mbedtls_ctr_drbg_random, ctr_drbg);
    mbedtls_ssl_conf_dbg(ssl_conf, my_debug, stdout);

    // Set session cache
    mbedtls_ssl_conf_session_cache(
        ssl_conf, cache, mbedtls_ssl_cache_get, mbedtls_ssl_cache_set);

    // OPTIONS:
    //  - MBEDTLS_SSL_VERIFY_REQUIRED (mutual TLS) = fail if no client cert or verification fails
    //  - MBEDTLS_SSL_VERIFY_OPTIONAL = request client cert, but allow connection even if no
    mbedtls_ssl_conf_authmode(ssl_conf, MBEDTLS_SSL_VERIFY_OPTIONAL);

    // Set certificate verification callback
    mbedtls_ssl_conf_verify(ssl_conf, verify_certificate, nullptr);
    mbedtls_ssl_conf_ca_chain(ssl_conf, server_cert->next, nullptr);

    // Set certificate and private key
    ret = mbedtls_ssl_conf_own_cert(ssl_conf, server_cert, pkey);
    if (ret != 0) {
        printf(LOG_PREFIX_TLS_SERVER "mbedtls_ssl_conf_own_cert failed: %d\n", ret);
        return ret;
    }

    // log + return success code
    printf(LOG_PREFIX_TLS_SERVER "TLS configuration completed\n");
    return 0;
}

// Handle a single client connection
static int handle_client_connection(
    int client_fd,
    const mbedtls_ssl_config* conf) {

    int ret = 0;
    mbedtls_ssl_context ssl;
    unsigned char buf[READ_BUFFER_SIZE];
    int len;

    mbedtls_ssl_init(&ssl);

    printf(LOG_PREFIX_TLS_SERVER "Setting up SSL session\n");

    ret = mbedtls_ssl_setup(&ssl, conf);
    if (ret != 0) {
        printf(LOG_PREFIX_TLS_SERVER "mbedtls_ssl_setup failed: %d\n", ret);
        goto exit;
    }

    // Set I/O functions
    mbedtls_ssl_set_bio(&ssl, &client_fd, enclave_send, enclave_recv, nullptr);

    printf(LOG_PREFIX_TLS_SERVER "Performing TLS handshake\n");

    // Perform handshake
    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ &&
            ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            printf(LOG_PREFIX_TLS_SERVER "mbedtls_ssl_handshake failed: -0x%x\n", -ret);
            goto exit;
        }
    }

    printf(LOG_PREFIX_TLS_SERVER "TLS handshake completed successfully\n");
    printf(LOG_PREFIX_TLS_SERVER "Waiting for client data\n");

    // Read data from client
    memset(buf, 0, sizeof(buf));
    len = mbedtls_ssl_read(&ssl, buf, sizeof(buf) - 1);

    if (len > 0) {
        printf(LOG_PREFIX_TLS_SERVER "Received %d bytes from client: %s\n", len, reinterpret_cast<char *>(buf));

        // Send response with enclave secret
        const auto response = TLS_ENCLAVE_SECRET_DATA;
        len = mbedtls_ssl_write(&ssl, reinterpret_cast<const unsigned char *>(response), strlen(response));

        if (len > 0) {
            printf(LOG_PREFIX_TLS_SERVER "Sent %d bytes to client\n", len);
        } else {
            printf(LOG_PREFIX_TLS_SERVER "mbedtls_ssl_write failed: %d\n", len);
        }
    } else if (len == 0 || len == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
        printf(LOG_PREFIX_TLS_SERVER "Client closed connection\n");
    } else {
        printf(LOG_PREFIX_TLS_SERVER "mbedtls_ssl_read failed: %d\n", len);
    }

    printf(LOG_PREFIX_TLS_SERVER "Closing client connection\n");
    mbedtls_ssl_close_notify(&ssl);

exit:
    mbedtls_ssl_free(&ssl);
    ocall_close(nullptr, client_fd);
    return ret;
}

// Main ECALL: Set up and run the TLS server
extern "C" int ecall_set_up_tls_server(char* port, bool keep_server_up) {
    int ret = 1;
    int server_fd = -1;
    int reuse = 1;
    int setsockopt_ret = 0;
    int send_ret = 0;
    int close_ret = 0;
    bool has_attestation = false;

    // validate + cast port number to correct type
    char* endptr = nullptr;
    long port_num = strtol(port, &endptr, 10);
    if (*endptr != '\0' || port_num <= 0 || port_num > 65535) {
        fprintf(stderr, "Invalid port number: %s\n", port);
        exit(EXIT_FAILURE);
    }

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config ssl_config;
    mbedtls_x509_crt server_cert;
    mbedtls_pk_context pkey;
    mbedtls_ssl_cache_context cache;
    mbedtls_net_context listen_fd;

    sockaddr_in serv_addr = {};

    // Initialize mbedTLS structures
    mbedtls_net_init(&listen_fd);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&ssl_config);
    mbedtls_ssl_cache_init(&cache);
    mbedtls_x509_crt_init(&server_cert);
    mbedtls_pk_init(&pkey);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    oe_verifier_initialize();

    // NOTE: this do-while(false) block is used to have a structured error handling
    // This is done to guarantee the cleanup without GOTO statements.
    do {
        // load required oe modules
        /* Load host resolver and socket interface modules explicitly */
        if (oe_enclave_utility::load_oe_modules() != OE_OK)
        {
            printf(LOG_PREFIX_TLS_SERVER "loading required Open Enclave modules failed\n");
            break;
        }

        printf(LOG_PREFIX_TLS_SERVER "Starting attested TLS server on port %s\n", port);

        // Seed the RNG
        printf(LOG_PREFIX_TLS_SERVER "Seeding random number generator\n");
        ret = mbedtls_ctr_drbg_seed(
            &ctr_drbg,
            mbedtls_entropy_func,
            &entropy,
            reinterpret_cast<const unsigned char *>("tls_server"),
            strlen("tls_server"));
        if (ret != 0) {
            printf(LOG_PREFIX_TLS_SERVER "mbedtls_ctr_drbg_seed failed: %d\n", ret);
            break;
        }

        // Try to generate certificate with attestation

        // FIXME: this has to be changed in the future. Now hard-coded.
        constexpr unsigned char certificate_subject_name[] =
            "CN=Attested TLS Server,"
            "O=Università della Svizzera italiana (USI),"
            "OU=Faculty of Informatics,"
            "L=Lugano,"
            "ST=Ticino,"
            "C=CH";
        ret = mbedtls_utility::generate_certificate_and_pkey(&server_cert, &pkey, certificate_subject_name);
        if (ret == 0) {
            printf(LOG_PREFIX_TLS_SERVER "✓ Attestation available - will use attested TLS\n");

            // Setup TLS configuration
            ret = setup_tls_config(&ssl_config, &server_cert, &pkey, &ctr_drbg, &cache);
            if (ret != 0) {
                printf(LOG_PREFIX_TLS_SERVER "Failed to set up TLS configuration\n");
                break;
            }
        } else {
            printf(LOG_PREFIX_TLS_SERVER "Failed to generate attested certificate.\n");
            break;
        }

        // Create server socket
        printf(LOG_PREFIX_TLS_SERVER "Creating server socket\n");
        ret = ocall_socket(&server_fd, AF_INET, SOCK_STREAM, 0);
        if (ret != 0 || server_fd < 0) {
            printf(LOG_PREFIX_TLS_SERVER "ocall_socket failed\n");
            break;
        }

        // Set socket options
        ocall_setsockopt(&setsockopt_ret, server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        // Bind to port
        printf(LOG_PREFIX_TLS_SERVER "Binding to port %s\n", port);
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(static_cast<uint16_t>(port_num));

        ret = ocall_bind(&ret, server_fd, &serv_addr, sizeof(serv_addr));
        if (ret != 0) {
            printf(LOG_PREFIX_TLS_SERVER "ocall_bind failed\n");
            break;
        }

        // Listen for connections
        printf(LOG_PREFIX_TLS_SERVER "Listening for connections\n");
        ret = ocall_listen(&ret, server_fd, 5);
        if (ret != 0) {
            printf(LOG_PREFIX_TLS_SERVER "ocall_listen failed\n");
            break;
        }

        printf(LOG_PREFIX_TLS_SERVER "✓ Server ready and listening on port %s\n", port);

        // Accept client connections
        do {
            sockaddr_in client_addr{};
            size_t client_addr_len_out = 0;
            int client_fd = -1;

            printf(LOG_PREFIX_TLS_SERVER "Waiting for client connection...\n");

            ret = ocall_accept(
                &client_fd,
                server_fd,
                &client_addr,
                sizeof(client_addr),
                &client_addr_len_out);

            if (ret != 0 || client_fd < 0) {
                printf(LOG_PREFIX_TLS_SERVER "ocall_accept failed\n");
                break;
            }

            printf(LOG_PREFIX_TLS_SERVER "✓ Client connected from %s\n",
                   inet_ntoa(client_addr.sin_addr));

            // Handle with TLS and attestation
            handle_client_connection(client_fd, &ssl_config);
        } while (keep_server_up);
        ret = 0;
    } while (false);

    // Cleanup
    if (server_fd >= 0) {
        ocall_close(&close_ret, server_fd);
    }

    // Cleanup mbedTLS structures
    mbedtls_ssl_cache_free(&cache);
    mbedtls_pk_free(&pkey);
    mbedtls_x509_crt_free(&server_cert);
    mbedtls_ssl_config_free(&ssl_config);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    printf(LOG_PREFIX_TLS_SERVER "Server shutdown complete\n");
    return ret;
}
