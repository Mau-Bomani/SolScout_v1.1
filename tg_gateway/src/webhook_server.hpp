
#pragma once
#include "config.hpp"
#include <httplib.h>
#include <functional>
#include <thread>
#include <atomic>
#include <nlohmann/json.hpp>

class WebhookServer {
public:
    explicit WebhookServer(const Config& config);
    ~WebhookServer();
    
    void start(std::function<void(const nlohmann::json&)> update_handler);
    void stop();
    bool is_running() const;
    
    struct HealthStatus {
        bool ok;
        bool redis_connected;
        std::string mode;
    };
    
    void set_health_status(const HealthStatus& status);
    
private:
    const Config& config_;
    std::unique_ptr<httplib::Server> server_;
    std::thread server_thread_;
    std::atomic<bool> running_;
    HealthStatus health_status_;
    std::mutex health_mutex_;
    
    void setup_routes(std::function<void(const nlohmann::json&)> update_handler);
};
