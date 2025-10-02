#pragma once
#include "config.hpp"
#include "json_schemas.hpp"
#include <sw/redis++/redis++.h>
#include <functional>
#include <thread>
#include <atomic>

class RedisBus {
public:
    explicit RedisBus(const Config& config);
    ~RedisBus();
    
    bool connect();
    void disconnect();
    bool is_connected() const;
    
    bool publish_command_request(const CommandRequest& request);
    bool publish_audit_event(const AuditEvent& event);
    
    void start_reply_consumer(std::function<void(const CommandReply&)> callback);
    void start_alert_consumer(std::function<void(const Alert&)> callback);
    void stop_consumers();
    
    bool store_guest_pin(const std::string& pin, int64_t user_id, int ttl_seconds);
    std::optional<int64_t> get_guest_pin_user(const std::string& pin);
    bool delete_guest_pin(const std::string& pin);
    
private:
    const Config& config_;
    std::unique_ptr<sw::redis::Redis> redis_;
    std::atomic<bool> running_;
    std::thread reply_consumer_thread_;
    std::thread alert_consumer_thread_;
    
    void reply_consumer_loop(std::function<void(const CommandReply&)> callback);
    void alert_consumer_loop(std::function<void(const Alert&)> callback);
};
