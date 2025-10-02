#include "dex_client.hpp"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <thread>
#include <algorithm>
#include <cmath>

class DexClient::Impl {
public:
    explicit Impl(const Config& config) 
        : config_(config), 
          rng_(std::random_device{}()),
          backoff_seconds_(config.base_backoff_seconds) {}

    std::vector<PoolInfo> fetch_pools() {
        std::vector<PoolInfo> all_pools;
        
        // Fetch from Raydium
        auto raydium_pools = fetch_raydium_pools();
        all_pools.insert(all_pools.end(), raydium_pools.begin(), raydium_pools.end());
        
        // Fetch from Orca
        auto orca_pools = fetch_orca_pools();
        all_pools.insert(all_pools.end(), orca_pools.begin(), orca_pools.end());
        
        spdlog::info("Fetched a total of {} pools from all DEXs", all_pools.size());
        
        // Filter pools by TVL and volume thresholds
        all_pools.erase(
            std::remove_if(all_pools.begin(), all_pools.end(), 
                [this](const PoolInfo& pool) {
                    return pool.tvl_usd < config_.min_tvl_threshold || 
                           pool.volume_24h_usd < config_.min_volume_threshold;
                }
            ),
            all_pools.end()
        );
        
        spdlog::info("{} pools remain after filtering by TVL and volume thresholds", all_pools.size());
        
        // Calculate additional metrics for each pool
        for (auto& pool : all_pools) {
            calculate_additional_metrics(pool);
        }
        
        return all_pools;
    }
    
    std::optional<PoolInfo> fetch_pool_by_id(const std::string& pool_id) {
        // Try Raydium first
        auto raydium_pool = fetch_raydium_pool_by_id(pool_id);
        if (raydium_pool) {
            calculate_additional_metrics(*raydium_pool);
            return raydium_pool;
        }
        
        // Try Orca if not found in Raydium
        auto orca_pool = fetch_orca_pool_by_id(pool_id);
        if (orca_pool) {
            calculate_additional_metrics(*orca_pool);
            return orca_pool;
        }
        
        return std::nullopt;
    }
    
    std::vector<PoolInfo> fetch_pools_by_token(const std::string& token_mint) {
        std::vector<PoolInfo> token_pools;
        
        // Fetch all pools and filter by token
        auto all_pools = fetch_pools();
        
        for (const auto& pool : all_pools) {
            if (pool.token_a.address == token_mint || pool.token_b.address == token_mint) {
                token_pools.push_back(pool);
            }
        }
        
        return token_pools;
    }

private:
    std::vector<PoolInfo> fetch_raydium_pools() {
        std::vector<PoolInfo> pools;
        try {
            spdlog::debug("Fetching Raydium pools...");
            
            // Prepare request
            auto url = config_.raydium_api_url + "/pools";
            
            // Make the request with proper error handling and backoff
            auto response = make_request_with_backoff([&]() {
                return cpr::Get(
                    cpr::Url{url},
                    cpr::Timeout{30000},
                    cpr::Header{{"User-Agent", "SoulScout/1.1"}}
                );
            });
            
            if (response.error) {
                spdlog::error("Failed to fetch Raydium pools: {}", response.error.message);
                return pools;
            }
            
            if (response.status_code != 200) {
                spdlog::error("Failed to fetch Raydium pools, status: {}", response.status_code);
                return pools;
            }
            
            auto json_res = nlohmann::json::parse(response.text);
            
            // Check if the response has the expected structure
            if (!json_res.contains("success") || !json_res["success"].get<bool>() || !json_res.contains("data")) {
                spdlog::error("Unexpected Raydium API response format");
                return pools;
            }
            
            const auto& data = json_res["data"];
            
            for (const auto& item : data) {
                if (!item.is_object()) continue;
                
                try {
                    PoolInfo pool;
                    pool.pool_id = item.value("id", "");
                    pool.dex_name = "Raydium";
                    pool.pool_type = "constant-product"; // Default for Raydium
                    
                    // Token A info
                    if (item.contains("baseMint") && item.contains("baseSymbol")) {
                        pool.token_a.address = item["baseMint"].get<std::string>();
                        pool.token_a.symbol = item["baseSymbol"].get<std::string>();
                        pool.token_a.decimals = item.value("baseDecimals", 0);
                    }
                    
                    // Token B info
                    if (item.contains("quoteMint") && item.contains("quoteSymbol")) {
                        pool.token_b.address = item["quoteMint"].get<std::string>();
                        pool.token_b.symbol = item["quoteSymbol"].get<std::string>();
                        pool.token_b.decimals = item.value("quoteDecimals", 0);
                    }
                    
                    // Pool metrics
                    pool.tvl_usd = item.value("liquidity", 0.0);
                    pool.volume_24h_usd = item.value("volume24h", 0.0);
                    pool.price_token_a_in_b = item.value("price", 0.0);
                    
                    // Calculate price_token_b_in_a if price_token_a_in_b is valid
                    if (pool.price_token_a_in_b > 0) {
                        pool.price_token_b_in_a = 1.0 / pool.price_token_a_in_b;
                    }
                    
                    // Reserve data if available
                    if (item.contains("baseReserve") && item.contains("quoteReserve")) {
                        pool.reserve_a = std::stod(item["baseReserve"].get<std::string>());
                        pool.reserve_b = std::stod(item["quoteReserve"].get<std::string>());
                    }
                    
                    pool.last_updated = std::chrono::system_clock::now();
                    
                    // Store raw data for debugging
                    pool.raw_data_json = item.dump();
                    
                    if (!pool.pool_id.empty()) {
                        pools.push_back(pool);
                    }
                } catch (const std::exception& e) {
                    spdlog::warn("Error parsing Raydium pool data: {}", e.what());
                }
            }
            
            spdlog::info("Fetched {} pools from Raydium", pools.size());
            
            // Reset backoff on success
            backoff_seconds_ = config_.base_backoff_seconds;
            
        } catch (const std::exception& e) {
            spdlog::error("Exception while fetching Raydium pools: {}", e.what());
            increase_backoff();
        }
        
        return pools;
    }
    
    std::optional<PoolInfo> fetch_raydium_pool_by_id(const std::string& pool_id) {
        try {
            spdlog::debug("Fetching Raydium pool by ID: {}", pool_id);
            
            // Prepare request
            auto url = config_.raydium_api_url + "/pool/" + pool_id;
            
            // Make the request with proper error handling and backoff
            auto response = make_request_with_backoff([&]() {
                return cpr::Get(
                    cpr::Url{url},
                    cpr::Timeout{30000},
                    cpr::Header{{"User-Agent", "SoulScout/1.1"}}
                );
            });
            
            if (response.error || response.status_code != 200) {
                return std::nullopt;
            }
            
            auto json_res = nlohmann::json::parse(response.text);
            
            // Check if the response has the expected structure
            if (!json_res.contains("success") || !json_res["success"].get<bool>() || !json_res.contains("data")) {
                return std::nullopt;
            }
            
            const auto& item = json_res["data"];
            
            PoolInfo pool;
            pool.pool_id = item.value("id", "");
            pool.dex_name = "Raydium";
            pool.pool_type = "constant-product"; // Default for Raydium
            
            // Token A info
            if (item.contains("baseMint") && item.contains("baseSymbol")) {
                pool.token_a.address = item["baseMint"].get<std::string>();
                pool.token_a.symbol = item["baseSymbol"].get<std::string>();
                pool.token_a.decimals = item.value("baseDecimals", 0);
            }
            
            // Token B info
            if (item.contains("quoteMint") && item.contains("quoteSymbol")) {
                pool.token_b.address = item["quoteMint"].get<std::string>();
                pool.token_b.symbol = item["quoteSymbol"].get<std::string>();
                pool.token_b.decimals = item.value("quoteDecimals", 0);
            }
            
            // Pool metrics
            pool.tvl_usd = item.value("liquidity", 0.0);
            pool.volume_24h_usd = item.value("volume24h", 0.0);
            pool.price_token_a_in_b = item.value("price", 0.0);
            
            // Calculate price_token_b_in_a if price_token_a_in_b is valid
            if (pool.price_token_a_in_b > 0) {
                pool.price_token_b_in_a = 1.0 / pool.price_token_a_in_b;
            }
            
            // Reserve data if available
            if (item.contains("baseReserve") && item.contains("quoteReserve")) {
                pool.reserve_a = std::stod(item["baseReserve"].get<std::string>());
                pool.reserve_b = std::stod(item["quoteReserve"].get<std::string>());
            }
            
            pool.last_updated = std::chrono::system_clock::now();
            
            // Store raw data for debugging
            pool.raw_data_json = item.dump();
            
            if (!pool.pool_id.empty()) {
                return pool;
            }
            
        } catch (const std::exception& e) {
            spdlog::error("Exception while fetching Raydium pool by ID: {}", e.what());
        }
        
        return std::nullopt;
    }

    std::vector<PoolInfo> fetch_orca_pools() {
        std::vector<PoolInfo> pools;
        try {
            spdlog::debug("Fetching Orca pools...");
            
            // Prepare request
            auto url = config_.orca_api_url + "/allPools";
            
            // Make the request with proper error handling and backoff
            auto response = make_request_with_backoff([&]() {
                return cpr::Get(
                    cpr::Url{url},
                    cpr::Timeout{30000},
                    cpr::Header{{"User-Agent", "SoulScout/1.1"}}
                );
            });
            
            if (response.error) {
                spdlog::error("Failed to fetch Orca pools: {}", response.error.message);
                return pools;
            }
            
            if (response.status_code != 200) {
                spdlog::error("Failed to fetch Orca pools, status: {}", response.status_code);
                return pools;
            }
            
            auto json_res = nlohmann::json::parse(response.text);
            
            for (const auto& item : json_res) {
                if (!item.is_object()) continue;
                
                try {
                    PoolInfo pool;
                    pool.pool_id = item.value("address", "");
                    pool.dex_name = "Orca";
                    
                    // Determine pool type
                    std::string pool_type = item.value("poolType", "");
                    if (pool_type == "STABLE") {
                        pool.pool_type = "stable";
                    } else {
                        pool.pool_type = "constant-product";
                    }
                    
                    // Token A info
                    if (item.contains("tokenA")) {
                        const auto& token_a = item["tokenA"];
                        pool.token_a.address = token_a.value("mint", "");
                        pool.token_a.symbol = token_a.value("symbol", "");
                        pool.token_a.decimals = token_a.value("decimals", 0);
                    }
                    
                    // Token B info
                    if (item.contains("tokenB")) {
                        const auto& token_b = item["tokenB"];
                        pool.token_b.address = token_b.value("mint", "");
                        pool.token_b.symbol = token_b.value("symbol", "");
                        pool.token_b.decimals = token_b.value("decimals", 0);
                    }
                    
                    // Pool metrics
                    pool.tvl_usd = item.value("liquidity", 0.0);
                    pool.volume_24h_usd = item.value("volume24h", 0.0);
                    
                    // Price data
                    if (item.contains("price")) {
                        pool.price_token_a_in_b = item["price"].get<double>();
                        
                        // Calculate price_token_b_in_a if price_token_a_in_b is valid
                        if (pool.price_token_a_in_b > 0) {
                            pool.price_token_b_in_a = 1.0 / pool.price_token_a_in_b;
                        }
                    }
                    
                    // Reserve data
                    if (item.contains("reserves")) {
                        const auto& reserves = item["reserves"];
                        pool.reserve_a = reserves.value("tokenA", 0.0);
                        pool.reserve_b = reserves.value("tokenB", 0.0);
                    }
                    
                    pool.last_updated = std::chrono::system_clock::now();
                    
                    // Store raw data for debugging
                    pool.raw_data_json = item.dump();
                    
                    if (!pool.pool_id.empty()) {
                        pools.push_back(pool);
                    }
                } catch (const std::exception& e) {
                    spdlog::warn("Error parsing Orca pool data: {}", e.what());
                }
            }
            
            spdlog::info("Fetched {} pools from Orca", pools.size());
            
            // Reset backoff on success
            backoff_seconds_ = config_.base_backoff_seconds;
            
        } catch (const std::exception& e) {
            spdlog::error("Exception while fetching Orca pools: {}", e.what());
            increase_backoff();
        }
        
        return pools;
    }
    
    std::optional<PoolInfo> fetch_orca_pool_by_id(const std::string& pool_id) {
        try {
            spdlog::debug("Fetching Orca pool by ID: {}", pool_id);
            
            // Prepare request
            auto url = config_.orca_api_url + "/pool/" + pool_id;
            
            // Make the request with proper error handling and backoff
            auto response = make_request_with_backoff([&]() {
                return cpr::Get(
                    cpr::Url{url},
                    cpr::Timeout{30000},
                    cpr::Header{{"User-Agent", "SoulScout/1.1"}}
                );
            });
            
            if (response.error || response.status_code != 200) {
                return std::nullopt;
            }
            
            auto json_res = nlohmann::json::parse(response.text);
            
            PoolInfo pool;
            pool.pool_id = json_res.value("address", "");
            pool.dex_name = "Orca";
            
            // Determine pool type
            std::string pool_type = json_res.value("poolType", "");
            if (pool_type == "STABLE") {
                pool.pool_type = "stable";
            } else {
                pool.pool_type = "constant-product";
            }
            
            // Token A info
            if (json_res.contains("tokenA")) {
                const auto& token_a = json_res["tokenA"];
                pool.token_a.address = token_a.value("mint", "");
                pool.token_a.symbol = token_a.value("symbol", "");
                pool.token_a.decimals = token_a.value("decimals", 0);
            }
            
            // Token B info
            if (json_res.contains("tokenB")) {
                const auto& token_b = json_res["tokenB"];
                pool.token_b.address = token_b.value("mint", "");
                pool.token_b.symbol = token_b.value("symbol", "");
                pool.token_b.decimals = token_b.value("decimals", 0);
            }
            
            // Pool metrics
            pool.tvl_usd = json_res.value("liquidity", 0.0);
            pool.volume_24h_usd = json_res.value("volume24h", 0.0);
            
            // Price data
            if (json_res.contains("price")) {
                pool.price_token_a_in_b = json_res["price"].get<double>();
                
                // Calculate price_token_b_in_a if price_token_a_in_b is valid
                if (pool.price_token_a_in_b > 0) {
                    pool.price_token_b_in_a = 1.0 / pool.price_token_a_in_b;
                }
            }
            
            // Reserve data
            if (json_res.contains("reserves")) {
                const auto& reserves = json_res["reserves"];
                pool.reserve_a = reserves.value("tokenA", 0.0);
                pool.reserve_b = reserves.value("tokenB", 0.0);
            }
            
            pool.last_updated = std::chrono::system_clock::now();
            
            // Store raw data for debugging
            pool.raw_data_json = json_res.dump();
            
            if (!pool.pool_id.empty()) {
                return pool;
            }
            
        } catch (const std::exception& e) {
            spdlog::error("Exception while fetching Orca pool by ID: {}", e.what());
        }
        
        return std::nullopt;
    }
    
    // Helper method to calculate additional metrics for a pool
    void calculate_additional_metrics(PoolInfo& pool) {
        // Calculate fee rate based on pool type
        if (pool.pool_type == "stable") {
            pool.fee_rate = 0.003; // 0.3% for stable pools
        } else {
            pool.fee_rate = 0.003; // 0.3% for constant product pools
        }
        
        // Calculate price impact for 1% of liquidity
        if (pool.reserve_a > 0 && pool.reserve_b > 0) {
            // For constant product pools (x*y=k), calculate price impact
            if (pool.pool_type == "constant-product") {
                // Calculate k
                double k = pool.reserve_a * pool.reserve_b;
                
                // Calculate new reserves after 1% swap of token A
                double delta_a = pool.reserve_a * 0.01;
                double new_reserve_a = pool.reserve_a + delta_a;
                double new_reserve_b = k / new_reserve_a;
                double delta_b = pool.reserve_b - new_reserve_b;
                
                // Calculate price impact
                double initial_price = pool.reserve_b / pool.reserve_a;
                double new_price = new_reserve_b / new_reserve_a;
                pool.price_impact_1pct = std::abs((new_price - initial_price) / initial_price) * 100.0;
            }
            // For stable pools, use a simplified model
            else if (pool.pool_type == "stable") {
                // Stable pools have lower price impact, use a simplified estimate
                pool.price_impact_1pct = 0.1; // 0.1% as a conservative estimate
            }
        }
        
        // Calculate APR (simplified)
        if (pool.volume_24h_usd > 0 && pool.tvl_usd > 0) {
            pool.apr = (pool.volume_24h_usd * 365.0) / pool.tvl_usd;
        } else {
            pool.apr = 0.0;
        }
    }

    // Helper method to make HTTP requests with exponential backoff and jitter
    template<typename RequestFunc>
    cpr::Response make_request_with_backoff(RequestFunc request_func) {
        int attempts = 0;
        const int max_attempts = 5;
        
        while (attempts < max_attempts) {
            auto response = request_func();
            
            // Check if request was successful
            if (!response.error && (response.status_code == 200 || response.status_code == 201)) {
                return response;
            }
            
            // If we've reached max attempts, return the last response
            if (++attempts >= max_attempts) {
                spdlog::warn("Max request attempts reached");
                return response;
            }
            
            // Calculate backoff time with jitter
            double jitter = std::uniform_real_distribution<>(0.0, 0.3)(rng_);
            double backoff_with_jitter = backoff_seconds_ * (1.0 + jitter);
            
            spdlog::debug("Request failed, backing off for {:.2f} seconds (attempt {}/{})",
                         backoff_with_jitter, attempts, max_attempts);
            
            // Sleep for the calculated time
            std::this_thread::sleep_for(std::chrono::milliseconds(
                static_cast<int>(backoff_with_jitter * 1000)));
            
            // Increase backoff for next attempt
            increase_backoff();
        }
        
        // This should never be reached, but return an empty response just in case
        return cpr::Response();
    }
    
    // Helper method to increase backoff time
    void increase_backoff() {
        backoff_seconds_ = std::min(backoff_seconds_ * 2.0, config_.max_backoff_seconds);
    }

    const Config& config_;
    std::mt19937 rng_;
    double backoff_seconds_;
};

// --- PIMPL forward declarations ---
DexClient::DexClient(const Config& config) : pImpl_(std::make_unique<Impl>(config)) {}
DexClient::~DexClient() = default;
std::vector<PoolInfo> DexClient::fetch_pools() { return pImpl_->fetch_pools(); }
std::optional<PoolInfo> DexClient::fetch_pool_by_id(const std::string& pool_id) { return pImpl_->fetch_pool_by_id(pool_id); }
std::vector<PoolInfo> DexClient::fetch_pools_by_token(const std::string& token_mint) { return pImpl_->fetch_pools_by_token(token_mint); }
