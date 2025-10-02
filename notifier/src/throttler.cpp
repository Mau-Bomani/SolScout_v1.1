#include "throttler.hpp"
#include <chrono>

Throttler::Throttler(const Config& config, std::shared_ptr<sw::redis::Redis> redis)
    : config_(config), redis_(redis) {
    mute_key_ = "notifier:mute_status";
    global_throttle_key_ = "notifier:global_throttle:actionable";
}

bool Throttler::is_muted() {
    try {
        return redis_->exists(mute_key_);
    } catch (const std::exception&) {
        return false; // Fail safe: don't send if redis is down
    }
}

void Throttler::set_mute(int minutes) {
    try {
        redis_->setex(mute_key_, minutes * 60, "1");
    } catch (const std::exception& e) {
        // Log error, but proceed as if muted to be safe
    }
}

void Throttler::clear_mute() {
    try {
        redis_->del(mute_key_);
    } catch (const std::exception& e) {
        // Log error
    }
}

bool Throttler::is_global_throttled() {
    try {
        auto result = redis_->get(global_throttle_key_);
        if (result && *result == "true") {
            return true;
        }
        
        // Set the key with expiration
        redis_->setex(global_throttle_key_, config_.global_throttle_duration, "true");
        return false;
    } catch (const std::exception&) {
        return false; // Fail safe: don't send if redis is down
    }
}

bool Throttler::is_globally_throttled(const std::string& severity) {
    if (severity != "actionable") {
        return false; // Only throttle actionable alerts
    }
    try {
        auto count = redis_->get(global_throttle_key_);
        if (count && std::stoi(*count) >= config_.global_throttle_limit) {
            return true;
        }
    } catch (const std::exception&) {
        return true; // Fail safe: throttle if redis is down
    }
    return false;
}

void Throttler::record_actionable_alert() {
    try {
        auto result = redis_->incr(global_throttle_key_);
        if (result == 1) {
            // First alert in the window, set the expiry
            redis_->expire(global_throttle_key_, config_.global_throttle_period_sec);
        }
    } catch (const std::exception& e) {
        // Log error
    }
}
