
#include "deduplicator.hpp"
#include <fmt/core.h>

Deduplicator::Deduplicator(const Config& config, std::shared_ptr<sw::redis::Redis> redis)
    : config_(config), redis_(redis) {}

bool Deduplicator::is_duplicate(const InboundAlert& alert) {
    std::string reason_hash = generate_reason_hash(alert.lines);
    std::string key = fmt::format("notifier:dedupe:{}:{}", alert.mint, reason_hash);

    try {
        // SETNX: Set if not exists. Returns true if key was set, false if it already existed.
        // We want to send if the key was set (i.e., it's not a duplicate).
        bool key_was_set = redis_->set(key, "1", std::chrono::seconds(config_.dedupe_period_sec), sw::redis::UpdateType::NOT_EXIST);
        return !key_was_set;
    } catch (const std::exception&) {
        // Fail safe: assume it's a duplicate to prevent spam if Redis is down.
        return true;
    }
}
