
#include "config.hpp"
#include <cstdlib>
#include <stdexcept>
#include <spdlog/spdlog.h>
#include <fstream>

std::string read_secret_file(const std::string& env_var, const std::string& fallback_env) {
    const char* file_path = std::getenv(env_var.c_str());
    if (file_path) {
        std::ifstream file(file_path);
        if (file.is_open()) {
            std::string content;
            std::getline(file, content);
            return content;
        }
    }
    
    // Fallback to direct environment variable
    const char* value = std::getenv(fallback_env.c_str());
    if (value) {
        return std::string(value);
    }
    
    throw std::runtime_error("Neither " + env_var + " nor " + fallback_env + " found");
}

Config Config::from_env() {
    Config config;
    
    // Read secrets from files or environment
    config.tg_bot_token = read_secret_file("TG_BOT_TOKEN_FILE", "TG_BOT_TOKEN");
    
    std::string owner_id_str = read_secret_file("OWNER_TELEGRAM_ID_FILE", "OWNER_TELEGRAM_ID");
    config.owner_telegram_id = std::stoll(owner_id_str);
    
    // Read other config from environment
    config.redis_url = std::getenv("REDIS_URL") ? std::getenv("REDIS_URL") : "redis://localhost:6379";
    config.gateway_mode = std::getenv("GATEWAY_MODE") ? std::getenv("GATEWAY_MODE") : "poll";
    config.webhook_public_url = std::getenv("WEBHOOK_PUBLIC_URL") ? std::getenv("WEBHOOK_PUBLIC_URL") : "";
    config.listen_addr = std::getenv("LISTEN_ADDR") ? std::getenv("LISTEN_ADDR") : "0.0.0.0";
    config.listen_port = std::getenv("LISTEN_PORT") ? std::stoi(std::getenv("LISTEN_PORT")) : 8080;
    config.rate_limit_msgs_per_min = std::getenv("RATE_LIMIT_MSGS_PER_MIN") ? std::stoi(std::getenv("RATE_LIMIT_MSGS_PER_MIN")) : 20;
    config.global_actionable_max_per_hour = std::getenv("GLOBAL_ACTIONABLE_MAX_PER_HOUR") ? std::stoi(std::getenv("GLOBAL_ACTIONABLE_MAX_PER_HOUR")) : 5;
    config.guest_default_minutes = std::getenv("GUEST_DEFAULT_MINUTES") ? std::stoi(std::getenv("GUEST_DEFAULT_MINUTES")) : 30;
    config.stream_req = std::getenv("STREAM_REQ") ? std::getenv("STREAM_REQ") : "soul.cmd.requests";
    config.stream_rep = std::getenv("STREAM_REP") ? std::getenv("STREAM_REP") : "soul.cmd.replies";
    config.stream_alerts = std::getenv("STREAM_ALERTS") ? std::getenv("STREAM_ALERTS") : "soul.alerts";
    config.stream_audit = std::getenv("STREAM_AUDIT") ? std::getenv("STREAM_AUDIT") : "soul.audit";
    config.service_name = std::getenv("SERVICE_NAME") ? std::getenv("SERVICE_NAME") : "tg_gateway";
    config.log_level = std::getenv("LOG_LEVEL") ? std::getenv("LOG_LEVEL") : "info";
    
    return config;
}

void Config::validate() const {
    if (tg_bot_token.empty()) {
        throw std::runtime_error("Telegram bot token is required");
    }
    
    if (owner_telegram_id == 0) {
        throw std::runtime_error("Owner Telegram ID is required");
    }
    
    if (gateway_mode == "webhook" && webhook_public_url.empty()) {
        throw std::runtime_error("Webhook public URL required for webhook mode");
    }
}
