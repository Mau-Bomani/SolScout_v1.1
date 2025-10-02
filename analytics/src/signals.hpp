#pragma once

#include "types.hpp"
#include "config.hpp"
#include <optional>
#include <string>
#include <vector>
#include <map>

class SignalCalculator {
public:
    explicit SignalCalculator(const Config& config);
    
    // Calculate all signals for a market update
    SignalResult calculate_signals(
        const MarketUpdate& update,
        const std::optional<TokenMetadata>& metadata,
        const std::vector<std::string>& token_list_mints
    );
    
    // Individual signal calculations
    double calculate_s1_liquidity(const MarketUpdate& update);
    double calculate_s2_volume(const MarketUpdate& update);
    double calculate_s3_momentum_1h(const MarketUpdate& update);
    double calculate_s4_momentum_24h(const MarketUpdate& update);
    double calculate_s5_volatility(const MarketUpdate& update);
    double calculate_s6_price_discovery(const MarketUpdate& update);
    double calculate_s7_rug_risk(const MarketUpdate& update, const std::optional<TokenMetadata>& metadata);
    double calculate_s8_tradability(const MarketUpdate& update);
    double calculate_s9_relative_strength(const MarketUpdate& update);
    double calculate_s10_route_quality(const MarketUpdate& update);
    double calculate_n1_hygiene(const std::string& mint, const std::vector<std::string>& token_list_mints);
    
    // Data quality assessment
    double calculate_data_quality(const MarketUpdate& update);
    
    // Generate reasons for the signal result
    std::vector<std::string> generate_reasons(
        const MarketUpdate& update,
        const std::optional<TokenMetadata>& metadata,
        const SignalResult& result
    );

private:
    const Config& config_;
};
