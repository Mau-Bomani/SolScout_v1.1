
#include "pool_cache.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>

PoolCache::PoolCache(const Config& config)
    : config_(config) {
}

PoolCache::PoolCache(size_t max_size, std::chrono::minutes ttl)
    : max_size_(max_size), ttl_(ttl) {
}

PoolCache::PoolCache() = default;

PoolCache::PoolCache(int max_size, int ttl_minutes)
    : max_size_(max_size), ttl_(std::chrono::minutes(ttl_minutes)) {}

void PoolCache::put(const std::string& pool_id, const PoolInfo& pool_info) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_.find(pool_id);
    if (it != cache_.end()) {
        // Update existing entry
        it->second.pool_info = pool_info;
        it->second.timestamp = std::chrono::steady_clock::now();
        
        // Move to front of LRU list
        lru_list_.erase(it->second.lru_iterator);
        lru_list_.push_front(pool_id);
        it->second.lru_iterator = lru_list_.begin();
    } else {
        // Add new entry
        if (cache_.size() >= max_size_) {
            evict_lru();
        }
        
        lru_list_.push_front(pool_id);
        auto [cache_it, inserted] = cache_.emplace(pool_id, CacheEntry(pool_info));
        cache_it->second.lru_iterator = lru_list_.begin();
    }
}

std::optional<PoolInfo> PoolCache::get(const std::string& pool_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_.find(pool_id);
    if (it == cache_.end()) {
        miss_count_++;
        return std::nullopt;
    }
    
    // Check if expired
    if (is_expired(it->second)) {
        lru_list_.erase(it->second.lru_iterator);
        cache_.erase(it);
        miss_count_++;
        return std::nullopt;
    }
    
    // Move to front of LRU list
    lru_list_.erase(it->second.lru_iterator);
    lru_list_.push_front(pool_id);
    it->second.lru_iterator = lru_list_.begin();
    
    hit_count_++;
    return it->second.pool_info;
}

bool PoolCache::contains(const std::string& pool_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_.find(pool_id);
    if (it == cache_.end()) {
        return false;
    }
    
    return !is_expired(it->second);
}

void PoolCache::remove(const std::string& pool_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_.find(pool_id);
    if (it != cache_.end()) {
        lru_list_.erase(it->second.lru_iterator);
        cache_.erase(it);
    }
}

void PoolCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
    lru_list_.clear();
    hit_count_ = 0;
    miss_count_ = 0;
    pools_.clear();
    token_to_pools_.clear();
}

size_t PoolCache::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.size();
}

size_t PoolCache::capacity() const {
    return max_size_;
}

double PoolCache::hit_rate() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t total = hit_count_ + miss_count_;
    if (total == 0) return 0.0;
    
    return static_cast<double>(hit_count_) / total;
}

std::vector<std::string> PoolCache::get_active_pool_ids() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> active_pools;
    active_pools.reserve(cache_.size());
    
    for (const auto& [pool_id, entry] : cache_) {
        if (!is_expired(entry) && entry.pool_info.is_active) {
            active_pools.push_back(pool_id);
        }
    }
    
    return active_pools;
}

std::vector<PoolInfo> PoolCache::get_pools_by_tvl_threshold(double min_tvl) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<PoolInfo> pools;
    pools.reserve(cache_.size());
    
    for (const auto& [pool_id, entry] : cache_) {
        if (!is_expired(entry) && 
            entry.pool_info.is_active && 
            entry.pool_info.tvl_usd >= min_tvl) {
            pools.push_back(entry.pool_info);
        }
    }
    
    return pools;
}

void PoolCache::cleanup_expired() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_.begin();
    while (it != cache_.end()) {
        if (is_expired(it->second)) {
            lru_list_.erase(it->second.lru_iterator);
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
    
    auto now = std::chrono::steady_clock::now();
    
    // Remove expired entries
    for (auto it = pools_.begin(); it != pools_.end();) {
        if (it->second.expiry <= now) {
            it = pools_.erase(it);
        } else {
            ++it;
        }
    }
    
    // Rebuild token_to_pools_ mapping
    token_to_pools_.clear();
    for (const auto& entry : pools_) {
        const auto& pool = entry.second.pool;
        token_to_pools_[pool.token_a.address].push_back(pool.pool_id);
        token_to_pools_[pool.token_b.address].push_back(pool.pool_id);
    }
}

void PoolCache::evict_lru() {
    if (lru_list_.empty()) return;
    
    std::string lru_key = lru_list_.back();
    lru_list_.pop_back();
    cache_.erase(lru_key);
}

bool PoolCache::is_expired(const CacheEntry& entry) const {
    auto now = std::chrono::steady_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::minutes>(now - entry.timestamp);
    return age >= ttl_;
}

bool PoolCache::check_and_update(const PoolInfo& new_pool_info) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_.find(new_pool_info.pool_id);

    // Case 1: Pool is not in the cache (it's new)
    if (it == cache_.end()) {
        cache_[new_pool_info.pool_id] = new_pool_info;
        return true; // It's a new pool, so it has "changed"
    }

    const auto& old_pool_info = it->second;
    bool has_changed = false;

    // Case 2: Check for significant changes
    // Compare is_active status
    if (old_pool_info.is_active != new_pool_info.is_active) {
        has_changed = true;
    }

    // Compare TVL (avoid division by zero)
    if (old_pool_info.tvl_usd > 1e-9) {
        double tvl_change = std::abs(new_pool_info.tvl_usd - old_pool_info.tvl_usd) / old_pool_info.tvl_usd;
        if (tvl_change > TVL_CHANGE_THRESHOLD) {
            has_changed = true;
        }
    } else if (new_pool_info.tvl_usd > 1.0) { // Change from near-zero to something
        has_changed = true;
    }

    // Compare Volume (avoid division by zero)
    if (old_pool_info.volume_24h_usd > 1e-9) {
        double volume_change = std::abs(new_pool_info.volume_24h_usd - old_pool_info.volume_24h_usd) / old_pool_info.volume_24h_usd;
        if (volume_change > VOLUME_CHANGE_THRESHOLD) {
            has_changed = true;
        }
    } else if (new_pool_info.volume_24h_usd > 1.0) { // Change from near-zero to something
        has_changed = true;
    }

    // If any significant change was detected, update the cache with the new info
    if (has_changed) {
        cache_[new_pool_info.pool_id] = new_pool_info;
    }

    return has_changed;
}

std::vector<PoolInfo> PoolCache::get_all_pools() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<PoolInfo> all_pools;
    all_pools.reserve(cache_.size());
    for (const auto& pair : cache_) {
        all_pools.push_back(pair.second);
    }
    
    std::vector<PoolInfo> result;
    result.reserve(pools_.size());
    
    auto now = std::chrono::steady_clock::now();
    for (const auto& entry : pools_) {
        if (entry.second.expiry > now) {
            result.push_back(entry.second.pool);
        }
    }
    
    return result;
}

void PoolCache::update_cache(const std::vector<PoolInfo>& new_pools) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Clear existing cache entries
    cache_.clear();
    lru_list_.clear();
    
    // Add new pools to cache
    for (const auto& pool : new_pools) {
        if (cache_.size() >= max_size_) {
            evict_lru();
        }
        
        lru_list_.push_front(pool.pool_id);
        cache_[pool.pool_id] = CacheEntry(pool);
        cache_[pool.pool_id].lru_iterator = lru_list_.begin();
    }
}

bool PoolCache::check_and_update(const PoolInfo& new_pool_info) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = cache_.find(new_pool_info.id);

    if (it == cache_.end()) {
        // New pool, always update
        cache_[new_pool_info.id] = {new_pool_info, std::chrono::steady_clock::now()};
        return true;
    }

    // Existing pool, check for significant change
    const auto& cached_pool = it->second.pool;
    bool changed = (std::abs(cached_pool.price - new_pool_info.price) / cached_pool.price > 0.01) ||
                   (std::abs(cached_pool.tvl - new_pool_info.tvl) / cached_pool.tvl > 0.05);

    if (changed) {
        it->second.pool = new_pool_info;
        it->second.last_updated = std::chrono::steady_clock::now();
    }
    return changed;
}

std::vector<PoolInfo> PoolCache::get_all_pools() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    std::vector<PoolInfo> pools;
    pools.reserve(cache_.size());
    for (const auto& pair : cache_) {
        pools.push_back(pair.second.pool);
    }
    return pools;
}

void PoolCache::cleanup_expired() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    cleanup_expired_unlocked();
}

void PoolCache::cleanup_expired_unlocked() {
    auto now = std::chrono::steady_clock::now();
    for (auto it = cache_.begin(); it != cache_.end(); ) {
        if (now - it->second.last_updated > ttl_) {
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
}

void PoolCache::update_pool(const PoolInfo& pool) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Calculate expiry time
    auto expiry = std::chrono::steady_clock::now() + 
                  std::chrono::minutes(config_.pool_cache_ttl_minutes);
    
    // Update or insert the pool
    pools_[pool.pool_id] = {pool, expiry};
    
    // Update token to pools mapping
    token_to_pools_[pool.token_a.address].push_back(pool.pool_id);
    token_to_pools_[pool.token_b.address].push_back(pool.pool_id);
    
    // Ensure we don't exceed the max cache size
    if (pools_.size() > static_cast<size_t>(config_.pool_cache_max_size)) {
        cleanup_expired();
        
        // If still too large, remove oldest entries
        if (pools_.size() > static_cast<size_t>(config_.pool_cache_max_size)) {
            // Sort entries by expiry time
            std::vector<std::pair<std::string, std::chrono::steady_clock::time_point>> entries;
            for (const auto& entry : pools_) {
                entries.emplace_back(entry.first, entry.second.expiry);
            }
            
            // Sort by expiry (oldest first)
            std::sort(entries.begin(), entries.end(), 
                [](const auto& a, const auto& b) {
                    return a.second < b.second;
                });
            
            // Remove oldest entries until we're under the limit
            size_t to_remove = pools_.size() - config_.pool_cache_max_size;
            for (size_t i = 0; i < to_remove && i < entries.size(); ++i) {
                pools_.erase(entries[i].first);
            }
            
            // Rebuild token_to_pools_ mapping
            token_to_pools_.clear();
            for (const auto& entry : pools_) {
                const auto& pool = entry.second.pool;
                token_to_pools_[pool.token_a.address].push_back(pool.pool_id);
                token_to_pools_[pool.token_b.address].push_back(pool.pool_id);
            }
        }
    }
}

void PoolCache::update_pools(const std::vector<PoolInfo>& pools) {
    for (const auto& pool : pools) {
        update_pool(pool);
    }
}

std::optional<PoolInfo> PoolCache::get_pool(const std::string& pool_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = pools_.find(pool_id);
    if (it != pools_.end() && it->second.expiry > std::chrono::steady_clock::now()) {
        return it->second.pool;
    }
    
    return std::nullopt;
}

std::vector<PoolInfo> PoolCache::get_all_pools() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<PoolInfo> result;
    result.reserve(pools_.size());
    
    auto now = std::chrono::steady_clock::now();
    for (const auto& entry : pools_) {
        if (entry.second.expiry > now) {
            result.push_back(entry.second.pool);
        }
    }
    
    return result;
}

std::vector<PoolInfo> PoolCache::get_pools_by_token(const std::string& token_mint) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<PoolInfo> result;
    
    auto token_it = token_to_pools_.find(token_mint);
    if (token_it == token_to_pools_.end()) {
        return result;
    }
    
    auto now = std::chrono::steady_clock::now();
    for (const auto& pool_id : token_it->second) {
        auto pool_it = pools_.find(pool_id);
        if (pool_it != pools_.end() && pool_it->second.expiry > now) {
            result.push_back(pool_it->second.pool);
        }
    }
    
    return result;
}

size_t PoolCache::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pools_.size();
}
