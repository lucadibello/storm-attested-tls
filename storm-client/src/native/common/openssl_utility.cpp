#include "openssl_utility.h"
#include "oe_utility.h"

#include <cstdio>
#include <cstring>
#include <openenclave/attestation/attester.h>
#include <openenclave/attestation/sgx/evidence.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/bio.h>

// SGX Remote Attestation UUID
static oe_uuid_t uuid_sgx_ecdsa = {OE_FORMAT_UUID_SGX_ECDSA};

#define LOG_PREFIX_OPENSSL_UTILITY "[openssl_utility] "

namespace openssl_utility {

oe_result_t generate_certificate_and_pkey(
    X509** certificate,
    EVP_PKEY** pkey,
    const char* subject_name) {

    constexpr unsigned char subject_name_override[] = "CN=Open Enclave SDK,O=OESDK TLS,C=US";

    uint8_t* output_certificate = nullptr;
    size_t output_certificate_size = 0;
    uint8_t* private_key_buffer = nullptr;
    size_t private_key_buffer_size = 0;
    uint8_t* public_key_buffer = nullptr;
    size_t public_key_buffer_size = 0;
    BIO* bio = nullptr;
    oe_result_t result = OE_FAILURE;
    uint8_t* optional_parameters = nullptr;
    size_t optional_parameters_size = 0;
    const unsigned char* cert_buf = nullptr;


    printf(LOG_PREFIX_OPENSSL_UTILITY "Generating key pair\n");

    result = oe_common::generate_key_pair(
        &public_key_buffer,
        &public_key_buffer_size,
        &private_key_buffer,
        &private_key_buffer_size);

    if (result != OE_OK) {
        printf(LOG_PREFIX_OPENSSL_UTILITY "generate_key_pair failed with %s\n", oe_result_str(result));
        goto cleanup;
    }

    // No optional parameters for now
    printf(LOG_PREFIX_OPENSSL_UTILITY "Generating certificate with attestation evidence\n");
    oe_attester_initialize();
    printf("-- attester initialized");

    result = oe_get_attestation_certificate_with_evidence_v2(
        &uuid_sgx_ecdsa,
        subject_name_override,
        private_key_buffer,
        private_key_buffer_size,
        public_key_buffer,
        public_key_buffer_size,
        optional_parameters,
        optional_parameters_size,
        &output_certificate,
        &output_certificate_size);

    if (result != OE_OK) {
        printf(LOG_PREFIX_OPENSSL_UTILITY "oe_get_attestation_certificate_with_evidence_v2 failed with %s\n",
               oe_result_str(result));
        oe_attester_shutdown();
        goto cleanup;
    }

    printf(LOG_PREFIX_OPENSSL_UTILITY "Generated certificate with size %zu bytes\n", output_certificate_size);

    // Parse certificate using OpenSSL
    cert_buf = output_certificate;
    *certificate = d2i_X509(nullptr, &cert_buf, output_certificate_size);
    if (*certificate == nullptr) {
        printf(LOG_PREFIX_OPENSSL_UTILITY "d2i_X509 failed\n");
        ERR_print_errors_fp(stderr);
        result = OE_FAILURE;
        oe_attester_shutdown();
        goto cleanup;
    }

    // Parse private key using OpenSSL
    bio = BIO_new_mem_buf(private_key_buffer, private_key_buffer_size);
    if (bio == nullptr) {
        printf(LOG_PREFIX_OPENSSL_UTILITY "BIO_new_mem_buf failed\n");
        result = OE_FAILURE;
        oe_attester_shutdown();
        goto cleanup;
    }

    *pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);

    if (*pkey == nullptr) {
        printf(LOG_PREFIX_OPENSSL_UTILITY "PEM_read_bio_PrivateKey failed\n");
        ERR_print_errors_fp(stderr);
        result = OE_FAILURE;
        oe_attester_shutdown();
        goto cleanup;
    }

    printf(LOG_PREFIX_OPENSSL_UTILITY "Successfully parsed certificate and private key\n");
    result = OE_OK;
    oe_attester_shutdown();

cleanup:
    if (bio != nullptr) {
        BIO_free(bio);
    }
    oe_free_key(private_key_buffer, private_key_buffer_size, nullptr, 0);
    oe_free_key(public_key_buffer, public_key_buffer_size, nullptr, 0);
    oe_free_attestation_certificate(output_certificate);

    return result;
}

oe_result_t initialize_ssl_context(SSL_CTX* ctx) {
    if (ctx == nullptr) {
        return OE_INVALID_PARAMETER;
    }

    // Set minimum TLS version to 1.2
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);

    // Set cipher suites (prefer forward secrecy)
    if (!SSL_CTX_set_cipher_list(ctx, "HIGH:!aNULL:!MD5:!RC4")) {
        printf(LOG_PREFIX_OPENSSL_UTILITY "Failed to set cipher list\n");
        return OE_FAILURE;
    }

    return OE_OK;
}

int read_from_session_peer(
    SSL* ssl_session,
    const char* expected_data,
    size_t expected_data_len) {

    if (ssl_session == nullptr || expected_data == nullptr) {
        return -1;
    }

    char buffer[1024];
    int bytes_read = SSL_read(ssl_session, buffer, sizeof(buffer) - 1);

    if (bytes_read <= 0) {
        int ssl_error = SSL_get_error(ssl_session, bytes_read);
        printf(TLS_SERVER "SSL_read failed with error: %d\n", ssl_error);
        return -1;
    }

    buffer[bytes_read] = '\0';
    printf(TLS_SERVER "Received %d bytes: %s\n", bytes_read, buffer);

    // Verify expected data if provided
    if (expected_data_len > 0) {
        if (bytes_read != static_cast<int>(expected_data_len) ||
            memcmp(buffer, expected_data, expected_data_len) != 0) {
            printf(TLS_SERVER "Unexpected data received\n");
            return -1;
        }
    }

    return 0;
}

int write_to_session_peer(
    SSL* ssl_session,
    const char* data,
    size_t data_len) {

    if (ssl_session == nullptr || data == nullptr) {
        return -1;
    }

    int bytes_written = SSL_write(ssl_session, data, data_len);

    if (bytes_written <= 0) {
        int ssl_error = SSL_get_error(ssl_session, bytes_written);
        printf(TLS_SERVER "SSL_write failed with error: %d\n", ssl_error);
        return -1;
    }

    printf(TLS_SERVER "Sent %d bytes: %.*s\n", bytes_written, bytes_written, data);
    return 0;
}

} // namespace openssl_utility
