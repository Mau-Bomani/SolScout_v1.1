
#include "redis_bus.hpp"
#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class RedisBus::Impl {
public:
    Impl(const Config& config) 
        : config_(config), running_(false), backoff_ms_(1000), retry_count_(0) {
        connect();
    }

    ~Impl() {
        stop_subscribers();
        disconnect();
    }

    bool connect() {
        try {
            redis_ = std::make_unique<sw::redis::Redis>(config_.redis_url);
            redis_->ping();
            spdlog::info("Connected to Redis at {}", config_.redis_url);
            backoff_ms_ = 1000;  // Reset backoff on successful connection
            retry_count_ = 0;
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to connect to Redis: {}", e.what());
            return false;
        }
    }

    void disconnect() {
        if (redis_) {
            redis_.reset();
            spdlog::info("Disconnected from Redis");
        }
    }

    bool is_connected() const {
        if (!redis_) return false;
        
        try {
            redis_->ping();
            return true;
        } catch (...) {
            return false;
        }
    }

    bool ensure_connection() {
        if (is_connected()) {
            return true;
        }

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_connection_attempt_).count() < backoff_ms_) {
            return false;
        }

        last_connection_attempt_ = now;
        
        try {
            if (connect()) {
                spdlog::info("Redis connection restored");
                return true;
            }
        } catch (const std::exception& e) {
            spdlog::warn("Redis reconnection failed (attempt {}): {}", ++retry_count_, e.what());
        }

        // Exponential backoff with cap
        backoff_ms_ = std::min(backoff_ms_ * 2, 30000);
        return false;
    }

    void subscribe_market_updates(std::function<void(const MarketUpdate&)> callback) {
        if (market_thread_.joinable()) {
            spdlog::warn("Market updates subscriber already running");
            return;
        }

        running_ = true;
        market_thread_ = std::thread([this, callback]() {
            spdlog::info("Starting market updates subscriber on {}", config_.stream_market);
            
            sw::redis::ConnectionOptions opts;
            opts.uri = config_.redis_url;
            
            sw::redis::Redis redis(opts);
            
            // Create a consumer group if it doesn't exist
            try {
                redis.xgroup_create(config_.stream_market, "analytics_group", "0", true);
            } catch (const std::exception& e) {
                spdlog::debug("Consumer group already exists or error: {}", e.what());
            }
            
            std::string consumer_id = "analytics_" + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()));
            
            while (running_) {
                try {
                    // Read from the stream with a block of 1000ms
                    auto result = redis.xreadgroup("analytics_group", consumer_id, 
                                                  {{config_.stream_market, ">"}}, 
                                                  1, 1000);
                    
                    if (!result.empty() && !result[0].second.empty()) {
                        const auto& stream_entries = result[0].second;
                        for (const auto& entry : stream_entries) {
                            const auto& id = entry.first;
                            const auto& fields = entry.second;
                            
                            try {
                                if (fields.find("data") != fields.end()) {
                                    auto j = json::parse(fields.at("data"));
                                    auto update = MarketUpdate::from_json(j);
                                    if (update) {
                                        callback(*update);
                                    }
                                }
                                
                                // Acknowledge the message
                                redis.xack(config_.stream_market, "analytics_group", id);
                            } catch (const std::exception& e) {
                                spdlog::error("Error processing market update: {}", e.what());
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    spdlog::error("Error in market updates subscriber: {}", e.what());
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
            }
            
            spdlog::info("Market updates subscriber stopped");
        });
    }

    void subscribe_command_requests(std::function<void(const CommandRequest&)> callback) {
        if (command_thread_.joinable()) {
            spdlog::warn("Command requests subscriber already running");
            return;
        }

        running_ = true;
        command_thread_ = std::thread([this, callback]() {
            spdlog::info("Starting command requests subscriber on {}", config_.stream_req);
            
            sw::redis::ConnectionOptions opts;
            opts.uri = config_.redis_url;
            
            sw::redis::Redis redis(opts);
            
            // Create a consumer group if it doesn't exist
            try {
                redis.xgroup_create(config_.stream_req, "analytics_group", "0", true);
            } catch (const std::exception& e) {
                spdlog::debug("Consumer group already exists or error: {}", e.what());
            }
            
            std::string consumer_id = "analytics_" + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()));
            
            while (running_) {
                try {
                    // Read from the stream with a block of 1000ms
                    auto result = redis.xreadgroup("analytics_group", consumer_id, 
                                                  {{config_.stream_req, ">"}}, 
                                                  1, 1000);
                    
                    if (!result.empty() && !result[0].second.empty()) {
                        const auto& stream_entries = result[0].second;
                        for (const auto& entry : stream_entries) {
                            const auto& id = entry.first;
                            const auto& fields = entry.second;
                            
                            try {
                                if (fields.find("data") != fields.end()) {
                                    auto j = json::parse(fields.at("data"));
                                    auto request = CommandRequest::from_json(j);
                                    if (request && request->cmd == "signals") {
                                        callback(*request);
                                    }
                                }
                                
                                // Acknowledge the message
                                redis.xack(config_.stream_req, "analytics_group", id);
                            } catch (const std::exception& e) {
                                spdlog::error("Error processing command request: {}", e.what());
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    spdlog::error("Error in command requests subscriber: {}", e.what());
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
            }
            
            spdlog::info("Command requests subscriber stopped");
        });
    }

    void stop_subscribers() {
        running_ = false;
        
        if (market_thread_.joinable()) {
            market_thread_.join();
        }
        
        if (command_thread_.joinable()) {
            command_thread_.join();
        }
    }

    bool publish_alert(const AlertData& alert) {
        if (!ensure_connection()) {
            return false;
        }

        try {
            auto j = alert.to_json();
            std::string data = j.dump();
            
            std::unordered_map<std::string, std::string> fields = {
                {"data", data},
                {"timestamp", std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                    alert.timestamp.time_since_epoch()).count())}
            };
            
            redis_->xadd(config_.stream_alerts, "*", fields.begin(), fields.end());
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to publish alert: {}", e.what());
            return false;
        }
    }

    bool publish_command_reply(const CommandReply& reply) {
        if (!ensure_connection()) {
            return false;
        }

        try {
            auto j = reply.to_json();
            std::string data = j.dump();
            
            std::unordered_map<std::string, std::string> fields = {
                {"data", data},
                {"corr_id", reply.corr_id},
                {"timestamp", std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                    reply.timestamp.time_since_epoch()).count())}
            };
            
            redis_->xadd(config_.stream_rep, "*", fields.begin(), fields.end());
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to publish command reply: {}", e.what());
            return false;
        }
    }

private:
    const Config& config_;
    std::unique_ptr<sw::redis::Redis> redis_;
    std::atomic<bool> running_;
    std::thread market_thread_;
    std::thread command_thread_;
    
    // Reconnection logic
    std::chrono::steady_clock::time_point last_connection_attempt_ = std::chrono::steady_clock::now();
    int backoff_ms_;
    int retry_count_;
};

// RedisBus implementation using the Impl class
RedisBus::RedisBus(const Config& config) : impl_(std::make_unique<Impl>(config)) {}

RedisBus::~RedisBus() = default;

bool RedisBus::connect() {
    return impl_->connect();
}

void RedisBus::disconnect() {
    impl_->disconnect();
}

bool RedisBus::is_connected() const {
    return impl_->is_connected();
}

bool RedisBus::ensure_connection() {
    return impl_->ensure_connection();
}

void RedisBus::subscribe_market_updates(std::function<void(const MarketUpdate&)> callback) {
    impl_->subscribe_market_updates(std::move(callback));
}

void RedisBus::subscribe_command_requests(std::function<void(const CommandRequest&)> callback) {
    impl_->subscribe_command_requests(std::move(callback));
}

void RedisBus::stop_subscribers() {
    impl_->stop_subscribers();
}

bool RedisBus::publish_alert(const AlertData& alert) {
    return impl_->publish_alert(alert);
}

bool RedisBus::publish_command_reply(const CommandReply& reply) {
    return impl_->publish_command_reply(reply);
}
