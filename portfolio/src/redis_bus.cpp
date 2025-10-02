#include "redis_bus.hpp"
#include "json_schemas.hpp"
#include <spdlog/spdlog.h>
#include <hiredis/hiredis.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>

class RedisBus::Impl {
public:
    Impl(const Config& config) 
        : config_(config), context_(nullptr), sub_context_(nullptr), running_(false) {
        connect();
    }

    ~Impl() {
        stop_subscriber();
        disconnect();
    }

    bool connect() {
        // Main connection
        context_ = redisConnect(config_.redis_host.c_str(), config_.redis_port);
        if (!context_ || context_->err) {
            spdlog::error("Redis connection failed: {}", 
                         context_ ? context_->errstr : "Can't allocate redis context");
            return false;
        }

        // Authenticate if password is provided
        if (!config_.redis_password.empty()) {
            redisReply* reply = (redisReply*)redisCommand(context_, "AUTH %s", config_.redis_password.c_str());
            if (!reply || reply->type == REDIS_REPLY_ERROR) {
                spdlog::error("Redis authentication failed");
                if (reply) freeReplyObject(reply);
                return false;
            }
            freeReplyObject(reply);
        }

        spdlog::info("Connected to Redis at {}:{}", config_.redis_host, config_.redis_port);
        return true;
    }

    void disconnect() {
        if (context_) {
            redisFree(context_);
            context_ = nullptr;
        }
        if (sub_context_) {
            redisFree(sub_context_);
            sub_context_ = nullptr;
        }
    }

    void subscribe(const std::string& channel, std::function<void(const std::string&)> callback) {
        if (running_) {
            spdlog::warn("Subscriber already running");
            return;
        }

        // Create separate connection for subscription
        sub_context_ = redisConnect(config_.redis_host.c_str(), config_.redis_port);
        if (!sub_context_ || sub_context_->err) {
            spdlog::error("Redis subscription connection failed");
            return;
        }

        // Authenticate subscription connection
        if (!config_.redis_password.empty()) {
            redisReply* reply = (redisReply*)redisCommand(sub_context_, "AUTH %s", config_.redis_password.c_str());
            if (!reply || reply->type == REDIS_REPLY_ERROR) {
                spdlog::error("Redis subscription authentication failed");
                if (reply) freeReplyObject(reply);
                return;
            }
            freeReplyObject(reply);
        }

        running_ = true;
        subscriber_thread_ = std::thread([this, channel, callback]() {
            run_subscriber(channel, callback);
        });

        spdlog::info("Subscribed to Redis channel: {}", channel);
    }

    void stop_subscriber() {
        if (!running_) return;

        running_ = false;
        if (subscriber_thread_.joinable()) {
            subscriber_thread_.join();
        }
        
        spdlog::info("Redis subscriber stopped");
    }

    bool publish_command_reply(const CommandReply& reply) {
        try {
            auto json_msg = reply.to_json();
            return publish(config_.redis_reply_channel, json_msg.dump());
        } catch (const std::exception& e) {
            spdlog::error("Failed to publish command reply: {}", e.what());
            return false;
        }
    }

    bool publish_audit_event(const Audit& audit) {
        try {
            auto json_msg = audit.to_json();
            return publish(config_.redis_audit_channel, json_msg.dump());
        } catch (const std::exception& e) {
            spdlog::error("Failed to publish audit event: {}", e.what());
            return false;
        }
    }

    bool is_connected() const {
        return context_ && !context_->err;
    }

private:
    void run_subscriber(const std::string& channel, std::function<void(const std::string&)> callback) {
        redisReply* reply = (redisReply*)redisCommand(sub_context_, "SUBSCRIBE %s", channel.c_str());
        if (!reply) {
            spdlog::error("Failed to subscribe to channel: {}", channel);
            return;
        }
        freeReplyObject(reply);

        while (running_) {
            reply = nullptr;
            if (redisGetReply(sub_context_, (void**)&reply) != REDIS_OK) {
                if (running_) {
                    spdlog::error("Redis subscription error: {}", sub_context_->errstr);
                }
                break;
            }

            if (reply && reply->type == REDIS_REPLY_ARRAY && reply->elements == 3) {
                if (reply->element[0]->type == REDIS_REPLY_STRING &&
                    std::string(reply->element[0]->str) == "message") {
                    
                    std::string message(reply->element[2]->str, reply->element[2]->len);
                    try {
                        callback(message);
                    } catch (const std::exception& e) {
                        spdlog::error("Callback error: {}", e.what());
                    }
                }
            }

            if (reply) {
                freeReplyObject(reply);
            }
        }
    }

    bool publish(const std::string& channel, const std::string& message) {
        if (!is_connected()) {
            if (!connect()) {
                return false;
            }
        }

        redisReply* reply = (redisReply*)redisCommand(context_, "PUBLISH %s %s", 
                                                     channel.c_str(), message.c_str());
        if (!reply) {
            spdlog::error("Failed to publish to channel: {}", channel);
            return false;
        }

        bool success = reply->type != REDIS_REPLY_ERROR;
        if (!success) {
            spdlog::error("Redis publish error: {}", reply->str);
        }

        freeReplyObject(reply);
        return success;
    }

    Config config_;
    redisContext* context_;
    redisContext* sub_context_;
    std::atomic<bool> running_;
    std::thread subscriber_thread_;
};

// Public interface implementation
RedisBus::RedisBus(const Config& config) 
    : pImpl_(std::make_unique<Impl>(config)) {}

RedisBus::~RedisBus() = default;

void RedisBus::subscribe(const std::string& channel, std::function<void(const std::string&)> callback) {
    pImpl_->subscribe(channel, callback);
}

void RedisBus::stop_subscriber() {
    pImpl_->stop_subscriber();
}

bool RedisBus::publish_command_reply(const CommandReply& reply) {
    return pImpl_->publish_command_reply(reply);
}

bool RedisBus::publish_audit_event(const Audit& audit) {
    return pImpl_->publish_audit_event(audit);
}

bool RedisBus::is_connected() const {
    return pImpl_->is_connected();
}
