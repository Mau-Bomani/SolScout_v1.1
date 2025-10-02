
#pragma once
#include "config.hpp"
#include <string>
#include <memory>

struct TokenInfo {
    std::string symbol;
    std::string name;
    std::string image_url;
};

class PriceClient {
public:
    explicit PriceClient(const Config& config);
    ~PriceClient();

    // Price operations
    double get_token_price(const std::string& mint_address);
    double get_sol_price();
    TokenInfo get_token_info(const std::string& mint_address);

    // Health check
    bool is_healthy() const;

    // Non-copyable
    PriceClient(const PriceClient&) = delete;
    PriceClient& operator=(const PriceClient&) = delete;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};
