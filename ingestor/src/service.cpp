#include "service.hpp"
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>
#include "util.hpp"

Service::Service(const Config& config)
    : config_(config),
      dex_client_(config),
      jupiter_client_(config),
      db_manager_(config),
      redis_publisher_(config),
      pool_cache_(config),
      last_db_save_(std::chrono::steady_clock::now()) {
    
    // Initialize database schema
    if (!db_manager_.initialize_schema()) {
        spdlog::warn("Failed to initialize database schema");
    }
}

void Service::run() {
    running_ = true;
    spdlog::info("Ingestor service started. Global tick every {} seconds.", config_.global_tick_seconds);

    while (running_) {
        try {
            tick();
        } catch (const std::exception& e) {
            spdlog::error("Error in service tick: {}", e.what());
        }
        
        auto wake_up_time = std::chrono::steady_clock::now() + std::chrono::seconds(config_.global_tick_seconds);
        while (running_ && std::chrono::steady_clock::now() < wake_up_time) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    spdlog::info("Ingestor service run loop finished.");
}

void Service::stop() {
    if (running_.exchange(false)) {
        spdlog::info("Stopping ingestor service...");
        // Here you could add logic to flush any pending data, e.g., save final snapshot
        db_manager_.save_pool_snapshot(pool_cache_.get_all_pools());
        spdlog::info("Final snapshot saved.");
    }
}

void Service::tick() {
    auto start_time = std::chrono::steady_clock::now();
    spdlog::debug("Starting service tick");
    
    // Fetch pools from DEXs
    auto pools = dex_client_.fetch_pools();
    spdlog::info("Fetched {} pools from DEXs", pools.size());
    
    // Filter pools based on TVL and volume thresholds
    auto filtered_pools = std::vector<PoolInfo>();
    for (const auto& pool : pools) {
        if (pool.tvl_usd >= config_.min_tvl_threshold || 
            pool.volume_24h_usd >= config_.min_volume_threshold) {
            filtered_pools.push_back(pool);
        }
    }
    spdlog::info("{} pools meet threshold criteria", filtered_pools.size());
    
    // Update pool cache
    pool_cache_.update_pools(filtered_pools);
    
    // Create market updates and publish to Redis
    std::vector<MarketUpdate> updates;
    for (const auto& pool : filtered_pools) {
        updates.push_back(create_market_update(pool));
    }
    
    if (!updates.empty()) {
        redis_publisher_.publish_market_updates(updates);
    }
    
    // Save snapshot to database if needed
    save_snapshot_if_needed();
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    spdlog::info("Service tick completed in {} ms", duration);
}

void Service::save_snapshot_if_needed() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - last_db_save_).count();
    
    if (elapsed >= config_.snapshot_persist_minutes) {
        spdlog::info("Saving pool snapshot to database");
        auto pools = pool_cache_.get_all_pools();
        if (db_manager_.save_pool_snapshot(pools)) {
            last_db_save_ = now;
        }
    }
}

MarketUpdate Service::create_market_update(const PoolInfo& pool_info) {
    MarketUpdate update;
    
    // Generate a unique ID for this update
    update.id = generate_uuid();
    
    // Copy pool information
    update.pool_id = pool_info.pool_id;
    update.dex_name = pool_info.dex_name;
    update.token_a = pool_info.token_a;
    update.token_b = pool_info.token_b;
    update.price_token_a_in_b = pool_info.price_token_a_in_b;
    update.price_token_b_in_a = pool_info.price_token_b_in_a;
    update.tvl_usd = pool_info.tvl_usd;
    update.volume_24h_usd = pool_info.volume_24h_usd;
    update.price_impact_1pct = pool_info.price_impact_1pct;
    
    // Set timestamp to current time
    update.timestamp = std::chrono::system_clock::now();
    
    return update;
}
