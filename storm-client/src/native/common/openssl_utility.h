#ifndef OE_ATTESTED_TLS_SERVER_OPENSSL_UTILITY_H
#define OE_ATTESTED_TLS_SERVER_OPENSSL_UTILITY_H

#include <openenclave/enclave.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/evp.h>

// Logging prefix
#define TLS_SERVER "[TLS Server] "

// Test payloads for client-server communication
#define CLIENT_PAYLOAD "Hello from client"
#define CLIENT_PAYLOAD_SIZE (sizeof(CLIENT_PAYLOAD) - 1)
#define SERVER_PAYLOAD "Hello from server"

namespace openssl_utility {

/**
 * Generate an attested certificate and private key using OpenSSL
 */
oe_result_t generate_certificate_and_pkey(
    X509** certificate,
    EVP_PKEY** pkey,
    const char* subject_name);

/**
 * Initialize SSL context with proper settings
 */
oe_result_t initialize_ssl_context(SSL_CTX* ctx);

/**
 * Read data from SSL session peer
 */
int read_from_session_peer(
    SSL* ssl_session,
    const char* expected_data,
    size_t expected_data_len);

/**
 * Write data to SSL session peer
 */
int write_to_session_peer(
    SSL* ssl_session,
    const char* data,
    size_t data_len);

} // namespace openssl_utility

#endif // OE_ATTESTED_TLS_SERVER_OPENSSL_UTILITY_H

