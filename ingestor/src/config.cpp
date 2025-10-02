
#include "config.hpp"
#include "util.hpp"
#include <stdexcept>
#include <spdlog/spdlog.h>

Config Config::from_env() {
    Config config;
    
    // Service
    config.service_name = get_env_var("SERVICE_NAME", "ingestor");
    config.log_level = get_env_var("LOG_LEVEL", "info");
    
    // Database
    config.db_conn_string = get_required_env_var("DATABASE_URL");
    
    // Redis
    config.redis_host = get_env_var("REDIS_HOST", "localhost");
    config.redis_port = std::stoi(get_env_var("REDIS_PORT", "6379"));
    config.redis_password = get_env_var("REDIS_PASSWORD");
    config.redis_stream = get_env_var("REDIS_STREAM", "soul.market.updates");
    
    // Solana RPC URLs (comma-separated)
    std::string rpc_urls_str = get_env_var("SOLANA_RPC_URLS", 
        "https://api.mainnet-beta.solana.com,https://solana-api.projectserum.com,https://rpc.ankr.com/solana");
    config.solana_rpc_urls = split_string(rpc_urls_str, ',');
    
    // DEX APIs
    config.raydium_api_url = get_env_var("RAYDIUM_API_URL", "https://api.raydium.io/v2");
    config.orca_api_url = get_env_var("ORCA_API_URL", "https://api.orca.so/v1");
    config.jupiter_api_url = get_env_var("JUPITER_API_URL", "https://quote-api.jup.ag/v6");
    
    // CoinGecko
    config.coingecko_api_url = get_env_var("COINGECKO_API_URL", "https://api.coingecko.com/api/v3");
    config.coingecko_api_key = get_env_var("COINGECKO_API_KEY");
    
    // Timing
    config.global_tick_seconds = std::stoi(get_env_var("GLOBAL_TICK_SECONDS", "60"));
    config.ohlcv_interval_minutes = std::stoi(get_env_var("OHLCV_INTERVAL_MINUTES", "5"));
    config.snapshot_persist_minutes = std::stoi(get_env_var("SNAPSHOT_PERSIST_MINUTES", "5"));
    
    // Rate limiting
    config.max_concurrent_requests = std::stoi(get_env_var("MAX_CONCURRENT_REQUESTS", "10"));
    config.base_backoff_seconds = std::stod(get_env_var("BASE_BACKOFF_SECONDS", "1.0"));
    config.max_backoff_seconds = std::stod(get_env_var("MAX_BACKOFF_SECONDS", "300.0"));
    
    // Cache
    config.pool_cache_max_size = std::stoi(get_env_var("POOL_CACHE_MAX_SIZE", "10000"));
    config.pool_cache_ttl_minutes = std::stoi(get_env_var("POOL_CACHE_TTL_MINUTES", "30"));
    
    // Thresholds
    config.min_tvl_threshold = std::stod(get_env_var("MIN_TVL_THRESHOLD", "25000.0"));
    config.min_volume_threshold = std::stod(get_env_var("MIN_VOLUME_THRESHOLD", "1000.0"));
    
    // Health
    config.health_host = get_env_var("HEALTH_HOST", "0.0.0.0");
    config.health_port = std::stoi(get_env_var("HEALTH_PORT", "8082"));
    
    return config;
}

void Config::validate() const {
    if (db_conn_string.empty()) {
        throw std::runtime_error("DATABASE_URL is required");
    }
    
    if (solana_rpc_urls.empty()) {
        throw std::runtime_error("At least one Solana RPC URL is required");
    }
    
    if (global_tick_seconds < 10) {
        throw std::runtime_error("Global tick interval must be at least 10 seconds");
    }
    
    if (max_concurrent_requests < 1 || max_concurrent_requests > 100) {
        throw std::runtime_error("Max concurrent requests must be between 1 and 100");
    }
    
    spdlog::info("Configuration validated successfully");
}
