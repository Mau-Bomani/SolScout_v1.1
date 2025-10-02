
#pragma once

#include "config.hpp"
#include "types.hpp"
#include <sw/redis++/redis++.h>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>

class RedisBus {
public:
    explicit RedisBus(const Config& config);
    ~RedisBus();

    // Connection management
    bool connect();
    void disconnect();
    bool is_connected() const;
    bool ensure_connection();

    // Subscription methods
    void subscribe_market_updates(std::function<void(const MarketUpdate&)> callback);
    void subscribe_command_requests(std::function<void(const CommandRequest&)> callback);
    void stop_subscribers();

    // Publishing methods
    bool publish_alert(const AlertData& alert);
    bool publish_command_reply(const CommandReply& reply);

    // Non-copyable
    RedisBus(const RedisBus&) = delete;
    RedisBus& operator=(const RedisBus&) = delete;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
