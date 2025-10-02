
#pragma once

#include "config.hpp"
#include <sw/redis++/redis.h>
#include <memory>

class Throttler {
public:
    Throttler(const Config& config, std::shared_ptr<sw::redis::Redis> redis);

    bool is_muted();
    void set_mute(int minutes);
    void clear_mute();

    bool is_globally_throttled(const std::string& severity);
    void record_actionable_alert();

private:
    const Config& config_;
    std::shared_ptr<sw::redis::Redis> redis_;
    std::string mute_key_;
    std::string global_throttle_key_;
};
