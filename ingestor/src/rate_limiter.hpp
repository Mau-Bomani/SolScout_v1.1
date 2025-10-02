
#pragma once
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

class RateLimiter {
public:
    RateLimiter(int requests_per_second = 10, int burst_capacity = 20);
    
    // Check if request is allowed for given endpoint
    bool allow_request(const std::string& endpoint = "default");
    
    // Get time until next request is allowed
    std::chrono::milliseconds time_until_allowed(const std::string& endpoint = "default");
    
    // Update rate limits for specific endpoint
    void set_endpoint_limit(const std::string& endpoint, int requests_per_second, int burst_capacity);
    
    // Reset all rate limits (useful after successful requests)
    void reset_limits();

private:
    struct TokenBucket {
        double tokens;
        int capacity;
        double refill_rate; // tokens per second
        std::chrono::steady_clock::time_point last_refill;
        
        TokenBucket(int cap, double rate) 
            : tokens(cap), capacity(cap), refill_rate(rate), 
              last_refill(std::chrono::steady_clock::now()) {}
    };
    
    void refill_bucket(TokenBucket& bucket);
    
    std::mutex mutex_;
    std::unordered_map<std::string, TokenBucket> buckets_;
    int default_requests_per_second_;
    int default_burst_capacity_;
};
