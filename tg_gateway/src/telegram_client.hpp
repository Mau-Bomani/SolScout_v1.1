
#pragma once
#include "config.hpp"
#include <string>
#include <functional>
#include <nlohmann/json.hpp>

struct TelegramUpdate {
    int64_t update_id;
    struct Message {
        int64_t message_id;
        struct User {
            int64_t id;
            std::string first_name;
            std::string username;
        } from;
        int64_t chat_id;
        std::string text;
    } message;
    
    static TelegramUpdate from_json(const nlohmann::json& j);
};

class TelegramClient {
public:
    explicit TelegramClient(const Config& config);
    
    bool send_message(int64_t chat_id, const std::string& text);
    bool set_webhook(const std::string& url);
    bool delete_webhook();
    std::vector<TelegramUpdate> get_updates(int offset = 0, int timeout = 30);
    
private:
    const Config& config_;
    std::string api_base_url_;
    
    nlohmann::json make_request(const std::string& method, const nlohmann::json& params = {});
};
