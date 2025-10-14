// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#include "argument_parser.h"
#include <algorithm>
#include <cstring>

namespace oe_common {

ArgumentParser::ArgumentParser(int argc, const char* argv[]) : argc_(argc) {
    for (int i = 0; i < argc; ++i) {
        argv_.push_back(argv[i]);
    }
}

bool ArgumentParser::check_and_remove_flag(const std::string& flag) {
    auto it = std::find(argv_.begin(), argv_.end(), flag);
    if (it != argv_.end()) {
        argv_.erase(it);
        argc_--;
        return true;
    }
    return false;
}

std::string ArgumentParser::parse_value_argument(const std::string& prefix) {
    for (const auto& arg : argv_) {
        if (arg.find(prefix) == 0) {
            return arg.substr(prefix.length());
        }
    }
    return "";
}

std::string ArgumentParser::get_arg(int index) const {
    if (index >= 0 && index < static_cast<int>(argv_.size())) {
        return argv_[index];
    }
    return "";
}

} // namespace oe_common
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

