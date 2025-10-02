
#include "backoff_manager.hpp"
#include "util.hpp"
#include <algorithm>
#include <cmath>

BackoffManager::BackoffManager(double base_delay_seconds, double max_delay_seconds, double multiplier)
    : base_delay_seconds_(base_delay_seconds),
      max_delay_seconds_(max_delay_seconds),
      multiplier_(multiplier) {
}

void BackoffManager::record_failure(const std::string& endpoint) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto& state = states_[endpoint];
    state.failure_count++;
    state.last_failure = std::chrono::steady_clock::now();
    state.current_delay = calculate_delay(state.failure_count);
}

void BackoffManager::record_success(const std::string& endpoint) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = states_.find(endpoint);
    if (it != states_.end()) {
        it->second.failure_count = 0;
        it->second.current_delay = std::chrono::milliseconds(0);
    }
}

std::chrono::milliseconds BackoffManager::get_delay(const std::string& endpoint) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = states_.find(endpoint);
    if (it == states_.end()) {
        return std::chrono::milliseconds(0);
    }
    
    return it->second.current_delay;
}

bool BackoffManager::should_wait(const std::string& endpoint) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = states_.find(endpoint);
    if (it == states_.end() || it->second.failure_count == 0) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - it->second.last_failure);
    
    return elapsed < it->second.current_delay;
}

std::chrono::milliseconds BackoffManager::time_until_allowed(const std::string& endpoint) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = states_.find(endpoint);
    if (it == states_.end() || it->second.failure_count == 0) {
        return std::chrono::milliseconds(0);
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - it->second.last_failure);
    
    if (elapsed >= it->second.current_delay) {
        return std::chrono::milliseconds(0);
    }
    
    return it->second.current_delay - elapsed;
}

void BackoffManager::reset_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    states_.clear();
}

std::chrono::milliseconds BackoffManager::calculate_delay(int failure_count) {
    if (failure_count <= 0) {
        return std::chrono::milliseconds(0);
    }
    
    // Exponential backoff with jitter
    double delay_seconds = base_delay_seconds_ * std::pow(multiplier_, failure_count - 1);
    delay_seconds = std::min(delay_seconds, max_delay_seconds_);
    
    // Add jitter (±10%)
    delay_seconds = util::random_jitter(delay_seconds, 0.1);
    
    return std::chrono::milliseconds(static_cast<int>(delay_seconds * 1000));
}
