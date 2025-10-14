#ifndef OE_ATTESTED_TLS_SERVER_OE_UTILITY_H
#define OE_ATTESTED_TLS_SERVER_OE_UTILITY_H

#include <openenclave/attestation/attester.h>
#include <openenclave/attestation/sgx/report.h>

namespace oe_common {

oe_result_t generate_key_pair(
    uint8_t** public_key,
    size_t* public_key_size,
    uint8_t** private_key,
    size_t* private_key_size);

bool verify_signer_id(
    const char* signing_public_key_buf,
    size_t signing_public_key_buf_size,
    const uint8_t* signer_id_buf,
    size_t signer_id_buf_size);

oe_result_t load_oe_modules();

} // namespace oe_common

#endif //OE_ATTESTED_TLS_SERVER_OE_UTILITY_H
