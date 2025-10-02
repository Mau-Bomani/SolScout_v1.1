
#include "redis_bus.hpp"
#include "util.hpp"
#include <spdlog/spdlog.h>
#include <chrono>

RedisBus::RedisBus(const Config& config) : config_(config), running_(false) {}

RedisBus::~RedisBus() {
    stop_consumers();
    disconnect();
}

bool RedisBus::connect() {
    try {
        redis_ = std::make_unique<sw::redis::Redis>(config_.redis_url);
        redis_->ping();
        spdlog::info("Connected to Redis: {}", config_.redis_url);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to connect to Redis: {}", e.what());
        return false;
    }
}

void RedisBus::disconnect() {
    redis_.reset();
}

bool RedisBus::is_connected() const {
    if (!redis_) return false;
    
    try {
        redis_->ping();
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool RedisBus::publish_command_request(const CommandRequest& request) {
    if (!redis_) return false;
    
    try {
        auto json_str = request.to_json().dump();
        redis_->xadd(config_.stream_req, "*", {{"data", json_str}});
        spdlog::debug("Published command request: {}", request.cmd);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to publish command request: {}", e.what());
        return false;
    }
}

bool RedisBus::publish_audit_event(const AuditEvent& event) {
    if (!redis_) return false;
    
    try {
        auto json_str = event.to_json().dump();
        redis_->xadd(config_.stream_audit, "*", {{"data", json_str}});
        spdlog::debug("Published audit event: {}", event.event);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to publish audit event: {}", e.what());
        return false;
    }
}

void RedisBus::start_reply_consumer(std::function<void(const CommandReply&)> callback) {
    if (running_) return;
    
    running_ = true;
    reply_consumer_thread_ = std::thread([this, callback]() {
        reply_consumer_loop(callback);
    });
}

void RedisBus::start_alert_consumer(std::function<void(const Alert&)> callback) {
    if (!running_) return;
    
    alert_consumer_thread_ = std::thread([this, callback]() {
        alert_consumer_loop(callback);
    });
}

void RedisBus::stop_consumers() {
    running_ = false;
    
    if (reply_consumer_thread_.joinable()) {
        reply_consumer_thread_.join();
    }
    
    if (alert_consumer_thread_.joinable()) {
        alert_consumer_thread_.join();
    }
}

bool RedisBus::store_guest_pin(const std::string& pin, int64_t owner_id, int ttl_seconds) {
    if (!redis_) return false;
    
    try {
        redis_->setex("guest_pin:" + pin, ttl_seconds, std::to_string(owner_id));
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to store guest PIN: {}", e.what());
        return false;
    }
}

std::optional<int64_t> RedisBus::get_guest_pin_user(const std::string& pin) {
    if (!redis_) return std::nullopt;
    
    try {
        auto result = redis_->get("guest_pin:" + pin);
        if (result) {
            return std::stoll(*result);
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to get guest PIN: {}", e.what());
    }
    
    return std::nullopt;
}

void RedisBus::delete_guest_pin(const std::string& pin) {
    if (!redis_) return;
    
    try {
        redis_->del("guest_pin:" + pin);
    } catch (const std::exception& e) {
        spdlog::error("Failed to delete guest PIN: {}", e.what());
    }
}

void RedisBus::reply_consumer_loop(std::function<void(const CommandReply&)> callback) {
    std::string consumer_group = config_.service_name + "_replies";
    std::string consumer_name = config_.service_name + "_" + std::to_string(getpid());
    
    try {
        redis_->xgroup_create(config_.stream_rep, consumer_group, "0", true);
    } catch (const std::exception&) {
        // Group might already exist
    }
    
    while (running_) {
        try {
            auto result = redis_->xreadgroup(consumer_group, consumer_name, 
                {{config_.stream_rep, ">"}}, std::chrono::milliseconds(1000));
            
            for (const auto& stream : result) {
                for (const auto& message : stream.second) {
                    try {
                        auto data_it = message.second.find("data");
                        if (data_it != message.second.end()) {
                            auto json = nlohmann::json::parse(data_it->second);
                            auto reply = CommandReply::from_json(json);
                            callback(reply);
                            
                            redis_->xack(config_.stream_rep, consumer_group, {message.first});
                        }
                    } catch (const std::exception& e) {
                        spdlog::error("Failed to process reply message: {}", e.what());
                    }
                }
            }
        } catch (const std::exception& e) {
            if (running_) {
                spdlog::error("Reply consumer error: {}", e.what());
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }
    }
}

void RedisBus::alert_consumer_loop(std::function<void(const Alert&)> callback) {
    std::string consumer_group = config_.service_name + "_alerts";
    std::string consumer_name = config_.service_name + "_" + std::to_string(getpid());
    
    try {
        redis_->xgroup_create(config_.stream_alerts, consumer_group, "0", true);
    } catch (const std::exception&) {
        // Group might already exist
    }
    
    while (running_) {
        try {
            auto result = redis_->xreadgroup(consumer_group, consumer_name,
                {{config_.stream_alerts, ">"}}, std::chrono::milliseconds(1000));
            
            for (const auto& stream : result) {
                for (const auto& message : stream.second) {
                    try {
                        auto data_it = message.second.find("data");
                        if (data_it != message.second.end()) {
                            auto json = nlohmann::json::parse(data_it->second);
                            auto alert = Alert::from_json(json);
                            callback(alert);
                            
                            redis_->xack(config_.stream_alerts, consumer_group, {message.first});
                        }
                    } catch (const std::exception& e) {
                        spdlog::error("Failed to process alert message: {}", e.what());
                    }
                }
            }
        } catch (const std::exception& e) {
            if (running_) {
                spdlog::error("Alert consumer error: {}", e.what());
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }
    }
}

// Enhanced Redis reconnection with exponential backoff
bool RedisBus::ensureConnection() {
    if (redis_ && redis_->ping() == "PONG") {
        return true;
    }

    static int retry_count = 0;
    static auto last_attempt = std::chrono::steady_clock::now();
    
    auto now = std::chrono::steady_clock::now();
    auto backoff_ms = std::min(1000 * (1 << retry_count), 30000); // Max 30s
    
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_attempt).count() < backoff_ms) {
        return false;
    }

    try {
        redis_ = std::make_unique<sw::redis::Redis>(config_.redis_url);
        redis_->ping();
        spdlog::info("Redis connection restored");
        retry_count = 0;
        return true;
    } catch (const std::exception& e) {
        retry_count++;
        last_attempt = now;
        spdlog::warn("Redis connection failed (attempt {}): {}", retry_count, e.what());
        return false;
    }
}
