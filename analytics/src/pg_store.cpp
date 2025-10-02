#include "pg_store.hpp"
#include <pqxx/pqxx>
#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>

class PostgresStore::Impl {
public:
    Impl(const Config& config) 
        : config_(config), backoff_ms_(1000), retry_count_(0) {
        connect();
    }

    ~Impl() {
        disconnect();
    }

    bool connect() {
        try {
            conn_ = std::make_unique<pqxx::connection>(config_.pg_dsn);
            if (conn_->is_open()) {
                spdlog::info("Connected to PostgreSQL database");
                backoff_ms_ = 1000;  // Reset backoff on successful connection
                retry_count_ = 0;
                return true;
            } else {
                spdlog::error("PostgreSQL connection is not open");
                return false;
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to connect to PostgreSQL: {}", e.what());
            return false;
        }
    }

    void disconnect() {
        if (conn_ && conn_->is_open()) {
            conn_->close();
            conn_.reset();
            spdlog::info("Disconnected from PostgreSQL database");
        }
    }

    bool is_connected() const {
        return conn_ && conn_->is_open();
    }

    bool ensure_connection() {
        if (is_connected()) {
            return true;
        }

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_connection_attempt_).count() < backoff_ms_) {
            return false;
        }

        last_connection_attempt_ = now;
        
        try {
            if (connect()) {
                spdlog::info("PostgreSQL connection restored");
                return true;
            }
        } catch (const std::exception& e) {
            spdlog::warn("PostgreSQL reconnection failed (attempt {}): {}", ++retry_count_, e.what());
        }

        // Exponential backoff with cap
        backoff_ms_ = std::min(backoff_ms_ * 2, 30000);
        return false;
    }

    std::optional<PortfolioSnapshot> get_portfolio(const std::string& wallet_address) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        // Check cache first
        auto cache_it = portfolio_cache_.find(wallet_address);
        if (cache_it != portfolio_cache_.end()) {
            auto now = std::chrono::system_clock::now();
            if (std::chrono::duration_cast<std::chrono::minutes>(
                    now - cache_it->second.timestamp).count() < 5) {
                return cache_it->second;
            }
        }
        
        if (!ensure_connection()) {
            return std::nullopt;
        }

        try {
            pqxx::work txn(*conn_);
            
            // Get wallet SOL balance and total value
            pqxx::result wallet_result = txn.exec_params(
                "SELECT sol_balance, total_value_usd, updated_at "
                "FROM wallets "
                "WHERE address = $1",
                wallet_address
            );
            
            if (wallet_result.empty()) {
                return std::nullopt;
            }
            
            PortfolioSnapshot snapshot;
            snapshot.wallet_address = wallet_address;
            snapshot.sol_balance = wallet_result[0]["sol_balance"].as<double>();
            snapshot.total_value_usd = wallet_result[0]["total_value_usd"].as<double>();
            
            // Parse timestamp
            std::string ts_str = wallet_result[0]["updated_at"].as<std::string>();
            snapshot.timestamp = parse_iso8601(ts_str);
            
            // Get holdings
            pqxx::result holdings_result = txn.exec_params(
                "SELECT h.mint, t.symbol, h.amount, h.value_usd, h.entry_price, h.first_acquired "
                "FROM holdings h "
                "JOIN tokens t ON h.mint = t.mint "
                "WHERE h.wallet_address = $1",
                wallet_address
            );
            
            for (const auto& row : holdings_result) {
                TokenHolding holding;
                holding.mint = row["mint"].as<std::string>();
                holding.symbol = row["symbol"].as<std::string>();
                holding.amount = row["amount"].as<double>();
                holding.value_usd = row["value_usd"].as<double>();
                holding.entry_price = row["entry_price"].as<double>();
                
                // Parse timestamp
                std::string acq_ts_str = row["first_acquired"].as<std::string>();
                holding.first_acquired = parse_iso8601(acq_ts_str);
                
                snapshot.holdings.push_back(holding);
            }
            
            txn.commit();
            
            // Update cache
            portfolio_cache_[wallet_address] = snapshot;
            
            return snapshot;
        } catch (const std::exception& e) {
            spdlog::error("Error fetching portfolio: {}", e.what());
            return std::nullopt;
        }
    }

    std::optional<TokenMetadata> get_token_metadata(const std::string& mint) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        // Check cache first
        auto cache_it = token_cache_.find(mint);
        if (cache_it != token_cache_.end()) {
            auto now = std::chrono::system_clock::now();
            if (std::chrono::duration_cast<std::chrono::minutes>(
                    now - cache_it->second.first_liquidity_ts).count() < 30) {
                return cache_it->second;
            }
        }
        
        if (!ensure_connection()) {
            return std::nullopt;
        }

        try {
            pqxx::work txn(*conn_);
            
            pqxx::result result = txn.exec_params(
                "SELECT t.symbol, t.name, t.decimals, t.on_token_list, "
                "t.top_holder_pct, t.risky_authorities, t.first_liquidity_ts "
                "FROM tokens t "
                "WHERE t.mint = $1",
                mint
            );
            
            if (result.empty()) {
                return std::nullopt;
            }
            
            TokenMetadata metadata;
            metadata.mint = mint;
            metadata.symbol = result[0]["symbol"].as<std::string>();
            metadata.name = result[0]["name"].as<std::string>();
            metadata.decimals = result[0]["decimals"].as<int>();
            metadata.on_token_list = result[0]["on_token_list"].as<bool>();
            metadata.top_holder_pct = result[0]["top_holder_pct"].as<double>();
            metadata.risky_authorities = result[0]["risky_authorities"].as<bool>();
            
            // Parse timestamp
            std::string ts_str = result[0]["first_liquidity_ts"].as<std::string>();
            metadata.first_liquidity_ts = parse_iso8601(ts_str);
            
            txn.commit();
            
            // Update cache
            token_cache_[mint] = metadata;
            
            return metadata;
        } catch (const std::exception& e) {
            spdlog::error("Error fetching token metadata: {}", e.what());
            return std::nullopt;
        }
    }

    std::vector<std::string> get_token_list_mints() {
        if (!ensure_connection()) {
            return {};
        }

        try {
            pqxx::work txn(*conn_);
            
            pqxx::result result = txn.exec(
                "SELECT mint FROM tokens WHERE on_token_list = true"
            );
            
            std::vector<std::string> mints;
            for (const auto& row : result) {
                mints.push_back(row["mint"].as<std::string>());
            }
            
            txn.commit();
            return mints;
        } catch (const std::exception& e) {
            spdlog::error("Error fetching token list mints: {}", e.what());
            return {};
        }
    }

    void clear_caches() {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        portfolio_cache_.clear();
        token_cache_.clear();
    }

private:
    std::chrono::system_clock::time_point parse_iso8601(const std::string& ts_str) {
        std::tm tm = {};
        std::istringstream ss(ts_str);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
        return std::chrono::system_clock::from_time_t(std::mktime(&tm));
    }

    const Config& config_;
    std::unique_ptr<pqxx::connection> conn_;
    
    // Reconnection logic
    std::chrono::steady_clock::time_point last_connection_attempt_ = std::chrono::steady_clock::now();
    int backoff_ms_;
    int retry_count_;
    
    // Caches
    std::mutex cache_mutex_;
    std::unordered_map<std::string, PortfolioSnapshot> portfolio_cache_;
    std::unordered_map<std::string, TokenMetadata> token_cache_;
};

// PostgresStore implementation using the Impl class
PostgresStore::PostgresStore(const Config& config) : impl_(std::make_unique<Impl>(config)) {}

PostgresStore::~PostgresStore() = default;

bool PostgresStore::connect() {
    return impl_->connect();
}

void PostgresStore::disconnect() {
    impl_->disconnect();
}

bool PostgresStore::is_connected() const {
    return impl_->is_connected();
}

bool PostgresStore::ensure_connection() {
    return impl_->ensure_connection();
}

std::optional<PortfolioSnapshot> PostgresStore::get_portfolio(const std::string& wallet_address) {
    return impl_->get_portfolio(wallet_address);
}

std::optional<TokenMetadata> PostgresStore::get_token_metadata(const std::string& mint) {
    return impl_->get_token_metadata(mint);
}

std::vector<std::string> PostgresStore::get_token_list_mints() {
    return impl_->get_token_list_mints();
}

void PostgresStore::clear_caches() {
    impl_->clear_caches();
}
