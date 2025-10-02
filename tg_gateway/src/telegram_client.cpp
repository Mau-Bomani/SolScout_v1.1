
#include "telegram_client.hpp"
#include "util.hpp"
#include <cpr/cpr.h>
#include <spdlog/spdlog.h>

TelegramUpdate TelegramUpdate::from_json(const nlohmann::json& j) {
    TelegramUpdate update;
    update.update_id = j["update_id"];
    
    if (j.contains("message")) {
        const auto& msg = j["message"];
        update.message.message_id = msg["message_id"];
        update.message.chat_id = msg["chat"]["id"];
        
        if (msg.contains("from")) {
            update.message.from.id = msg["from"]["id"];
            update.message.from.first_name = msg["from"].value("first_name", "");
            update.message.from.username = msg["from"].value("username", "");
        }
        
        update.message.text = msg.value("text", "");
    }
    
    return update;
}

TelegramClient::TelegramClient(const Config& config) 
    : config_(config), api_base_url_("https://api.telegram.org/bot" + config.tg_bot_token) {}

bool TelegramClient::send_message(int64_t chat_id, const std::string& text) {
    nlohmann::json params = {
        {"chat_id", chat_id},
        {"text", text},
        {"parse_mode", "HTML"}
    };
    
    auto response = make_request("sendMessage", params);
    bool success = response.value("ok", false);
    
    if (!success) {
        spdlog::error("Failed to send message: {}", response.dump());
    }
    
    return success;
}

bool TelegramClient::set_webhook(const std::string& url) {
    nlohmann::json params = {
        {"url", url}
    };
    
    auto response = make_request("setWebhook", params);
    bool success = response.value("ok", false);
    
    if (success) {
        spdlog::info("Webhook set to: {}", url);
    } else {
        spdlog::error("Failed to set webhook: {}", response.dump());
    }
    
    return success;
}

bool TelegramClient::delete_webhook() {
    auto response = make_request("deleteWebhook");
    bool success = response.value("ok", false);
    
    if (success) {
        spdlog::info("Webhook deleted");
    } else {
        spdlog::error("Failed to delete webhook: {}", response.dump());
    }
    
    return success;
}

std::vector<TelegramUpdate> TelegramClient::get_updates(int offset, int timeout) {
    nlohmann::json params = {
        {"offset", offset},
        {"timeout", timeout}
    };
    
    auto response = make_request("getUpdates", params);
    std::vector<TelegramUpdate> updates;
    
    if (response.value("ok", false) && response.contains("result")) {
        for (const auto& update_json : response["result"]) {
            try {
                updates.push_back(TelegramUpdate::from_json(update_json));
            } catch (const std::exception& e) {
                spdlog::warn("Failed to parse update: {}", e.what());
            }
        }
    }
    
    return updates;
}

nlohmann::json TelegramClient::make_request(const std::string& method, const nlohmann::json& params) {
    std::string url = api_base_url_ + "/" + method;
    
    try {
        cpr::Response response;
        if (params.empty()) {
            response = cpr::Post(cpr::Url{url});
        } else {
            response = cpr::Post(
                cpr::Url{url},
                cpr::Header{{"Content-Type", "application/json"}},
                cpr::Body{params.dump()}
            );
        }
        
        if (response.status_code == 200) {
            return nlohmann::json::parse(response.text);
        } else {
            spdlog::error("HTTP error {}: {}", response.status_code, response.text);
            return nlohmann::json{{"ok", false}, {"error", "HTTP error"}};
        }
    } catch (const std::exception& e) {
        spdlog::error("Request failed: {}", e.what());
        return nlohmann::json{{"ok", false}, {"error", "Request failed"}};
    }
}
