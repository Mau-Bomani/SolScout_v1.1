
#include "redis_publisher.hpp"
#include <sw/redis++/redis++.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>
#include "util.hpp"

class RedisPublisher::Impl {
public:
    explicit Impl(const Config& config) : config_(config) {
        try {
            // Configure Redis connection
            sw::redis::ConnectionOptions connection_opts;
            connection_opts.host = config_.redis_host;
            connection_opts.port = config_.redis_port;
            
            if (!config_.redis_password.empty()) {
                connection_opts.password = config_.redis_password;
            }
            
            // Set connection pool size
            sw::redis::ConnectionPoolOptions pool_opts;
            pool_opts.size = 5; // Use 5 connections in the pool
            
            // Create Redis client
            redis_ = std::make_unique<sw::redis::Redis>(connection_opts, pool_opts);
            
            spdlog::info("Connected to Redis at {}:{}", config_.redis_host, config_.redis_port);
        } catch (const std::exception& e) {
            spdlog::error("Failed to connect to Redis: {}", e.what());
            redis_ = nullptr;
        }
    }
    
    bool publish_market_update(const MarketUpdate& update) {
        if (!redis_) {
            spdlog::error("Redis client not initialized");
            return false;
        }
        
        try {
            // Convert market update to JSON
            nlohmann::json json_update = {
                {"id", update.id},
                {"pool_id", update.pool_id},
                {"dex_name", update.dex_name},
                {"token_a", {
                    {"address", update.token_a.address},
                    {"symbol", update.token_a.symbol},
                    {"decimals", update.token_a.decimals}
                }},
                {"token_b", {
                    {"address", update.token_b.address},
                    {"symbol", update.token_b.symbol},
                    {"decimals", update.token_b.decimals}
                }},
                {"price_token_a_in_b", update.price_token_a_in_b},
                {"price_token_b_in_a", update.price_token_b_in_a},
                {"tvl_usd", update.tvl_usd},
                {"volume_24h_usd", update.volume_24h_usd},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    update.timestamp.time_since_epoch()).count()}
            };
            
            // Add fields that might be null
            if (update.price_impact_1pct) {
                json_update["price_impact_1pct"] = *update.price_impact_1pct;
            }
            
            // Publish to Redis stream
            std::unordered_map<std::string, std::string> fields;
            fields["data"] = json_update.dump();
            
            redis_->xadd(config_.redis_stream, "*", fields.begin(), fields.end());
            
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to publish market update to Redis: {}", e.what());
            return false;
        }
    }
    
    bool publish_market_updates(const std::vector<MarketUpdate>& updates) {
        if (updates.empty()) {
            return true;
        }
        
        if (!redis_) {
            spdlog::error("Redis client not initialized");
            return false;
        }
        
        try {
            // Use a pipeline for better performance with multiple updates
            auto pipe = redis_->pipeline();
            
            for (const auto& update : updates) {
                // Convert market update to JSON
                nlohmann::json json_update = {
                    {"id", update.id},
                    {"pool_id", update.pool_id},
                    {"dex_name", update.dex_name},
                    {"token_a", {
                        {"address", update.token_a.address},
                        {"symbol", update.token_a.symbol},
                        {"decimals", update.token_a.decimals}
                    }},
                    {"token_b", {
                        {"address", update.token_b.address},
                        {"symbol", update.token_b.symbol},
                        {"decimals", update.token_b.decimals}
                    }},
                    {"price_token_a_in_b", update.price_token_a_in_b},
                    {"price_token_b_in_a", update.price_token_b_in_a},
                    {"tvl_usd", update.tvl_usd},
                    {"volume_24h_usd", update.volume_24h_usd},
                    {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                        update.timestamp.time_since_epoch()).count()}
                };
                
                // Add fields that might be null
                if (update.price_impact_1pct) {
                    json_update["price_impact_1pct"] = *update.price_impact_1pct;
                }
                
                // Add to pipeline
                std::unordered_map<std::string, std::string> fields;
                fields["data"] = json_update.dump();
                pipe.xadd(config_.redis_stream, "*", fields.begin(), fields.end());
            }
            
            // Execute pipeline
            pipe.exec();
            
            spdlog::debug("Published {} market updates to Redis stream {}", 
                         updates.size(), config_.redis_stream);
            
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to publish market updates to Redis: {}", e.what());
            return false;
        }
    }
    
    bool check_health() {
        if (!redis_) {
            return false;
        }
        
        try {
            // Simple ping to check if Redis is responsive
            redis_->ping();
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Redis health check failed: {}", e.what());
            return false;
        }
    }

private:
    const Config& config_;
    std::unique_ptr<sw::redis::Redis> redis_;
};

// --- PIMPL forward declarations ---
RedisPublisher::RedisPublisher(const Config& config) : pImpl_(std::make_unique<Impl>(config)) {}
RedisPublisher::~RedisPublisher() = default;
bool RedisPublisher::publish_market_update(const MarketUpdate& update) { return pImpl_->publish_market_update(update); }
bool RedisPublisher::publish_market_updates(const std::vector<MarketUpdate>& updates) { return pImpl_->publish_market_updates(updates); }
bool RedisPublisher::check_health() { return pImpl_->check_health(); }
