
#pragma once
#include "config.hpp"
#include <string>
#include <vector>
#include <memory>

struct TokenAccount {
    std::string mint;
    double amount;
    int decimals;
};

class SolanaClient {
public:
    explicit SolanaClient(const Config& config);
    ~SolanaClient();

    // Account operations
    std::vector<TokenAccount> get_token_accounts(const std::string& wallet_address);
    double get_sol_balance(const std::string& wallet_address);

    // Health check
    bool is_healthy() const;

    // Non-copyable
    SolanaClient(const SolanaClient&) = delete;
    SolanaClient& operator=(const SolanaClient&) = delete;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};
