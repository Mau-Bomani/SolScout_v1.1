#pragma once

#include "config.hpp"
#include "dex_client.hpp"
#include "jupiter_client.hpp"
#include "db_manager.hpp"
#include "redis_publisher.hpp"
#include "pool_cache.hpp"
#include <atomic>
#include <memory>
#include <chrono>

class Service {
public:
    explicit Service(const Config& config);
    ~Service();

    void run();
    void stop();

private:
    void tick();
    void save_snapshot_if_needed();
    MarketUpdate create_market_update(const PoolInfo& pool_info);

    const Config& config_;
    DexClient dex_client_;
    JupiterClient jupiter_client_;
    DatabaseManager db_manager_;
    RedisPublisher redis_publisher_;
    PoolCache pool_cache_;

    std::atomic<bool> running_{false};
    std::chrono::steady_clock::time_point last_db_save_;
};
