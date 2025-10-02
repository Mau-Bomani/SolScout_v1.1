
#pragma once
#include "config.hpp"
#include <string>
#include <vector>
#include <memory>

class DatabaseManager {
public:
    explicit DatabaseManager(const Config& config);
    ~DatabaseManager();

    // Wallet management
    std::vector<std::string> get_user_wallets(int64_t user_id);
    bool add_user_wallet(int64_t user_id, const std::string& wallet_address);
    bool remove_user_wallet(int64_t user_id, const std::string& wallet_address);

    // Health check
    bool is_healthy() const;

    // Non-copyable
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};
