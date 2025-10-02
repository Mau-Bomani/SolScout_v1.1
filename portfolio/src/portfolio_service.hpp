#pragma once
#include "config.hpp"
#include "json_schemas.hpp"
#include <atomic>
#include <thread>
#include <memory>

// Forward declarations for components
class RedisBus;
class DatabaseManager;
class SolanaClient;
class PriceClient;
class HealthChecker;

class PortfolioService {
public:
    PortfolioService(const Config& config);
    ~PortfolioService();

    void run();
    void stop();

private:
    void on_command_request(const std::string& message);

    // Command handlers
    void handle_add_wallet(const CommandRequest& request);
    void handle_remove_wallet(const CommandRequest& request);
    void handle_balance(const CommandRequest& request);
    void handle_holdings(const CommandRequest& request);

    void send_reply(const std::string& corr_id, const std::string& message);
    void publish_audit(const std::string& event, const Actor& actor, const std::string& detail);

    Config config_;
    std::atomic<bool> running_{false};
    std::unique_ptr<RedisBus> redis_bus_;
    std::unique_ptr<DatabaseManager> db_manager_;
    std::unique_ptr<SolanaClient> solana_client_;
    std::unique_ptr<PriceClient> price_client_;
    std::unique_ptr<HealthChecker> health_checker_;
    std::thread service_thread_;
};
