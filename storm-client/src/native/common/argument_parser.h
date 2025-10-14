// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#pragma once

#include <string>
#include <vector>

namespace oe_common {

/**
 * Command-line argument parser utility.
 * Helps parse common arguments like --simulate, -port:, etc.
 */
class ArgumentParser {
public:
    ArgumentParser(int argc, const char* argv[]);

    /**
     * Check if a flag is present and remove it from the argument list.
     * @param flag The flag to check for (e.g., "--simulate")
     * @return true if flag was present, false otherwise
     */
    bool check_and_remove_flag(const std::string& flag);

    /**
     * Parse a key:value argument (e.g., "-port:8443")
     * @param prefix The prefix to look for (e.g., "-port:")
     * @return The value part, or empty string if not found
     */
    std::string parse_value_argument(const std::string& prefix);

    /**
     * Get the remaining argument count.
     */
    int get_argc() const { return argc_; }

    /**
     * Get the remaining arguments.
     */
    const std::vector<std::string>& get_argv() const { return argv_; }

    /**
     * Get a specific argument by index.
     * @param index The index of the argument
     * @return The argument string, or empty if index is out of bounds
     */
    std::string get_arg(int index) const;

private:
    int argc_;
    std::vector<std::string> argv_;
};

} // namespace oe_common

