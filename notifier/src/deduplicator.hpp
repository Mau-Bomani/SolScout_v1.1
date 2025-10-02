#pragma once

#include "config.hpp"
#include "types.hpp"
#include "util.hpp"
#include <sw/redis++/redis.h>
#include <memory>

class Deduplicator {
public:
    Deduplicator(const Config& config, std::shared_ptr<sw::redis::Redis> redis);

    bool is_duplicate(const InboundAlert& alert);

private:
    const Config& config_;
    std::shared_ptr<sw::redis::Redis> redis_;
};
