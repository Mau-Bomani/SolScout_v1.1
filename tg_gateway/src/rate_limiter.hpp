
#pragma once
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <deque>

class RateLimiter {
public:
    RateLimiter(int msgs_per_min, int actionable_per_hour);
    
    bool check_user_rate_limit(int64_t user_id);
    bool check_global_actionable_limit();
    void record_actionable();
    void cleanup_old_entries();
    
private:
    const int msgs_per_min_;
    const int actionable_per_hour_;
    
    std::unordered_map<int64_t, std::deque<std::chrono::system_clock::time_point>> user_messages_;
    std::deque<std::chrono::system_clock::time_point> actionable_alerts_;
    std::mutex rate_limit_mutex_;
    std::mutex actionable_mutex_;
};
