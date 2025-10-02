
#include "throttles.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>

ThrottleManager::ThrottleManager(const Config& config) : config_(config) {}

bool ThrottleManager::should_throttle(const std::string& mint, const std::string& band) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::system_clock::now();
    
    // Check if we've sent an alert for this mint recently
    for (const auto& record : alert_history_) {
        if (record.mint == mint) {
            auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - record.timestamp).count();
            
            // Different cooldown periods based on band
            int cooldown_minutes = 0;
            if (band == "high_conviction") {
                cooldown_minutes = config_.cooldown_high_conviction_min;
            } else if (band == "actionable") {
                cooldown_minutes = config_.cooldown_actionable_min;
            } else if (band == "heads_up") {
                cooldown_minutes = config_.cooldown_headsup_min;
            } else {
                cooldown_minutes = config_.cooldown_watch_min;
            }
            
            if (elapsed < cooldown_minutes) {
                spdlog::debug("Throttling alert for {}: {} minutes elapsed, cooldown is {} minutes",
                             mint, elapsed, cooldown_minutes);
                return true;
            }
        }
    }
    
    // Check global rate limits
    int alerts_in_window = 0;
    for (const auto& record : alert_history_) {
        auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - record.timestamp).count();
        if (elapsed < config_.rate_limit_window_min) {
            alerts_in_window++;
        }
    }
    
    if (alerts_in_window >= config_.max_alerts_per_window) {
        spdlog::debug("Global rate limit reached: {} alerts in {} minute window",
                     alerts_in_window, config_.rate_limit_window_min);
        return true;
    }
    
    // Check band-specific rate limits
    int band_alerts_in_window = 0;
    for (const auto& record : alert_history_) {
        if (record.band == band) {
            auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - record.timestamp).count();
            if (elapsed < config_.rate_limit_window_min) {
                band_alerts_in_window++;
            }
        }
    }
    
    int max_band_alerts = 0;
    if (band == "high_conviction") {
        max_band_alerts = config_.max_high_conviction_per_window;
    } else if (band == "actionable") {
        max_band_alerts = config_.max_actionable_per_window;
    } else if (band == "heads_up") {
        max_band_alerts = config_.max_headsup_per_window;
    } else {
        max_band_alerts = config_.max_watch_per_window;
    }
    
    if (band_alerts_in_window >= max_band_alerts) {
        spdlog::debug("Band-specific rate limit reached for {}: {} alerts in {} minute window",
                     band, band_alerts_in_window, config_.rate_limit_window_min);
        return true;
    }
    
    return false;
}

void ThrottleManager::record_alert(const std::string& mint, const std::string& band) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    AlertRecord record;
    record.mint = mint;
    record.band = band;
    record.timestamp = std::chrono::system_clock::now();
    
    alert_history_.push_back(record);
    
    // Clean up old records
    cleanup();
}

void ThrottleManager::cleanup() {
    auto now = std::chrono::system_clock::now();
    
    // Remove records older than the maximum cooldown period
    int max_cooldown = std::max({
        config_.cooldown_high_conviction_min,
        config_.cooldown_actionable_min,
        config_.cooldown_headsup_min,
        config_.cooldown_watch_min
    });
    
    alert_history_.erase(
        std::remove_if(
            alert_history_.begin(),
            alert_history_.end(),
            [now, max_cooldown](const AlertRecord& record) {
                auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - record.timestamp).count();
                return elapsed > max_cooldown;
            }
        ),
        alert_history_.end()
    );
}
