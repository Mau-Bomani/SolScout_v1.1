
#pragma once
#include <string>
#include <stdexcept>

class Config {
public:
    // Service info
    std::string service_name = "portfolio";
    std::string log_level = "info";
    
    // Database
    std::string db_conn_string;
    
    // Redis
    std::string redis_host = "localhost";
    int redis_port = 6379;
    std::string redis_password;
    std::string redis_command_channel = "commands";
    std::string redis_reply_channel = "replies";
    std::string redis_audit_channel = "audit";
    
    // Solana RPC
    std::string solana_rpc_url = "https://api.mainnet-beta.solana.com";
    
    // Price API
    std::string price_api_url = "https://api.coingecko.com/api/v3";
    std::string price_api_key;
    
    // Health check
    std::string health_host = "0.0.0.0";
    int health_port = 8081;

    static Config from_env();
    void validate() const;

private:
    std::string get_env_var(const std::string& name, const std::string& default_value = "") const;
    std::string get_required_env_var(const std::string& name) const;
};
