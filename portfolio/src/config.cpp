
#include "config.hpp"
#include <cstdlib>
#include <spdlog/spdlog.h>

std::string Config::get_env(const std::string& name, const std::string& default_val) {
    const char* value = std::getenv(name.c_str());
    return value ? value : default_val;
}

int Config::get_env_int(const std::string& name, int default_val) {
    const char* value = std::getenv(name.c_str());
    if (!value) {
        return default_val;
    }
    try {
        return std::stoi(value);
    } catch (const std::exception& e) {
        throw std::runtime_error(fmt::format("Invalid integer value for env var {}: {}", name, value));
    }
}

Config Config::from_env() {
    Config config;
    
    // Service
    config.service_name = get_env_var("SERVICE_NAME", "portfolio");
    config.log_level = get_env_var("LOG_LEVEL", "info");
    
    // Database
    config.db_conn_string = get_required_env_var("DATABASE_URL");
    
    // Redis
    config.redis_host = get_env_var("REDIS_HOST", "localhost");
    config.redis_port = std::stoi(get_env_var("REDIS_PORT", "6379"));
    config.redis_password = get_env_var("REDIS_PASSWORD");
    config.redis_command_channel = get_env_var("REDIS_COMMAND_CHANNEL", "commands");
    config.redis_reply_channel = get_env_var("REDIS_REPLY_CHANNEL", "replies");
    config.redis_audit_channel = get_env_var("REDIS_AUDIT_CHANNEL", "audit");
    
    // Solana
    config.solana_rpc_url = get_env_var("SOLANA_RPC_URL", "https://api.mainnet-beta.solana.com");
    
    // Price API
    config.price_api_url = get_env_var("PRICE_API_URL", "https://api.coingecko.com/api/v3");
    config.price_api_key = get_env_var("PRICE_API_KEY");
    
    // Health
    config.health_host = get_env_var("HEALTH_HOST", "0.0.0.0");
    config.health_port = std::stoi(get_env_var("HEALTH_PORT", "8081"));
    
    return config;
}

void Config::validate() const {
    if (db_conn_string.empty()) {
        throw std::runtime_error("DATABASE_URL is required");
    }
    
    if (redis_host.empty()) {
        throw std::runtime_error("REDIS_HOST cannot be empty");
    }
    
    if (redis_port <= 0 || redis_port > 65535) {
        throw std::runtime_error("REDIS_PORT must be between 1 and 65535");
    }
    
    if (health_port <= 0 || health_port > 65535) {
        throw std::runtime_error("HEALTH_PORT must be between 1 and 65535");
    }
    
    spdlog::info("Configuration validated successfully");
}

std::string Config::get_env_var(const std::string& name, const std::string& default_value) const {
    const char* value = std::getenv(name.c_str());
    return value ? std::string(value) : default_value;
}

std::string Config::get_required_env_var(const std::string& name) const {
    const char* value = std::getenv(name.c_str());
    if (!value) {
        throw std::runtime_error("Required environment variable " + name + " is not set");
    }
    return std::string(value);
}
