
#pragma once

#include "config.hpp"
#include "types.hpp"
#include <memory>
#include <vector>
#include <optional>
#include <string>
#include <chrono>
#include <mutex>
#include <unordered_map>

class PostgresStore {
public:
    explicit PostgresStore(const Config& config);
    ~PostgresStore();

    // Connection management
    bool connect();
    void disconnect();
    bool is_connected() const;
    bool ensure_connection();

    // Portfolio data
    std::optional<PortfolioSnapshot> get_portfolio(const std::string& wallet_address);
    
    // Token metadata
    std::optional<TokenMetadata> get_token_metadata(const std::string& mint);
    
    // Market data
    std::vector<std::string> get_token_list_mints();
    
    // Cache management
    void clear_caches();

    // Non-copyable
    PostgresStore(const PostgresStore&) = delete;
    PostgresStore& operator=(const PostgresStore&) = delete;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
