#pragma once

#include "config.hpp"
#include "types.hpp"
#include <pqxx/pqxx>
#include <memory>
#include <vector>
#include <string>
#include <mutex>
#include <optional>

class DatabaseManager {
public:
    explicit DatabaseManager(const Config& config);
    ~DatabaseManager();
    
    // Initialize database tables if they don't exist
    bool initialize_schema();
    
    // Save a snapshot of all pools
    bool save_pool_snapshot(const std::vector<PoolInfo>& pools);
    
    // Save OHLCV bars
    bool save_ohlcv_bars(const std::vector<OHLCVBar>& bars);
    
    // Save token information
    bool save_tokens(const std::vector<TokenInfo>& tokens);
    
    // Get the latest OHLCV bar for a pool
    std::optional<OHLCVBar> get_latest_ohlcv(const std::string& pool_id, int interval_minutes);
    
    // Check database connection health
    bool check_health();

private:
    // Helper method to get a connection from the pool
    std::unique_ptr<pqxx::connection> get_connection();
    
    // Helper method to execute a query with retry logic
    template<typename Func>
    bool execute_with_retry(Func query_func, const std::string& operation_name, int max_retries = 3);
    
    const Config& config_;
    std::mutex mutex_;
    std::vector<std::unique_ptr<pqxx::connection>> connection_pool_;
};
