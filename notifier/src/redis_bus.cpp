#include "redis_bus.hpp"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <unistd.h> // for getpid()

RedisBus::RedisBus(const Config& config) : config_(config) {
    ensure_connection();
}

RedisBus::~RedisBus() {
    stop();
}

bool RedisBus::ensure_connection() {
    if (redis_ && redis_->ping()) {
        return true;
    }
    try {
        redis_ = std::make_unique<sw::redis::Redis>(config_.redis_url);
        return redis_->ping();
    } catch (const std::exception& e) {
        spdlog::error("Failed to connect to Redis: {}", e.what());
        redis_.reset();
        return false;
    }
}

void RedisBus::start_consumers(
    std::function<void(const InboundAlert&)> alert_callback,
    std::function<void(const CommandRequest&)> command_callback) {
    if (running_) return;
    running_ = true;

    alert_consumer_thread_ = std::thread([this, callback = std::move(alert_callback)]() {
        alert_consumer_loop(callback);
    });

    command_consumer_thread_ = std::thread([this, callback = std::move(command_callback)]() {
        command_consumer_loop(callback);
    });
}

void RedisBus::stop() {
    if (!running_) return;
    running_ = false;
    if (alert_consumer_thread_.joinable()) {
        alert_consumer_thread_.join();
    }
    if (command_consumer_thread_.joinable()) {
        command_consumer_thread_.join();
    }
}

bool RedisBus::is_connected() {
    return redis_ && redis_->ping();
}

void RedisBus::alert_consumer_loop(std::function<void(const InboundAlert&)> callback) {
    std::string consumer_group = config_.service_name + "_alerts_in";
    std::string consumer_name = config_.service_name + "_" + std::to_string(getpid());

    if (!ensure_connection()) return;

    try {
        redis_->xgroup_create(config_.stream_alerts_in, consumer_group, "0", true);
    } catch (const sw::redis::Error&) { /* Group likely exists */ }

    while (running_) {
        try {
            if (!ensure_connection()) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                continue;
            }

            auto result = redis_->xreadgroup(consumer_group, consumer_name,
                {{config_.stream_alerts_in, ">"}}, std::chrono::milliseconds(1000));

            for (const auto& stream : result) {
                for (const auto& msg : stream.second) {
                    try {
                        auto data_it = msg.second.find("data");
                        if (data_it != msg.second.end()) {
                            auto json = nlohmann::json::parse(data_it->second);
                            auto alert = InboundAlert::from_json(json);
                            callback(alert);
                            redis_->xack(config_.stream_alerts_in, consumer_group, msg.first);
                        }
                    } catch (const std::exception& e) {
                        spdlog::error("Failed to process inbound alert message: {}", e.what());
                    }
                }
            }
        } catch (const sw::redis::TimeoutError&) {
            // This is expected, just continue
        } catch (const std::exception& e) {
            if (running_) {
                spdlog::error("Alert consumer error: {}", e.what());
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }
    }
}

void RedisBus::command_consumer_loop(std::function<void(const CommandRequest&)> callback) {
    std::string consumer_group = config_.service_name + "_commands";
    std::string consumer_name = config_.service_name + "_" + std::to_string(getpid());

    if (!ensure_connection()) return;

    try {
        redis_->xgroup_create(config_.stream_req, consumer_group, "0", true);
    } catch (const sw::redis::Error&) { /* Group likely exists */ }

    while (running_) {
        try {
            if (!ensure_connection()) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                continue;
            }

            auto result = redis_->xreadgroup(consumer_group, consumer_name,
                {{config_.stream_req, ">"}}, std::chrono::milliseconds(1000));

            for (const auto& stream : result) {
                for (const auto& msg : stream.second) {
                    try {
                        auto data_it = msg.second.find("data");
                        if (data_it != msg.second.end()) {
                            auto json = nlohmann::json::parse(data_it->second);
                            auto req = CommandRequest::from_json(json);
                            callback(req);
                            redis_->xack(config_.stream_req, consumer_group, msg.first);
                        }
                    } catch (const std::exception& e) {
                        spdlog::error("Failed to process command request message: {}", e.what());
                    }
                }
            }
        } catch (const sw::redis::TimeoutError&) {
            // This is expected, just continue
        } catch (const std::exception& e) {
            if (running_) {
                spdlog::error("Command consumer error: {}", e.what());
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }
    }
}

void RedisBus::publish_alert(const OutboundAlert& alert) {
    if (!ensure_connection()) return;

    try {
        nlohmann::json json = alert.to_json();
        std::string data = json.dump();

        redis_->xadd(config_.stream_alerts_out, "*", "data", data);
    } catch (const std::exception& e) {
        spdlog::error("Failed to publish alert: {}", e.what());
    }
}

void RedisBus::publish_command(const CommandResponse& command) {
    if (!ensure_connection()) return;

    try {
        nlohmann::json json = command.to_json();
        std::string data = json.dump();

        redis_->xadd(config_.stream_commands_out, "*", "data", data);
    } catch (const std::exception& e) {
        spdlog::error("Failed to publish command: {}", e.what());
    }
}

bool RedisBus::publish_outbound_alert(const OutboundAlert& alert) {
    if (!ensure_connection()) return false;
    try {
        auto j = alert.to_json();
        std::string data = j.dump();
        std::unordered_map<std::string, std::string> fields = {{"data", data}};
        redis_->xadd(config_.stream_alerts_out, "*", fields.begin(), fields.end());
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to publish outbound alert: {}", e.what());
        return false;
    }
}

bool RedisBus::publish_command_reply(const CommandReply& reply) {
    if (!ensure_connection()) return false;
    try {
        auto j = reply.to_json();
        std::string data = j.dump();
        std::unordered_map<std::string, std::string> fields = {{"data", data}};
        redis_->xadd(config_.stream_rep, "*", fields.begin(), fields.end());
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to publish command reply: {}", e.what());
        return false;
    }
}
