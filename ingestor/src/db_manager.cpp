#include "db_manager.hpp"
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>
#include <random>

DatabaseManager::DatabaseManager(const Config& config)
    : config_(config) {
    // Initialize connection pool
    try {
        for (int i = 0; i < 5; ++i) { // Create 5 connections in the pool
            auto conn = std::make_unique<pqxx::connection>(config_.db_conn_string);
            connection_pool_.push_back(std::move(conn));
        }
        spdlog::info("Database connection pool initialized with {} connections", connection_pool_.size());
    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize database connection pool: {}", e.what());
    }
}

DatabaseManager::~DatabaseManager() {
    // Connections will be automatically closed when their unique_ptrs are destroyed
}

bool DatabaseManager::initialize_schema() {
    return execute_with_retry([this]() {
        auto conn = get_connection();
        pqxx::work txn(*conn);
        
        // Create tokens table
        txn.exec(R"(
            CREATE TABLE IF NOT EXISTS tokens (
                address TEXT PRIMARY KEY,
                symbol TEXT NOT NULL,
                name TEXT,
                decimals INTEGER NOT NULL,
                price_usd DOUBLE PRECISION,
                first_seen TIMESTAMP WITH TIME ZONE,
                last_updated TIMESTAMP WITH TIME ZONE DEFAULT NOW()
            )
        )");
        
        // Create pools table
        txn.exec(R"(
            CREATE TABLE IF NOT EXISTS pools (
                pool_id TEXT PRIMARY KEY,
                dex_name TEXT NOT NULL,
                pool_type TEXT NOT NULL,
                token_a_address TEXT NOT NULL REFERENCES tokens(address),
                token_b_address TEXT NOT NULL REFERENCES tokens(address),
                reserve_a DOUBLE PRECISION NOT NULL,
                reserve_b DOUBLE PRECISION NOT NULL,
                tvl_usd DOUBLE PRECISION,
                volume_24h_usd DOUBLE PRECISION,
                price_token_a_in_b DOUBLE PRECISION,
                price_token_b_in_a DOUBLE PRECISION,
                price_impact_1pct DOUBLE PRECISION,
                last_updated TIMESTAMP WITH TIME ZONE DEFAULT NOW()
            )
        )");
        
        // Create OHLCV table
        txn.exec(R"(
            CREATE TABLE IF NOT EXISTS ohlcv_bars (
                pool_id TEXT NOT NULL REFERENCES pools(pool_id),
                timestamp TIMESTAMP WITH TIME ZONE NOT NULL,
                interval_minutes INTEGER NOT NULL,
                open DOUBLE PRECISION NOT NULL,
                high DOUBLE PRECISION NOT NULL,
                low DOUBLE PRECISION NOT NULL,
                close DOUBLE PRECISION NOT NULL,
                volume_usd DOUBLE PRECISION,
                tvl_usd DOUBLE PRECISION,
                base_token TEXT NOT NULL,
                quote_token TEXT NOT NULL,
                PRIMARY KEY (pool_id, timestamp, interval_minutes)
            )
        )");
        
        // Create index on timestamp for efficient time-based queries
        txn.exec("CREATE INDEX IF NOT EXISTS idx_ohlcv_timestamp ON ohlcv_bars(timestamp)");
        
        // Create jupiter routes table
        txn.exec(R"(
            CREATE TABLE IF NOT EXISTS jupiter_routes (
                input_token TEXT NOT NULL,
                output_token TEXT NOT NULL,
                hop_count INTEGER NOT NULL,
                price_impact_pct DOUBLE PRECISION,
                in_amount DOUBLE PRECISION NOT NULL,
                out_amount DOUBLE PRECISION NOT NULL,
                is_healthy BOOLEAN NOT NULL,
                timestamp TIMESTAMP WITH TIME ZONE NOT NULL,
                PRIMARY KEY (input_token, output_token, timestamp)
            )
        )");
        
        txn.commit();
        return true;
    }, "initialize_schema");
}

bool DatabaseManager::save_pool_snapshot(const std::vector<PoolInfo>& pools) {
    if (pools.empty()) {
        spdlog::debug("No pools to save in snapshot");
        return true;
    }
    
    return execute_with_retry([this, &pools]() {
        auto conn = get_connection();
        pqxx::work txn(*conn);
        
        // First, extract and save all tokens
        std::unordered_map<std::string, TokenInfo> tokens;
        for (const auto& pool : pools) {
            tokens[pool.token_a.address] = pool.token_a;
            tokens[pool.token_b.address] = pool.token_b;
        }
        
        // Prepare token insert statement
        std::string token_sql = 
            "INSERT INTO tokens (address, symbol, name, decimals, price_usd, first_seen, last_updated) "
            "VALUES ($1, $2, $3, $4, $5, $6, NOW()) "
            "ON CONFLICT (address) DO UPDATE SET "
            "symbol = EXCLUDED.symbol, "
            "name = EXCLUDED.name, "
            "decimals = EXCLUDED.decimals, "
            "price_usd = EXCLUDED.price_usd, "
            "last_updated = NOW()";
        
        // Insert tokens
        for (const auto& [address, token] : tokens) {
            txn.exec_params(
                token_sql,
                address,
                token.symbol,
                token.name,
                token.decimals,
                token.price_usd ? *token.price_usd : pqxx::null,
                token.first_seen ? pqxx::to_string(std::chrono::system_clock::to_time_t(*token.first_seen)) : pqxx::null
            );
        }
        
        // Prepare pool insert statement
        std::string pool_sql = 
            "INSERT INTO pools (pool_id, dex_name, pool_type, token_a_address, token_b_address, "
            "reserve_a, reserve_b, tvl_usd, volume_24h_usd, price_token_a_in_b, "
            "price_token_b_in_a, price_impact_1pct, last_updated) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, NOW()) "
            "ON CONFLICT (pool_id) DO UPDATE SET "
            "dex_name = EXCLUDED.dex_name, "
            "pool_type = EXCLUDED.pool_type, "
            "token_a_address = EXCLUDED.token_a_address, "
            "token_b_address = EXCLUDED.token_b_address, "
            "reserve_a = EXCLUDED.reserve_a, "
            "reserve_b = EXCLUDED.reserve_b, "
            "tvl_usd = EXCLUDED.tvl_usd, "
            "volume_24h_usd = EXCLUDED.volume_24h_usd, "
            "price_token_a_in_b = EXCLUDED.price_token_a_in_b, "
            "price_token_b_in_a = EXCLUDED.price_token_b_in_a, "
            "price_impact_1pct = EXCLUDED.price_impact_1pct, "
            "last_updated = NOW()";
        
        // Insert pools
        for (const auto& pool : pools) {
            txn.exec_params(
                pool_sql,
                pool.pool_id,
                pool.dex_name,
                pool.pool_type,
                pool.token_a.address,
                pool.token_b.address,
                pool.reserve_a,
                pool.reserve_b,
                pool.tvl_usd,
                pool.volume_24h_usd,
                pool.price_token_a_in_b,
                pool.price_token_b_in_a,
                pool.price_impact_1pct
            );
        }
        
        txn.commit();
        spdlog::info("Saved snapshot of {} pools to database", pools.size());
        return true;
    }, "save_pool_snapshot");
}

bool DatabaseManager::save_ohlcv_bars(const std::vector<OHLCVBar>& bars) {
    if (bars.empty()) {
        return true;
    }
    
    return execute_with_retry([this, &bars]() {
        auto conn = get_connection();
        pqxx::work txn(*conn);
        
        std::string sql = 
            "INSERT INTO ohlcv_bars (pool_id, timestamp, interval_minutes, open, high, low, close, "
            "volume_usd, tvl_usd, base_token, quote_token) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11) "
            "ON CONFLICT (pool_id, timestamp, interval_minutes) DO UPDATE SET "
            "open = EXCLUDED.open, "
            "high = EXCLUDED.high, "
            "low = EXCLUDED.low, "
            "close = EXCLUDED.close, "
            "volume_usd = EXCLUDED.volume_usd, "
            "tvl_usd = EXCLUDED.tvl_usd";
        
        for (const auto& bar : bars) {
            txn.exec_params(
                sql,
                bar.pool_id,
                pqxx::to_string(std::chrono::system_clock::to_time_t(bar.timestamp)),
                bar.interval_minutes,
                bar.open,
                bar.high,
                bar.low,
                bar.close,
                bar.volume_usd,
                bar.tvl_usd,
                bar.base_token,
                bar.quote_token
            );
        }
        
        txn.commit();
        spdlog::info("Saved {} OHLCV bars to database", bars.size());
        return true;
    }, "save_ohlcv_bars");
}

bool DatabaseManager::save_tokens(const std::vector<TokenInfo>& tokens) {
    if (tokens.empty()) {
        return true;
    }
    
    return execute_with_retry([this, &tokens]() {
        auto conn = get_connection();
        pqxx::work txn(*conn);
        
        std::string sql = 
            "INSERT INTO tokens (address, symbol, name, decimals, price_usd, first_seen, last_updated) "
            "VALUES ($1, $2, $3, $4, $5, $6, NOW()) "
            "ON CONFLICT (address) DO UPDATE SET "
            "symbol = EXCLUDED.symbol, "
            "name = EXCLUDED.name, "
            "decimals = EXCLUDED.decimals, "
            "price_usd = EXCLUDED.price_usd, "
            "last_updated = NOW()";
        
        for (const auto& token : tokens) {
            txn.exec_params(
                sql,
                token.address,
                token.symbol,
                token.name,
                token.decimals,
                token.price_usd ? *token.price_usd : pqxx::null,
                token.first_seen ? pqxx::to_string(std::chrono::system_clock::to_time_t(*token.first_seen)) : pqxx::null
            );
        }
        
        txn.commit();
        spdlog::info("Saved {} tokens to database", tokens.size());
        return true;
    }, "save_tokens");
}

std::optional<OHLCVBar> DatabaseManager::get_latest_ohlcv(const std::string& pool_id, int interval_minutes) {
    try {
        auto conn = get_connection();
        pqxx::work txn(*conn);
        
        auto result = txn.exec_params(
            "SELECT pool_id, timestamp, interval_minutes, open, high, low, close, "
            "volume_usd, tvl_usd, base_token, quote_token "
            "FROM ohlcv_bars "
            "WHERE pool_id = $1 AND interval_minutes = $2 "
            "ORDER BY timestamp DESC "
            "LIMIT 1",
            pool_id,
            interval_minutes
        );
        
        if (result.empty()) {
            return std::nullopt;
        }
        
        const auto& row = result[0];
        OHLCVBar bar;
        bar.pool_id = row["pool_id"].as<std::string>();
        bar.timestamp = std::chrono::system_clock::from_time_t(row["timestamp"].as<time_t>());
        bar.interval_minutes = row["interval_minutes"].as<int>();
        bar.open = row["open"].as<double>();
        bar.high = row["high"].as<double>();
        bar.low = row["low"].as<double>();
        bar.close = row["close"].as<double>();
        bar.volume_usd = row["volume_usd"].is_null() ? 0.0 : row["volume_usd"].as<double>();
        bar.tvl_usd = row["tvl_usd"].is_null() ? 0.0 : row["tvl_usd"].as<double>();
        bar.base_token = row["base_token"].as<std::string>();
        bar.quote_token = row["quote_token"].as<std::string>();
        
        return bar;
    } catch (const std::exception& e) {
        spdlog::error("Error getting latest OHLCV: {}", e.what());
        return std::nullopt;
    }
}

bool DatabaseManager::check_health() {
    try {
        auto conn = get_connection();
        pqxx::work txn(*conn);
        auto result = txn.exec1("SELECT 1");
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Database health check failed: {}", e.what());
        return false;
    }
}

std::unique_ptr<pqxx::connection> DatabaseManager::get_connection() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if we have any connections in the pool
    if (!connection_pool_.empty()) {
        auto conn = std::move(connection_pool_.back());
        connection_pool_.pop_back();
        
        // Check if the connection is still valid
        try {
            if (conn->is_open()) {
                return conn;
            }
        } catch (...) {
            // Connection is not valid, create a new one
        }
    }
    
    // Create a new connection
    return std::make_unique<pqxx::connection>(config_.db_conn_string);
}

template<typename Func>
bool DatabaseManager::execute_with_retry(Func query_func, const std::string& operation_name, int max_retries) {
    int attempts = 0;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> jitter(0.1, 0.3);
    
    while (attempts < max_retries) {
        try {
            return query_func();
        } catch (const pqxx::broken_connection& e) {
            // Connection issue, retry
            spdlog::error("Database connection error during {}: {}", operation_name, e.what());
        } catch (const pqxx::sql_error& e) {
            // SQL error, might be temporary
            spdlog::error("SQL error during {}: {}", operation_name, e.what());
        } catch (const std::exception& e) {
            // Other error
            spdlog::error("Error during {}: {}", operation_name, e.what());
        }
        
        ++attempts;
        if (attempts >= max_retries) {
            spdlog::error("Max retry attempts reached for {}", operation_name);
            return false;
        }
        
        // Exponential backoff with jitter
        double backoff_seconds = std::pow(2.0, attempts) * (1.0 + jitter(gen));
        spdlog::info("Retrying {} in {:.2f} seconds (attempt {}/{})", 
                    operation_name, backoff_seconds, attempts + 1, max_retries);
        std::this_thread::sleep_for(std::chrono::milliseconds(
            static_cast<int>(backoff_seconds * 1000)));
    }
    
    return false;
}
