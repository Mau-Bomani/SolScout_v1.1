
#include "util.hpp"
#include <cstdlib>
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <regex>
#include <random>
#include <cmath>
#include <iomanip>
#include <chrono>

namespace util {

std::string get_env_var(const std::string& name, const std::string& default_value) {
    const char* value = std::getenv(name.c_str());
    return value ? std::string(value) : default_value;
}

std::string get_required_env_var(const std::string& name) {
    const char* value = std::getenv(name.c_str());
    if (!value || std::string(value).empty()) {
        throw std::runtime_error("Required environment variable " + name + " is not set");
    }
    return std::string(value);
}

std::vector<std::string> split_string(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    
    return tokens;
}

std::string trim(const std::string& str) {
    auto start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    
    auto end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

bool starts_with(const std::string& str, const std::string& prefix) {
    return str.length() >= prefix.length() && 
           str.compare(0, prefix.length(), prefix) == 0;
}

bool ends_with(const std::string& str, const std::string& suffix) {
    return str.length() >= suffix.length() && 
           str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

std::string current_iso8601() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return ss.str();
}

std::chrono::system_clock::time_point parse_iso8601(const std::string& iso_string) {
    std::tm tm = {};
    std::istringstream ss(iso_string);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    
    if (ss.fail()) {
        throw std::runtime_error("Failed to parse ISO8601 timestamp: " + iso_string);
    }
    
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

std::string format_timestamp(const std::chrono::system_clock::time_point& tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch() % std::chrono::seconds(1)).count();
    
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms << 'Z';
    
    return ss.str();
}

std::chrono::system_clock::time_point round_to_interval(
    const std::chrono::system_clock::time_point& tp, 
    int interval_minutes
) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    auto tm = *std::gmtime(&time_t);
    
    // Round down to the nearest interval
    int rounded_minutes = (tm.tm_min / interval_minutes) * interval_minutes;
    tm.tm_min = rounded_minutes;
    tm.tm_sec = 0;
    
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

bool is_valid_solana_address(const std::string& address) {
    if (address.length() < 32 || address.length() > 44) {
        return false;
    }
    
    // Check for valid base58 characters
    std::regex base58_regex("^[1-9A-HJ-NP-Za-km-z]+$");
    return std::regex_match(address, base58_regex);
}

bool is_valid_pool_id(const std::string& pool_id) {
    return is_valid_solana_address(pool_id);
}

double safe_parse_double(const std::string& str, double default_value) {
    try {
        return std::stod(str);
    } catch (const std::exception&) {
        return default_value;
    }
}

double calculate_price_impact(double reserve_x, double reserve_y, double trade_amount) {
    if (reserve_x <= 0 || reserve_y <= 0 || trade_amount <= 0) {
        return 0.0;
    }
    
    // Constant product formula: x * y = k
    double k = reserve_x * reserve_y;
    double new_reserve_x = reserve_x + trade_amount;
    double new_reserve_y = k / new_reserve_x;
    double output_amount = reserve_y - new_reserve_y;
    
    // Price impact = (expected_output - actual_output) / expected_output
    double expected_output = trade_amount * (reserve_y / reserve_x);
    if (expected_output <= 0) return 0.0;
    
    double price_impact = (expected_output - output_amount) / expected_output;
    return std::max(0.0, std::min(1.0, price_impact)); // Clamp between 0 and 1
}

double calculate_k_constant(double reserve_x, double reserve_y) {
    return reserve_x * reserve_y;
}

std::pair<double, double> calculate_output_amount(
    double input_amount, 
    double input_reserve, 
    double output_reserve,
    double fee_rate
) {
    if (input_amount <= 0 || input_reserve <= 0 || output_reserve <= 0) {
        return {0.0, 0.0};
    }
    
    // Apply fee
    double input_after_fee = input_amount * (1.0 - fee_rate);
    
    // Constant product formula
    double k = input_reserve * output_reserve;
    double new_input_reserve = input_reserve + input_after_fee;
    double new_output_reserve = k / new_input_reserve;
    double output_amount = output_reserve - new_output_reserve;
    
    // Calculate price impact
    double price_impact = calculate_price_impact(input_reserve, output_reserve, input_amount);
    
    return {output_amount, price_impact};
}

std::string generate_uuid() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);
    
    std::stringstream ss;
    ss << std::hex;
    
    for (int i = 0; i < 8; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 4; i++) ss << dis(gen);
    ss << "-4";
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-" << dis2(gen);
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 12; i++) ss << dis(gen);
    
    return ss.str();
}

double random_jitter(double base_value, double jitter_factor) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(-jitter_factor, jitter_factor);
    
    double jitter = dis(gen);
    return base_value * (1.0 + jitter);
}

bool is_network_error(int http_status) {
    return http_status == 0 || // Connection failed
           http_status == 408 || // Request timeout
           http_status == 429 || // Too many requests
           http_status == 502 || // Bad gateway
           http_status == 503 || // Service unavailable
           http_status == 504;   // Gateway timeout
}

bool should_retry_request(int http_status, int attempt_count) {
    if (attempt_count >= 3) return false;
    
    return is_network_error(http_status) || 
           (http_status >= 500 && http_status < 600);
}

} // namespace util
