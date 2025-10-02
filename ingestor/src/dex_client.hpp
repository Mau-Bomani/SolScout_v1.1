
#pragma once

#include "config.hpp"
#include "types.hpp"
#include <vector>
#include <memory>
#include <string>
#include <functional>
#include <random>
#include <chrono>

class DexClient {
public:
    explicit DexClient(const Config& config);
    ~DexClient();

    // Fetches pools from all configured DEXs (Raydium, Orca)
    std::vector<PoolInfo> fetch_pools();
    
    // Fetch a specific pool by ID
    std::optional<PoolInfo> fetch_pool_by_id(const std::string& pool_id);
    
    // Fetch pools for a specific token
    std::vector<PoolInfo> fetch_pools_by_token(const std::string& token_mint);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};
