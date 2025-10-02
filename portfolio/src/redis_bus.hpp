
#pragma once
#include "config.hpp"
#include "json_schemas.hpp"
#include <string>
#include <functional>
#include <memory>

class RedisBus {
public:
    explicit RedisBus(const Config& config);
    ~RedisBus();

    // Subscription
    void subscribe(const std::string& channel, std::function<void(const std::string&)> callback);
    void stop_subscriber();

    // Publishing
    bool publish_command_reply(const CommandReply& reply);
    bool publish_audit_event(const Audit& audit);

    // Health check
    bool is_connected() const;

    // Non-copyable
    RedisBus(const RedisBus&) = delete;
    RedisBus& operator=(const RedisBus&) = delete;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};
