
#include "portfolio_service.hpp"
#include "redis_bus.hpp"
#include "database_manager.hpp"
#include "solana_client.hpp"
#include "price_client.hpp"
#include "health_checker.hpp"
#include "util.hpp"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/locale.h>
#include <algorithm>
#include <iomanip>

PortfolioService::PortfolioService(const Config& config)
    : config_(config),
      running_(false) {
    
    // Initialize components
    db_manager_ = std::make_unique<DatabaseManager>(config);
    solana_client_ = std::make_unique<SolanaClient>(config);
    price_client_ = std::make_unique<PriceClient>(config);
    redis_bus_ = std::make_unique<RedisBus>(config);
    health_checker_ = std::make_unique<HealthChecker>(config.health_check_host, config.health_check_port);
}

PortfolioService::~PortfolioService() {
    stop();
    if (service_thread_.joinable()) {
        service_thread_.join();
    }
}

void PortfolioService::run() {
    service_thread_ = std::thread([this]() {
        spdlog::info("Starting Portfolio Service...");
        running_ = true;

        health_checker_->start();

        redis_bus_->subscribe(config_.redis_cmd_channel, [this](const std::string& msg) {
            on_command_request(msg);
        });

        spdlog::info("Portfolio Service started successfully.");
        
        while(running_) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        redis_bus_->stop_subscriber();
        health_checker_->stop();
        spdlog::info("Portfolio Service event loop finished.");
    });
}

void PortfolioService::stop() {
    running_ = false;
}

void PortfolioService::on_command_request(const std::string& message) {
    try {
        auto json_msg = nlohmann::json::parse(message);
        auto request = CommandRequest::from_json(json_msg);
        
        spdlog::info("Received command: {} from user: {}", request.cmd, request.from.tg_user_id);
        
        // Route to appropriate handler
        if (request.cmd == "balance") {
            handle_balance(request);
        } else if (request.cmd == "holdings") {
            handle_holdings(request);
        } else if (request.cmd == "add_wallet") {
            handle_add_wallet(request);
        } else if (request.cmd == "remove_wallet") {
            handle_remove_wallet(request);
        } else {
            spdlog::warn("Unknown command: {}", request.cmd);
            send_reply(request.corr_id, "❌ Unknown command.");
        }
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to process command request: {}", e.what());
    }
}

void PortfolioService::handle_balance(const CommandRequest& req) {
    try {
        auto wallets = db_manager_->get_user_wallets(req.from.tg_user_id);
        
        if (wallets.empty()) {
            send_reply(req.corr_id, "No wallets tracked. Use /add_wallet <address> to add one.");
            return;
        }

        double total_usd_value = 0.0;
        double total_sol_balance = 0.0;
        std::string response = "💰 Wallet Balances\n\n";

        for (const auto& wallet : wallets) {
            // Get SOL balance
            double sol_balance = solana_client_->get_sol_balance(wallet);
            total_sol_balance += sol_balance;
            
            // Get SOL price
            double sol_price = price_client_->get_sol_price();
            double sol_usd_value = sol_balance * sol_price;
            total_usd_value += sol_usd_value;

            // Get token balances
            auto token_accounts = solana_client_->get_token_accounts(wallet);
            double wallet_token_value = 0.0;
            
            for (const auto& token : token_accounts) {
                if (token.amount > 0) {
                    double price = price_client_->get_token_price(token.mint);
                    wallet_token_value += token.amount * price;
                }
            }
            
            total_usd_value += wallet_token_value;
            
            // Format wallet summary
            response += fmt::format("📍 {}...{}\n", 
                                  wallet.substr(0, 4), 
                                  wallet.substr(wallet.length() - 4));
            response += fmt::format("  SOL: {:.4f} (${:.2f})\n", sol_balance, sol_usd_value);
            response += fmt::format("  Tokens: ${:.2f}\n", wallet_token_value);
            response += fmt::format("  Total: ${:.2f}\n\n", sol_usd_value + wallet_token_value);
        }

        // Add totals
        response += "📊 Portfolio Summary\n";
        response += fmt::format("Total SOL: {:.4f}\n", total_sol_balance);
        response += fmt::format("Total Value: ${:.2f}\n", total_usd_value);
        response += fmt::format("Updated: {}", util::current_iso8601());

        send_reply(req.corr_id, response);
        publish_audit("balance_check", req.from, fmt::format("${:.2f} total value", total_usd_value));

    } catch (const std::exception& e) {
        spdlog::error("Balance command failed: {}", e.what());
        send_reply(req.corr_id, "❌ Failed to get balances. Please try again.");
    }
}

void PortfolioService::handle_holdings(const CommandRequest& req) {
    try {
        auto wallets = db_manager_->get_user_wallets(req.from.tg_user_id);
        
        if (wallets.empty()) {
            send_reply(req.corr_id, "No wallets tracked. Use /add_wallet <address> to add one.");
            return;
        }

        struct TokenHolding {
            std::string symbol;
            double amount;
            double usd_value;
        };

        std::vector<TokenHolding> all_holdings;

        for (const auto& wallet : wallets) {
            auto holdings = solana_client_->get_token_accounts(wallet);
            for (const auto& holding : holdings) {
                if (holding.amount > 0) {
                    auto price = price_client_->get_token_price(holding.mint);
                    auto token_info = price_client_->get_token_info(holding.mint);
                    
                    all_holdings.push_back({
                        token_info.symbol,
                        holding.amount,
                        holding.amount * price
                    });
                }
            }
        }

        // Sort by USD value descending
        std::sort(all_holdings.begin(), all_holdings.end(),
                  [](const TokenHolding& a, const TokenHolding& b) {
                      return a.usd_value > b.usd_value;
                  });

        std::string response = "📊 Top Holdings\n\n";
        const int max_display = 10;
        
        for (int i = 0; i < std::min(max_display, (int)all_holdings.size()); i++) {
            const auto& holding = all_holdings[i];
            response += fmt::format(
                "{}. {} {:.4f} (${:.2f})\n",
                i + 1, holding.symbol, holding.amount, holding.usd_value
            );
        }

        if (all_holdings.size() > max_display) {
            response += fmt::format("\n... and {} more positions", all_holdings.size() - max_display);
        }

        response += fmt::format("\nUpdated: {}", util::current_iso8601());

        send_reply(req.corr_id, response);
        publish_audit("holdings_check", req.from, fmt::format("{} positions", all_holdings.size()));

    } catch (const std::exception& e) {
        spdlog::error("Holdings command failed: {}", e.what());
        send_reply(req.corr_id, "❌ Failed to get holdings. Please try again.");
    }
}

void PortfolioService::handle_add_wallet(const CommandRequest& req) {
    try {
        auto it = req.args.find("address");
        if (it == req.args.end()) {
            send_reply(req.corr_id, "❌ Missing wallet address. Usage: /add_wallet <address>");
            return;
        }

        const std::string& address = it->second;
        
        // Validate address format
        if (!util::is_valid_address(address)) {
            send_reply(req.corr_id, "❌ Invalid wallet address format.");
            return;
        }

        // Check if wallet already exists
        auto existing_wallets = db_manager_->get_user_wallets(req.from.tg_user_id);
        for (const auto& wallet : existing_wallets) {
            if (wallet == address) {
                send_reply(req.corr_id, "⚠️ Wallet already being tracked.");
                return;
            }
        }

        // Test wallet accessibility
        double sol_balance = solana_client_->get_sol_balance(address);
        if (sol_balance < 0) {
            send_reply(req.corr_id, "❌ Unable to access wallet. Please check the address.");
            return;
        }

        // Add wallet to database
        if (db_manager_->add_user_wallet(req.from.tg_user_id, address)) {
            std::string response = fmt::format("✅ Wallet added successfully!\n"
                                             "Address: {}...{}\n"
                                             "SOL Balance: {:.4f}",
                                             address.substr(0, 8),
                                             address.substr(address.length() - 8),
                                             sol_balance);
            send_reply(req.corr_id, response);
            publish_audit("wallet_added", req.from, fmt::format("Added wallet: {}", address));
        } else {
            send_reply(req.corr_id, "❌ Failed to add wallet. Please try again.");
        }

    } catch (const std::exception& e) {
        spdlog::error("Add wallet command failed: {}", e.what());
        send_reply(req.corr_id, "❌ Failed to add wallet. Please try again.");
    }
}

void PortfolioService::handle_remove_wallet(const CommandRequest& req) {
    try {
        auto it = req.args.find("address");
        if (it == req.args.end()) {
            send_reply(req.corr_id, "❌ Missing wallet address. Usage: /remove_wallet <address>");
            return;
        }

        const std::string& address = it->second;
        
        // Check if wallet exists
        auto existing_wallets = db_manager_->get_user_wallets(req.from.tg_user_id);
        bool found = false;
        for (const auto& wallet : existing_wallets) {
            if (wallet == address) {
                found = true;
                break;
            }
        }

        if (!found) {
            send_reply(req.corr_id, "⚠️ Wallet not found in your tracked wallets.");
            return;
        }

        // Remove wallet from database
        if (db_manager_->remove_user_wallet(req.from.tg_user_id, address)) {
            std::string response = fmt::format("✅ Wallet removed successfully!\n"
                                             "Address: {}...{}",
                                             address.substr(0, 8),
                                             address.substr(address.length() - 8));
            send_reply(req.corr_id, response);
            publish_audit("wallet_removed", req.from, fmt::format("Removed wallet: {}", address));
        } else {
            send_reply(req.corr_id, "❌ Failed to remove wallet. Please try again.");
        }

    } catch (const std::exception& e) {
        spdlog::error("Remove wallet command failed: {}", e.what());
        send_reply(req.corr_id, "❌ Failed to remove wallet. Please try again.");
    }
}

void PortfolioService::send_reply(const std::string& corr_id, const std::string& message) {
    try {
        CommandReply reply;
        reply.corr_id = corr_id;
        reply.message = message;
        reply.ts = util::current_iso8601();
        
        redis_bus_->publish_command_reply(reply);
        spdlog::debug("Sent reply for correlation ID: {}", corr_id);
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to send reply: {}", e.what());
    }
}

void PortfolioService::publish_audit(const std::string& event, const Actor& actor, const std::string& detail) {
    try {
        Audit audit_event;
        audit_event.event = event;
        audit_event.actor = actor;
        audit_event.detail = detail;
        audit_event.ts = util::current_iso8601();
        
        redis_bus_->publish_audit_event(audit_event);
        spdlog::debug("Published audit event: {}", event);
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to publish audit event: {}", e.what());
    }
}
