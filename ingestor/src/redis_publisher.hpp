
#pragma once

#include "config.hpp"
#include "types.hpp"
#include <memory>
#include <string>
#include <vector>

class RedisPublisher {
public:
    explicit RedisPublisher(const Config& config);
    ~RedisPublisher();
    
    // Publish a market update to Redis
    bool publish_market_update(const MarketUpdate& update);
    
    // Publish multiple market updates to Redis
    bool publish_market_updates(const std::vector<MarketUpdate>& updates);
    
    // Check Redis connection health
    bool check_health();

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};
