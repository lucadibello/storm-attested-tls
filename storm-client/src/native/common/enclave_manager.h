// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#pragma once

#include <openenclave/host.h>
#include <string>

namespace oe_common {

/**
 * Enclave manager handles creation, destruction, and management of enclaves.
 * This can be used by any host application that needs to work with enclaves.
 */
class EnclaveManager {
public:
    /**
     * Create and load an enclave from a signed enclave file.
     *
     * @param enclave_path Path to the signed enclave binary
     * @param flags Enclave flags (e.g., OE_ENCLAVE_FLAG_DEBUG, OE_ENCLAVE_FLAG_SIMULATE)
     * @return Pointer to the created enclave, or nullptr on failure
     */
    static oe_enclave_t* create_enclave(
        const std::string& enclave_path,
        uint32_t flags = OE_ENCLAVE_FLAG_DEBUG);

    /**
     * Terminate and destroy an enclave.
     *
     * @param enclave Pointer to the enclave to destroy
     * @return true if successful, false otherwise
     */
    static bool destroy_enclave(oe_enclave_t* enclave);

    /**
     * Check if an enclave pointer is valid.
     *
     * @param enclave Pointer to check
     * @return true if valid, false otherwise
     */
    static bool is_valid(oe_enclave_t* enclave) {
        return enclave != nullptr;
    }
};

} // namespace oe_common

