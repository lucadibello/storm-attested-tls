#ifndef OE_ATTESTED_TLS_SERVER_MBEDTLS_UTILITY_H
#define OE_ATTESTED_TLS_SERVER_MBEDTLS_UTILITY_H

#include <mbedtls/pk.h>
#include <mbedtls/x509_crt.h>
#include <openenclave/enclave.h>
#include "utility.h"

oe_result_t generate_certificate_and_pkey(
    mbedtls_x509_crt* certificate,
    mbedtls_pk_context* private_key);

#endif //OE_ATTESTED_TLS_SERVER_MBEDTLS_UTILITY_H