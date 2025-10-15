#include "mbedtls_utility.h"

#include <cstdio>
#include <oe_utility.h>
#include <openenclave/attestation/attester.h>
#include <openenclave/attestation/sgx/evidence.h>

// SGX Remote Attestation UUID.
static oe_uuid_t uuid_sgx_ecdsa = {OE_FORMAT_UUID_SGX_ECDSA};

#define LOG_PREFIX_MBEDTLS_UTILITY "[mbedtls_utility] "

namespace mbedtls_utility {
    oe_result_t generate_certificate_and_pkey(
        mbedtls_x509_crt *certificate,
        mbedtls_pk_context *private_key,
        const unsigned char *certificate_subject_name) {
        uint8_t *output_certificate = nullptr;
        size_t output_certificate_size = 0;
        uint8_t *private_key_buffer = nullptr;
        size_t private_key_buffer_size = 0;
        uint8_t *public_key_buffer = nullptr;
        size_t public_key_buffer_size = 0;

        printf(LOG_PREFIX_MBEDTLS_UTILITY "Generating key pair\n");

        oe_result_t result = oe_common::generate_key_pair(
            &public_key_buffer,
            &public_key_buffer_size,
            &private_key_buffer,
            &private_key_buffer_size);
        if (result != OE_OK) {
            printf(LOG_PREFIX_MBEDTLS_UTILITY "generate_key_pair failed with %s\n", oe_result_str(result));
            return result;
        }
        // NOTE: currently we don't use any additional parameter. Kept here for easier customization in the future
        uint8_t* optional_parameters = nullptr;
        size_t optional_parameters_size = 0;

        // print public key buffer contents (as string!)
        printf(LOG_PREFIX_MBEDTLS_UTILITY "Generated public key:\n%.*s\n",
               static_cast<int>(public_key_buffer_size),
               reinterpret_cast<const char *>(public_key_buffer));

        // both ec key such ASYMMETRIC_KEY_EC_SECP256P1 or RSA key work
        printf(LOG_PREFIX_MBEDTLS_UTILITY "generating certificate using derived key pair\n");
        oe_attester_initialize();
        result = oe_get_attestation_certificate_with_evidence_v2(
            &uuid_sgx_ecdsa,
            certificate_subject_name,
            private_key_buffer,
            private_key_buffer_size,
            public_key_buffer,
            public_key_buffer_size,
            optional_parameters,
            optional_parameters_size,
            &output_certificate,
            &output_certificate_size);
        if (result != OE_OK) {
            printf(
                LOG_PREFIX_MBEDTLS_UTILITY "oe_get_attestation_certificate_with_evidence_v2 failed with %s\n",
                oe_result_str(result));
            return result;
        }
        printf(LOG_PREFIX_MBEDTLS_UTILITY "Done. Generated certificate with size %zu bytes\n",
               output_certificate_size);

        // create mbedtls_x509_crt from output_cert
        int ret = mbedtls_x509_crt_parse_der(
            certificate, output_certificate, output_certificate_size);
        if (ret != 0) {
            printf(LOG_PREFIX_MBEDTLS_UTILITY "mbedtls_x509_crt_parse_der failed with ret = %d\n", ret);
            result = OE_FAILURE;
        } else {
            // create mbedtls_pk_context from private key data
            ret = mbedtls_pk_parse_key(
                private_key,
                private_key_buffer,
                private_key_buffer_size,
                nullptr,
                0);
            if (ret != 0) {
                printf(LOG_PREFIX_MBEDTLS_UTILITY "mbedtls_pk_parse_key failed with ret = %d\n", ret);
                result = OE_FAILURE;
            }
        }

        oe_attester_shutdown();

        // Cleanup
        oe_free_key(private_key_buffer, private_key_buffer_size, nullptr, 0);
        oe_free_key(public_key_buffer, public_key_buffer_size, nullptr, 0);
        oe_free_attestation_certificate(output_certificate);
        return result;
    }
} // namespace mbedtls_utility
