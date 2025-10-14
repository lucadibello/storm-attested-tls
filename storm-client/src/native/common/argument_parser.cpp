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
