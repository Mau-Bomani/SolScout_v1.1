
#pragma once
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

class BackoffManager {
public:
    BackoffManager(double base_delay_seconds = 1.0, double max_delay_seconds = 300.0, double multiplier = 2.0);
    
    // Record a failure for an endpoint
    void record_failure(const std::string& endpoint);
    
    // Record a success for an endpoint (resets backoff)
    void record_success(const std::string& endpoint);
    
    // Get current delay for an endpoint
    std::chrono::milliseconds get_delay(const std::string& endpoint);
    
    // Check if we should wait before making a request
    bool should_wait(const std::string& endpoint);
    
    // Get time until next request is allowed
    std::chrono::milliseconds time_until_allowed(const std::string& endpoint);
    
    // Reset all backoff states
    void reset_all();

private:
    struct BackoffState {
        int failure_count = 0;
        std::chrono::steady_clock::time_point last_failure;
        std::chrono::milliseconds current_delay{0};
        
        BackoffState() : last_failure(std::chrono::steady_clock::now()) {}
    };
    
    std::chrono::milliseconds calculate_delay(int failure_count);
    
    std::mutex mutex_;
    std::unordered_map<std::string, BackoffState> states_;
    double base_delay_seconds_;
    double max_delay_seconds_;
    double multiplier_;
};
