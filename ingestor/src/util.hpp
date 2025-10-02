
#pragma once
#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <random>
#include <sstream>

namespace util {

// Environment variable helpers
std::string get_env_var(const std::string& name, const std::string& default_value = "");
std::string get_required_env_var(const std::string& name);

// String utilities
std::vector<std::string> split_string(const std::string& str, char delimiter);
std::string trim(const std::string& str);
bool starts_with(const std::string& str, const std::string& prefix);
bool ends_with(const std::string& str, const std::string& suffix);

// Time utilities
std::string current_iso8601();
std::chrono::system_clock::time_point parse_iso8601(const std::string& iso_string);
std::string format_timestamp(const std::chrono::system_clock::time_point& tp);
std::chrono::system_clock::time_point round_to_interval(
    const std::chrono::system_clock::time_point& tp, 
    int interval_minutes
);

// Validation utilities
bool is_valid_solana_address(const std::string& address);
bool is_valid_pool_id(const std::string& pool_id);
double safe_parse_double(const std::string& str, double default_value = 0.0);

// Math utilities
double calculate_price_impact(double reserve_x, double reserve_y, double trade_amount);
double calculate_k_constant(double reserve_x, double reserve_y);
std::pair<double, double> calculate_output_amount(
    double input_amount, 
    double input_reserve, 
    double output_reserve,
    double fee_rate = 0.003
);

// Random utilities
std::string generate_uuid();
double random_jitter(double base_value, double jitter_factor = 0.1);

// Network utilities
bool is_network_error(int http_status);
bool should_retry_request(int http_status, int attempt_count);

} // namespace util
