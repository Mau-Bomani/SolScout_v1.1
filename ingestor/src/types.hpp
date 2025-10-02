
#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <optional>

struct PoolInfo {
    std::string pool_id;
    std::string token_a_mint;
    std::string token_b_mint;
    std::string dex_name;
    double reserve_a = 0.0;
    double reserve_b = 0.0;
    double tvl_usd = 0.0;
    double volume_24h_usd = 0.0;
    double price_a_in_b = 0.0;
    double price_impact_1pct = 0.0;
    std::chrono::system_clock::time_point last_updated;
    std::chrono::system_clock::time_point first_seen;
    bool is_active = true;
};

struct TokenInfo {
    std::string mint_address;
    std::string symbol;
    std::string name;
    int decimals = 9;
    double price_usd = 0.0;
    std::chrono::system_clock::time_point first_liquidity_25k;
    bool has_sufficient_liquidity = false;
};

struct OHLCVBar {
    std::string pool_id;
    std::chrono::system_clock::time_point timestamp;
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    double volume = 0.0;
    int interval_minutes = 5;
};

struct JupiterRoute {
    std::string input_mint;
    std::string output_mint;
    std::vector<std::string> route_plan;
    int hop_count = 0;
    double price_impact_pct = 0.0;
    bool is_healthy = true;
    std::chrono::system_clock::time_point last_checked;
};

struct MarketUpdate {
    std::string pool_id;
    std::string event_type; // "pool_update", "ohlcv_bar", "route_health"
    std::string data_json;
    std::chrono::system_clock::time_point timestamp;
};
