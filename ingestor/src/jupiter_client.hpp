
#pragma once

#include "config.hpp"
#include "types.hpp"
#include <vector>
#include <memory>
#include <string>
#include <optional>

class JupiterClient {
public:
    explicit JupiterClient(const Config& config);
    ~JupiterClient();
    
    // Get a quote for swapping between two tokens
    std::optional<JupiterRoute> get_quote(
        const std::string& input_mint,
        const std::string& output_mint,
        double amount_in
    );
    
    // Get route health for a token pair
    bool check_route_health(
        const std::string& input_mint,
        const std::string& output_mint
    );
    
    // Get normalized USD price for a token using USDC/USDT as reference
    std::optional<double> get_usd_price(const std::string& token_mint);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};
