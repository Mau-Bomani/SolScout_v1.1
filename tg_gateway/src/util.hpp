
#pragma once
#include <string>

namespace util {
    void setup_logging(const std::string& level);
    std::string generate_uuid();
    std::string generate_pin();
    std::string current_iso8601();
    bool is_valid_address(const std::string& address);
}
