//
// Created by luca on 10/14/25.
//

#ifndef OE_ATTESTED_TLS_SERVER_MBEDTLS_UTILITY_H
#define OE_ATTESTED_TLS_SERVER_MBEDTLS_UTILITY_H

#include <openenclave/3rdparty/mbedtls/x509_crt.h>
#include <openenclave/enclave.h>

namespace mbedtls_utility {
    oe_result_t generate_certificate_and_pkey(
        mbedtls_x509_crt* certificate,
        mbedtls_pk_context* private_key,
        const unsigned char *certificate_subject_name);

    oe_result_t generate_certificate_and_pkey_v2(
        mbedtls_x509_crt* certificate,
        mbedtls_pk_context* private_key,
        const unsigned char *certificate_subject_name);
} // namespace mbedtls_utility

#endif //OE_ATTESTED_TLS_SERVER_MBEDTLS_UTILITY_H