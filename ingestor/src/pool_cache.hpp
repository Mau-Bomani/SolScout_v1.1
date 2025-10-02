
#pragma once

#include "types.hpp"
#include "config.hpp"
#include <unordered_map>
#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <memory>

class PoolCache {
public:
    explicit PoolCache(const Config& config);
    
    // Add or update a pool in the cache
    void update_pool(const PoolInfo& pool);
    
    // Add or update multiple pools in the cache
    void update_pools(const std::vector<PoolInfo>& pools);
    
    // Get a pool by ID
    std::optional<PoolInfo> get_pool(const std::string& pool_id) const;
    
    // Get all pools in the cache
    std::vector<PoolInfo> get_all_pools() const;
    
    // Get pools for a specific token
    std::vector<PoolInfo> get_pools_by_token(const std::string& token_mint) const;
    
    // Get the number of pools in the cache
    size_t size() const;
    
    // Clear expired entries
    void cleanup_expired();
    
    // Clear the entire cache
    void clear();

private:
    struct CacheEntry {
        PoolInfo pool;
        std::chrono::steady_clock::time_point expiry;
    };
    
    const Config& config_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, CacheEntry> pools_;
    std::unordered_map<std::string, std::vector<std::string>> token_to_pools_;
};
