#ifndef OE_ATTESTED_TLS_SERVER_OPENSSL_UTILITY_H
#define OE_ATTESTED_TLS_SERVER_OPENSSL_UTILITY_H

#include <openenclave/enclave.h>
#include <openssl/ssl.h>
#include "utility.h"

int read_from_session_peer(
    SSL*& ssl_session,
    const char* payload,
    size_t payload_length);

int write_to_session_peer(
    SSL*& ssl_session,
    const char* payload,
    size_t payload_length);

oe_result_t load_tls_certificates_and_keys(
    SSL_CTX* ctx,
    X509*& certificate,
    EVP_PKEY*& pkey);

oe_result_t initalize_ssl_context(SSL_CONF_CTX*& ssl_conf_ctx, SSL_CTX*& ctx);

#endif //OE_ATTESTED_TLS_SERVER_OPENSSL_UTILITY_H