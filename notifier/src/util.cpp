
#include "util.hpp"
#include <iomanip>
#include <sstream>
#include <functional>
#include <numeric>

std::string format_iso8601(const std::chrono::system_clock::time_point& tp) {
    auto tt = std::chrono::system_clock::to_time_t(tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&tt), "%Y-%m-%dT%H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return ss.str();
}

std::chrono::system_clock::time_point parse_iso8601(const std::string& iso_string) {
    std::tm tm = {};
    std::stringstream ss(iso_string);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    
    // Handle fractional seconds
    char dot;
    int fractional_seconds = 0;
    if (ss >> dot && dot == '.') {
        ss >> fractional_seconds;
    }
    
    auto time = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    time += std::chrono::milliseconds(fractional_seconds);
    
    return time;
}

std::string generate_reason_hash(const std::vector<std::string>& reasons) {
    std::string combined;
    for (const auto& reason : reasons) {
        combined += reason;
    }
    std::hash<std::string> hasher;
    return std::to_string(hasher(combined));
}
