
#pragma once
#include <string>

namespace util {
    // Time utilities
    std::string current_iso8601();
    
    // UUID generation
    std::string generate_uuid();
    
    // Address validation
    bool is_valid_address(const std::string& address);
}
