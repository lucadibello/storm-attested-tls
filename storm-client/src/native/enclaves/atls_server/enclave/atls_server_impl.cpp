#include <cassert>
#include <openenclave/enclave.h>
#include <openenclave/tracee.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/ssl_cache.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/platform.h>
#include <sys/socket.h>

#include <cstdio>
#include <cstring>
#include <mbedtls_utility.h>
#include <oe_enclave_utility.h>
#include <openenclave/log.h>
#include <openenclave/attestation/verifier.h>

#include "atls_server_t.h"
#include "consts.h"

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

    printf(LOG_PREFIX_TLS_SERVER "Setting up TLS configuration\n");

    // Initialize SSL configuration
    int ret = mbedtls_ssl_config_defaults(
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

    // flush stdout to ensure all logs are printed before returning
    fflush(stdout);

    // if we reach here, everything went fine (ret == 0)
    assert((void("Somehow ret is not equal to zero. Ensure that you handled all error branches properly!"), ret == 0));
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

    /*
     * Add logic here to modify the log message, to obscure enclave logs from
     * the host. The context might be used, for example, to transfer a
     * shared/public/private secret, that may be used to encrypt the log
     * message.
     *
     * NOTE: Do not use operations based on OE's host filesystem support
     * in this function, they do not work consistently. If used, the
     * logs generated after oe_terminate_enclave() cause the program to crash
     * with a segmentation fault (issue #4349).
     */
    OE_UNUSED(context);

    /* invoke ocall to copy enclave logs to file */
    const oe_result_t result = ocall_transfer_logs_to_file(modified_log, strlen(modified_log));
    OE_UNUSED(result);
}

// ECALL to set handler
extern "C" void ecall_set_log_callback()
{
    // make sure that logs use the enclave_customized_log function
    oe_enclave_log_set_callback(nullptr, enclave_customized_log);
}

// Main ECALL: Set up and run the TLS server
extern "C" int ecall_set_up_tls_server(char* port, bool keep_server_up) {
    int ret = OE_FAILURE;
    int server_fd = -1;
    int reuse = 1;
    int setsockopt_ret = 0;
    int send_ret = 0;
    int close_ret = 0;

    // validate + cast port number to correct type
    char* endptr = nullptr;
    long port_num = strtol(port, &endptr, 10);
    if (*endptr != '\0' || port_num <= 0 || port_num > 65535) {
        fprintf(stderr, "Invalid port number: %s\n", port);
        exit(EXIT_FAILURE);
    }

    // load required oe modules
    ret = oe_enclave_utility::load_oe_modules();
    if (ret != OE_OK) {
        printf(LOG_PREFIX_TLS_SERVER "loading required Open Enclave modules failed\n");
        return ret;
    }

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl_ctx;
    mbedtls_ssl_config ssl_config;
    mbedtls_x509_crt server_cert;
    mbedtls_pk_context pkey;
    mbedtls_ssl_cache_context cache;
    mbedtls_net_context listen_fd;

    // Initialize mbedTLS structures
    mbedtls_net_init(&listen_fd);
    mbedtls_ssl_init(&ssl_ctx);
    mbedtls_ssl_config_init(&ssl_config);
    mbedtls_ssl_cache_init(&cache);
    mbedtls_x509_crt_init(&server_cert);
    mbedtls_pk_init(&pkey);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    oe_verifier_initialize(); // initialize verifier to validate client certificates!

    // NOTE: this do-while(false) block is used to have a structured error handling
    // This is done to guarantee the cleanup without GOTO statements.
    do {
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
        printf(LOG_PREFIX_TLS_SERVER "Generating attested certificate...\n");

        // FIXME: this has to be changed in the future. Now hard-coded.
        constexpr unsigned char certificate_subject_name[] =
            "CN=Attested TLS Server,"
            "O=Università della Svizzera italiana (USI),"
            "OU=Faculty of Informatics,"
            "L=Lugano,"
            "ST=Ticino,"
            "C=CH";
        ret = mbedtls_utility::generate_certificate_and_pkey(&server_cert, &pkey, certificate_subject_name);
        printf(LOG_PREFIX_TLS_SERVER "Generated certificate and pkey. Result: %d\n", ret);
        if (ret != OE_OK) {
            printf(LOG_PREFIX_TLS_SERVER "Failed to generate attested certificate. Error: %s\n", oe_result_str(static_cast<oe_result_t>(ret)));
            break; // go to clean up stage
        }

        // Setup TLS configuration based on generated server certificate
        printf(LOG_PREFIX_TLS_SERVER "Configuring mBedTLS ssl config...\n");
        ret = setup_tls_config(&ssl_config, &server_cert, &pkey, &ctr_drbg, &cache);
        if (ret != OE_OK) {
            printf(LOG_PREFIX_TLS_SERVER "Failed to set up TLS configuration\n");
            break;
        }

        // TODO: accept new clients + handle connection
        bool keep_server_up = true;
        printf(LOG_PREFIX_TLS_SERVER "Creating and binding listening socket...\n");


        ret = 0; // reset any error before returning!
    } while (false);

    // free resource (if needed)
    mbedtls_net_free(&listen_fd);
    mbedtls_x509_crt_free(&server_cert);
    mbedtls_pk_free(&pkey);
    mbedtls_ssl_free(&ssl_ctx);
    mbedtls_ssl_config_free(&ssl_config);
    mbedtls_ssl_cache_free(&cache);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    oe_verifier_shutdown();

    printf(LOG_PREFIX_TLS_SERVER "Server shutdown complete\n");
    return ret;
}