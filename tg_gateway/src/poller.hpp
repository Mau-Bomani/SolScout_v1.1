
#pragma once
#include "config.hpp"
#include "telegram_client.hpp"
#include <functional>
#include <thread>
#include <atomic>

class TelegramPoller {
public:
    TelegramPoller(const Config& config, TelegramClient& client);
    ~TelegramPoller();
    
    void start(std::function<void(const TelegramUpdate&)> update_handler);
    void stop();
    bool is_running() const;
    
private:
    const Config& config_;
    TelegramClient& client_;
    std::thread poller_thread_;
    std::atomic<bool> running_;
    int last_update_id_;
    
    void polling_loop(std::function<void(const TelegramUpdate&)> update_handler);
};
