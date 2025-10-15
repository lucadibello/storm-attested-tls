#include "oe_utility.h"

#include <cstdio>
#include <cstring>
#include <openenclave/enclave.h>
#include <openenclave/bits/asym_keys.h>

namespace oe_common {

oe_result_t generate_key_pair(
    uint8_t** public_key,
    size_t* public_key_size,
    uint8_t** private_key,
    size_t* private_key_size) {
    oe_asymmetric_key_params_t params;

    // FIXME: hardcoded user data for key derivation. In production, this
    // should be provided by the enclave application!
    char user_data[] = "test user data!";
    constexpr size_t user_data_size = sizeof(user_data) - 1;

    // Call oe_get_public_key_by_policy() to generate key pair derived from an
    // enclave's seal key If an enclave does not want to have this key pair tied
    // to enclave instance, it can generate its own key pair using any chosen
    // crypto API

    params.type = OE_ASYMMETRIC_KEY_EC_SECP256P1; // MBEDTLS_ECP_DP_SECP256R1
    params.format = OE_ASYMMETRIC_KEY_PEM;
    params.user_data = user_data;
    params.user_data_size = user_data_size;
    oe_result_t result = oe_get_public_key_by_policy(
        OE_SEAL_POLICY_UNIQUE,
        &params,
        public_key,
        public_key_size,
        nullptr,
        nullptr);
    if (result != OE_OK)
    {
        std::printf(
            "oe_get_public_key_by_policy(OE_SEAL_POLICY_UNIQUE) = %s",
            oe_result_str(result));
        return result;
    }

    result = oe_get_private_key_by_policy(
        OE_SEAL_POLICY_UNIQUE,
        &params,
        private_key,
        private_key_size,
        nullptr,
        nullptr);
    if (result != OE_OK)
    {
        printf(
            "oe_get_private_key_by_policy(OE_SEAL_POLICY_UNIQUE) = %s",
            oe_result_str(result));
        return result;
    }

    return result;
}

bool verify_signer_id(
        const char* signing_public_key_buf,
        const size_t signing_public_key_buf_size,
        const uint8_t* signer_id_buf,
        const size_t signer_id_buf_size)
{
    printf("\nverify connecting client's identity\n");

    uint8_t signer[OE_SIGNER_ID_SIZE];
    size_t signer_size = sizeof(signer);
    if (oe_sgx_get_signer_id_from_public_key(
            signing_public_key_buf,
            signing_public_key_buf_size,
            signer,
            &signer_size) != OE_OK)
    {
        printf("oe_sgx_get_signer_id_from_public_key failed\n");
        return false;
    }
    if (std::memcmp(signer, signer_id_buf, signer_id_buf_size) != 0)
    {
        printf("mrsigner is not equal!\n");
        for (size_t i = 0; i < signer_id_buf_size; i++)
        {
            printf(
                "0x%x - 0x%x\n", static_cast<uint8_t>(signer[i]), static_cast<uint8_t>(signer_id_buf[i]));
        }
        return false;
    }
    return true;
}
}
