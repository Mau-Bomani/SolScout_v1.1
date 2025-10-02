
#include "rate_limiter.hpp"
#include <algorithm>

RateLimiter::RateLimiter(int msgs_per_min, int global_actionable_per_hour)
    : msgs_per_min_(msgs_per_min), global_actionable_per_hour_(global_actionable_per_hour) {}

bool RateLimiter::check_user_rate_limit(int64_t user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::system_clock::now();
    auto& data = user_limits_[user_id];
    
    // Reset window if it's been more than a minute
    if (now - data.window_start > std::chrono::minutes(1)) {
        data.window_start = now;
        data.message_count = 0;
    }
    
    if (data.message_count >= msgs_per_min_) {
        return false; // Rate limit exceeded
    }
    
    data.message_count++;
    return true;
}

bool RateLimiter::check_global_actionable_limit() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::system_clock::now();
    auto hour_ago = now - std::chrono::hours(1);
    
    // Remove timestamps older than 1 hour
    actionable_timestamps_.erase(
        std::remove_if(actionable_timestamps_.begin(), actionable_timestamps_.end(),
                      [hour_ago](const auto& ts) { return ts < hour_ago; }),
        actionable_timestamps_.end());
    
    return actionable_timestamps_.size() < static_cast<size_t>(global_actionable_per_hour_);
}

void RateLimiter::record_actionable() {
    std::lock_guard<std::mutex> lock(mutex_);
    actionable_timestamps_.push_back(std::chrono::system_clock::now());
}

void RateLimiter::cleanup_old_entries() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::system_clock::now();
    
    // Clean up user limits older than 2 minutes
    auto it = user_limits_.begin();
    while (it != user_limits_.end()) {
        if (now - it->second.window_start > std::chrono::minutes(2)) {
            it = user_limits_.erase(it);
        } else {
            ++it;
        }
    }
    
    // Clean up actionable timestamps older than 1 hour
    auto hour_ago = now - std::chrono::hours(1);
    actionable_timestamps_.erase(
        std::remove_if(actionable_timestamps_.begin(), actionable_timestamps_.end(),
                      [hour_ago](const auto& ts) { return ts < hour_ago; }),
        actionable_timestamps_.end());
}
