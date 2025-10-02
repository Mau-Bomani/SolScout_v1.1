
#pragma once

#include <string>
#include <chrono>
#include <vector>

// Format a time_point to an ISO8601 string
std::string format_iso8601(const std::chrono::system_clock::time_point& tp);

// Parse an ISO8601 string to a time_point
std::chrono::system_clock::time_point parse_iso8601(const std::string& iso_string);

// Generate a hash for a vector of strings
std::string generate_reason_hash(const std::vector<std::string>& reasons);
