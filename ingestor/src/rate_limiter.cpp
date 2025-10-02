#include "rate_limiter.hpp"
#include <algorithm>

RateLimiter::RateLimiter(int requests_per_second, int burst_capacity)
    : default_requests_per_second_(requests_per_second),
      default_burst_capacity_(burst_capacity) {
}

bool RateLimiter::allow_request(const std::string& endpoint) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = buckets_.find(endpoint);
    if (it == buckets_.end()) {
        // Create new bucket for this endpoint
        buckets_.emplace(endpoint, TokenBucket(default_burst_capacity_, default_requests_per_second_));
        it = buckets_.find(endpoint);
    }
    
    refill_bucket(it->second);
    
    if (it->second.tokens >= 1.0) {
        it->second.tokens -= 1.0;
        return true;
    }
    
    return false;
}

std::chrono::milliseconds RateLimiter::time_until_allowed(const std::string& endpoint) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = buckets_.find(endpoint);
    if (it == buckets_.end()) {
        return std::chrono::milliseconds(0);
    }
    
    refill_bucket(it->second);
    
    if (it->second.tokens >= 1.0) {
        return std::chrono::milliseconds(0);
    }
    
    // Calculate time needed to accumulate 1 token
    double tokens_needed = 1.0 - it->second.tokens;
    double seconds_needed = tokens_needed / it->second.refill_rate;
    
    return std::chrono::milliseconds(static_cast<int>(seconds_needed * 1000));
}

void RateLimiter::set_endpoint_limit(const std::string& endpoint, int requests_per_second, int burst_capacity) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = buckets_.find(endpoint);
    if (it != buckets_.end()) {
        it->second.capacity = burst_capacity;
        it->second.refill_rate = static_cast<double>(requests_per_second);
        it->second.tokens = std::min(it->second.tokens, static_cast<double>(burst_capacity));
    } else {
        buckets_.emplace(endpoint, TokenBucket(burst_capacity, requests_per_second));
    }
}

void RateLimiter::reset_limits() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& [endpoint, bucket] : buckets_) {
        bucket.tokens = bucket.capacity;
        bucket.last_refill = std::chrono::steady_clock::now();
    }
}

void RateLimiter::refill_bucket(TokenBucket& bucket) {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::duration<double>>(now - bucket.last_refill).count();
    
    bucket.tokens += duration * bucket.refill_rate;
    
    if (bucket.tokens > bucket.capacity) {
        bucket.tokens = bucket.capacity;
    }
    
    bucket.last_refill = now;
}