#include "database_manager.hpp"
#include <spdlog/spdlog.h>
#include <sqlite3.h>
#include <vector>
#include <string>
#include <mutex>

class DatabaseManager::Impl {
public:
    Impl(const Config& config) : config_(config), db_(nullptr) {
        if (!initialize()) {
            throw std::runtime_error("Failed to initialize database");
        }
    }

    ~Impl() {
        if (db_) {
            sqlite3_close(db_);
        }
    }

    bool initialize() {
        // Open database
        int rc = sqlite3_open(config_.db_path.c_str(), &db_);
        if (rc != SQLITE_OK) {
            spdlog::error("Cannot open database: {}", sqlite3_errmsg(db_));
            return false;
        }

        // Create tables
        if (!create_tables()) {
            return false;
        }

        spdlog::info("Database initialized at: {}", config_.db_path);
        return true;
    }

    std::vector<std::string> get_user_wallets(int64_t user_id) {
        std::lock_guard<std::mutex> lock(db_mutex_);
        std::vector<std::string> wallets;

        const char* sql = "SELECT wallet_address FROM user_wallets WHERE user_id = ?";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
            return wallets;
        }

        sqlite3_bind_int64(stmt, 1, user_id);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* wallet = (const char*)sqlite3_column_text(stmt, 0);
            if (wallet) {
                wallets.emplace_back(wallet);
            }
        }

        sqlite3_finalize(stmt);
        return wallets;
    }

    bool add_user_wallet(int64_t user_id, const std::string& wallet_address) {
        std::lock_guard<std::mutex> lock(db_mutex_);

        const char* sql = "INSERT OR IGNORE INTO user_wallets (user_id, wallet_address, created_at) VALUES (?, ?, datetime('now'))";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
            return false;
        }

        sqlite3_bind_int64(stmt, 1, user_id);
        sqlite3_bind_text(stmt, 2, wallet_address.c_str(), -1, SQLITE_STATIC);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) {
            spdlog::error("Failed to insert wallet: {}", sqlite3_errmsg(db_));
            return false;
        }

        spdlog::info("Added wallet {} for user {}", wallet_address, user_id);
        return true;
    }

    bool remove_user_wallet(int64_t user_id, const std::string& wallet_address) {
        std::lock_guard<std::mutex> lock(db_mutex_);

        const char* sql = "DELETE FROM user_wallets WHERE user_id = ? AND wallet_address = ?";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
            return false;
        }

        sqlite3_bind_int64(stmt, 1, user_id);
        sqlite3_bind_text(stmt, 2, wallet_address.c_str(), -1, SQLITE_STATIC);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) {
            spdlog::error("Failed to remove wallet: {}", sqlite3_errmsg(db_));
            return false;
        }

        int changes = sqlite3_changes(db_);
        if (changes == 0) {
            spdlog::warn("No wallet found to remove: {} for user {}", wallet_address, user_id);
            return false;
        }

        spdlog::info("Removed wallet {} for user {}", wallet_address, user_id);
        return true;
    }

    bool is_healthy() const {
        std::lock_guard<std::mutex> lock(db_mutex_);
        
        if (!db_) {
            return false;
        }

        // Simple health check - try to query sqlite_master
        const char* sql = "SELECT name FROM sqlite_master WHERE type='table' LIMIT 1";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        return rc == SQLITE_ROW || rc == SQLITE_DONE;
    }

private:
    bool create_tables() {
        const char* create_user_wallets = R"(
            CREATE TABLE IF NOT EXISTS user_wallets (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                user_id INTEGER NOT NULL,
                wallet_address TEXT NOT NULL,
                created_at TEXT NOT NULL,
                UNIQUE(user_id, wallet_address)
            )
        )";

        const char* create_portfolio_snapshots = R"(
            CREATE TABLE IF NOT EXISTS portfolio_snapshots (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                user_id INTEGER NOT NULL,
                wallet_address TEXT NOT NULL,
                snapshot_data TEXT NOT NULL,
                total_value_usd REAL NOT NULL,
                created_at TEXT NOT NULL
            )
        )";

        const char* create_indices = R"(
            CREATE INDEX IF NOT EXISTS idx_user_wallets_user_id ON user_wallets(user_id);
            CREATE INDEX IF NOT EXISTS idx_portfolio_snapshots_user_id ON portfolio_snapshots(user_id);
            CREATE INDEX IF NOT EXISTS idx_portfolio_snapshots_created_at ON portfolio_snapshots(created_at);
        )";

        char* err_msg = nullptr;

        // Create user_wallets table
        if (sqlite3_exec(db_, create_user_wallets, nullptr, nullptr, &err_msg) != SQLITE_OK) {
            spdlog::error("Failed to create user_wallets table: {}", err_msg);
            sqlite3_free(err_msg);
            return false;
        }

        // Create portfolio_snapshots table
        if (sqlite3_exec(db_, create_portfolio_snapshots, nullptr, nullptr, &err_msg) != SQLITE_OK) {
            spdlog::error("Failed to create portfolio_snapshots table: {}", err_msg);
            sqlite3_free(err_msg);
            return false;
        }

        // Create indices
        if (sqlite3_exec(db_, create_indices, nullptr, nullptr, &err_msg) != SQLITE_OK) {
            spdlog::error("Failed to create indices: {}", err_msg);
            sqlite3_free(err_msg);
            return false;
        }

        return true;
    }

    Config config_;
    sqlite3* db_;
    mutable std::mutex db_mutex_;
};

// Public interface implementation
DatabaseManager::DatabaseManager(const Config& config) 
    : pImpl_(std::make_unique<Impl>(config)) {}

DatabaseManager::~DatabaseManager() = default;

std::vector<std::string> DatabaseManager::get_user_wallets(int64_t user_id) {
    return pImpl_->get_user_wallets(user_id);
}

bool DatabaseManager::add_user_wallet(int64_t user_id, const std::string& wallet_address) {
    return pImpl_->add_user_wallet(user_id, wallet_address);
}

bool DatabaseManager::remove_user_wallet(int64_t user_id, const std::string& wallet_address) {
    return pImpl_->remove_user_wallet(user_id, wallet_address);
}

bool DatabaseManager::is_healthy() const {
    return pImpl_->is_healthy();
}
