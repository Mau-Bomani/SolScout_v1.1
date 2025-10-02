
#include "poller.hpp"
#include <spdlog/spdlog.h>
#include <chrono>

TelegramPoller::TelegramPoller(const Config& config, TelegramClient& client)
    : config_(config), client_(client), running_(false), last_update_id_(0) {}

TelegramPoller::~TelegramPoller() {
    stop();
}

void TelegramPoller::start(std::function<void(const TelegramUpdate&)> update_handler) {
    running_ = true;
    poller_thread_ = std::thread(&TelegramPoller::polling_loop, this, update_handler);
    spdlog::info("Started Telegram polling");
}

void TelegramPoller::stop() {
    if (running_) {
        running_ = false;
        if (poller_thread_.joinable()) {
            poller_thread_.join();
        }
        spdlog::info("Stopped Telegram polling");
    }
}

bool TelegramPoller::is_running() const {
    return running_;
}

void TelegramPoller::polling_loop(std::function<void(const TelegramUpdate&)> update_handler) {
    while (running_) {
        try {
            auto updates = client_.get_updates(last_update_id_ + 1, 30);
            
            for (const auto& update : updates) {
                if (update.update_id > last_update_id_) {
                    last_update_id_ = update.update_id;
                }
                
                try {
                    update_handler(update);
                } catch (const std::exception& e) {
                    spdlog::error("Error handling update {}: {}", update.update_id, e.what());
                }
            }
            
            if (updates.empty()) {
                // Short sleep if no updates to prevent busy waiting
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
        } catch (const std::exception& e) {
            spdlog::error("Polling error: {}", e.what());
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
}
