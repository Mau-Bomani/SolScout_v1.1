
#pragma once
#include <string>
#include <vector>

class Config {
public:
    // Service info
    std::string service_name = "ingestor";
    std::string log_level = "info";
    
    // Database
    std::string db_conn_string;
    
    // Redis
    std::string redis_host = "localhost";
    int redis_port = 6379;
    std::string redis_password;
    std::string redis_stream = "soul.market.updates";
    
    // Solana RPC endpoints (rotation on failure)
    std::vector<std::string> solana_rpc_urls = {
        "https://api.mainnet-beta.solana.com",
        "https://solana-api.projectserum.com",
        "https://rpc.ankr.com/solana"
    };
    
    // DEX endpoints
    std::string raydium_api_url = "https://api.raydium.io/v2";
    std::string orca_api_url = "https://api.orca.so/v1";
    std::string jupiter_api_url = "https://quote-api.jup.ag/v6";
    
    // CoinGecko fallback
    std::string coingecko_api_url = "https://api.coingecko.com/api/v3";
    std::string coingecko_api_key;
    
    // Timing configuration
    int global_tick_seconds = 60;
    int ohlcv_interval_minutes = 5;
    int snapshot_persist_minutes = 5;
    
    // Rate limiting
    int max_concurrent_requests = 10;
    double base_backoff_seconds = 1.0;
    double max_backoff_seconds = 300.0;
    
    // Cache settings
    int pool_cache_max_size = 10000;
    int pool_cache_ttl_minutes = 30;
    
    // Liquidity thresholds
    double min_tvl_threshold = 25000.0;
    double min_volume_threshold = 1000.0;
    
    // Health check
    std::string health_host = "0.0.0.0";
    int health_port = 8082;

    static Config from_env();
    void validate() const;
};
