
#pragma once

#include "types.hpp"
#include "config.hpp"
#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <vector>

class ThrottleManager {
public:
    explicit ThrottleManager(const Config& config);
    
    // Check if an alert should be throttled
    bool should_throttle(const std::string& mint, const std::string& band);
    
    // Record an alert for throttling purposes
    void record_alert(const std::string& mint, const std::string& band);
    
    // Clean up expired throttles
    void cleanup();

private:
    struct AlertRecord {
        std::string mint;
        std::string band;
        std::chrono::system_clock::time_point timestamp;
    };
    
    const Config& config_;
    std::mutex mutex_;
    std::vector<AlertRecord> alert_history_;
};
