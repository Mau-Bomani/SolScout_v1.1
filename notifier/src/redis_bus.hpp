#pragma once

#include "config.hpp"
#include "types.hpp"
#include <sw/redis++/redis.h>
#include <functional>
#include <thread>
#include <atomic>
#include <memory>

class RedisBus {
public:
    explicit RedisBus(const Config& config);
    ~RedisBus();

    void start_consumers(
        std::function<void(const InboundAlert&)> alert_callback,
        std::function<void(const CommandRequest&)> command_callback
    );
    void stop();

    bool publish_outbound_alert(const OutboundAlert& alert);
    bool publish_command_reply(const CommandReply& reply);

    bool is_connected();

private:
    void alert_consumer_loop(std::function<void(const InboundAlert&)> callback);
    void command_consumer_loop(std::function<void(const CommandRequest&)> callback);
    bool ensure_connection();

    Config config_;
    std::unique_ptr<sw::redis::Redis> redis_;
    std::atomic<bool> running_{false};

    std::thread alert_consumer_thread_;
    std::thread command_consumer_thread_;
};
