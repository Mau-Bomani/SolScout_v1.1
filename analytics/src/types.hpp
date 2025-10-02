
#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <optional>
#include <nlohmann/json.hpp>

// Forward declarations
struct MarketUpdate;
struct PortfolioSnapshot;
struct TokenMetadata;
struct SignalResult;
struct AlertData;

// Market update from ingestor
struct OHLCVBar {
    double open;
    double high;
    double low;
    double close;
    double volume_usd;
};

struct RouteInfo {
    bool ok;
    int hops;
    double deviation_pct;
};

struct MarketUpdate {
    std::string pool_id;
    std::string mint_base;
    std::string mint_quote;
    std::string symbol;
    double price;
    double liq_usd;
    double vol24h_usd;
    double spread_pct;
    double impact_1pct_pct;
    double age_hours;
    RouteInfo route;
    std::map<std::string, OHLCVBar> bars;
    std::string data_quality;
    std::chrono::system_clock::time_point timestamp;
    
    static std::optional<MarketUpdate> from_json(const nlohmann::json& j);
};

// Portfolio data
struct TokenHolding {
    std::string mint;
    std::string symbol;
    double amount;
    double value_usd;
    double entry_price;
    std::chrono::system_clock::time_point first_acquired;
};

struct PortfolioSnapshot {
    std::string wallet_address;
    double sol_balance;
    double total_value_usd;
    std::vector<TokenHolding> holdings;
    std::chrono::system_clock::time_point timestamp;
};

// Token metadata
struct TokenMetadata {
    std::string mint;
    std::string symbol;
    std::string name;
    int decimals;
    bool on_token_list;
    double top_holder_pct;
    bool risky_authorities;
    std::chrono::system_clock::time_point first_liquidity_ts;
};

// Signal calculation result
struct SignalResult {
    double s1_liquidity;
    double s2_volume;
    double s3_momentum_1h;
    double s4_momentum_24h;
    double s5_volatility;
    double s6_price_discovery;
    double s7_rug_risk;
    double s8_tradability;
    double s9_relative_strength;
    double s10_route_quality;
    double n1_hygiene;
    
    double data_quality;
    int confidence_score;
    std::vector<std::string> reasons;
    std::string band;
    bool entry_confirmed;
    bool net_edge_ok;
    
    std::string to_string() const;
};

// Alert data
struct AlertData {
    std::string severity;
    std::string symbol;
    double price;
    int confidence;
    std::vector<std::string> lines;
    std::string plan;
    std::string sol_path;
    double est_impact_pct;
    std::chrono::system_clock::time_point timestamp;
    
    nlohmann::json to_json() const;
};

// Command request/reply
struct CommandRequest {
    std::string type;
    std::string cmd;
    nlohmann::json args;
    nlohmann::json from;
    std::string corr_id;
    std::chrono::system_clock::time_point timestamp;
    
    static std::optional<CommandRequest> from_json(const nlohmann::json& j);
};

struct CommandReply {
    std::string corr_id;
    bool ok;
    std::string message;
    nlohmann::json data;
    std::chrono::system_clock::time_point timestamp;
    
    nlohmann::json to_json() const;
};

// Signal item for API response
struct SignalItem {
    std::string symbol;
    int confidence;
    std::string band;
    std::vector<std::string> reasons;
    
    nlohmann::json to_json() const;
};
