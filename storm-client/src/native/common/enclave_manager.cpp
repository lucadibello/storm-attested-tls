#include "enclave_manager.h"
#include <openenclave/host.h>
#include <cstdio>

namespace oe_common {

// Note: This uses the generic oe_create_enclave which doesn't register OCALLs.
// For enclaves with OCALLs, use the enclave-specific constructor instead
// (e.g., oe_create_atls_server_enclave from the generated header).
oe_enclave_t* EnclaveManager::create_enclave(
    const std::string& enclave_path,
    uint32_t flags) {

    oe_enclave_t* enclave = nullptr;

    // This is a simplified version for enclaves without OCALLs
    // For enclaves with OCALLs, the host should call the generated
    // enclave-specific constructor directly
    fprintf(stderr, "Warning: EnclaveManager::create_enclave() uses generic constructor.\n");
    fprintf(stderr, "For enclaves with OCALLs, use the enclave-specific constructor.\n");

    oe_result_t result = oe_create_enclave(
        enclave_path.c_str(),
        OE_ENCLAVE_TYPE_AUTO,
        flags,
        nullptr,    // settings
        0,          // settings_count
        nullptr,    // ocall_table
        0,          // ocall_table_size
        nullptr,    // ecall_info_table
        0,          // ecall_info_table_size
        &enclave);

    if (result != OE_OK) {
        fprintf(stderr, "Failed to create enclave: %s (code: %d)\n",
                oe_result_str(result), result);
        return nullptr;
    }

    return enclave;
}

bool EnclaveManager::destroy_enclave(oe_enclave_t* enclave) {
    if (!enclave) {
        return false;
    }

    oe_result_t result = oe_terminate_enclave(enclave);

    if (result != OE_OK) {
        fprintf(stderr, "Failed to terminate enclave: %s (code: %d)\n",
                oe_result_str(result), result);
        return false;
    }

    return true;
}

} // namespace oe_common

