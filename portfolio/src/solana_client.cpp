#include "solana_client.hpp"
#include "util.hpp"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>

struct TokenAccount {
    std::string mint;
    double amount;
    int decimals;
};

class SolanaClient::Impl {
public:
    Impl(const Config& config) : config_(config) {
        // Extract host and path from RPC URL
        if (config_.solana_rpc_url.find("https://") == 0) {
            auto url_without_protocol = config_.solana_rpc_url.substr(8);
            auto slash_pos = url_without_protocol.find('/');
            if (slash_pos != std::string::npos) {
                rpc_host_ = url_without_protocol.substr(0, slash_pos);
                rpc_path_ = url_without_protocol.substr(slash_pos);
            } else {
                rpc_host_ = url_without_protocol;
                rpc_path_ = "/";
            }
        } else {
            spdlog::error("Invalid Solana RPC URL format: {}", config_.solana_rpc_url);
            throw std::runtime_error("Invalid Solana RPC URL");
        }
        
        spdlog::info("Solana client configured for host: {}, path: {}", rpc_host_, rpc_path_);
    }

    std::vector<TokenAccount> get_token_accounts(const std::string& wallet_address) {
        try {
            // Create RPC request for getTokenAccountsByOwner
            nlohmann::json rpc_request = {
                {"jsonrpc", "2.0"},
                {"id", 1},
                {"method", "getTokenAccountsByOwner"},
                {"params", {
                    wallet_address,
                    {{"programId", "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA"}}, // SPL Token program
                    {
                        {"encoding", "jsonParsed"},
                        {"commitment", "confirmed"}
                    }
                }}
            };

            auto response = make_rpc_call(rpc_request);
            if (!response) {
                return {};
            }

            std::vector<TokenAccount> accounts;
            
            if (response->contains("result") && response->at("result").contains("value")) {
                for (const auto& account : response->at("result").at("value")) {
                    try {
                        const auto& parsed = account.at("account").at("data").at("parsed");
                        const auto& info = parsed.at("info");
                        
                        TokenAccount token_account;
                        token_account.mint = info.at("mint").get<std::string>();
                        token_account.decimals = info.at("tokenAmount").at("decimals").get<int>();
                        
                        // Convert amount from string to double, accounting for decimals
                        std::string amount_str = info.at("tokenAmount").at("amount").get<std::string>();
                        double raw_amount = std::stod(amount_str);
                        token_account.amount = raw_amount / std::pow(10, token_account.decimals);
                        
                        if (token_account.amount > 0) {
                            accounts.push_back(token_account);
                        }
                    } catch (const std::exception& e) {
                        spdlog::warn("Failed to parse token account: {}", e.what());
                        continue;
                    }
                }
            }

            spdlog::debug("Found {} token accounts for wallet {}", accounts.size(), wallet_address);
            return accounts;

        } catch (const std::exception& e) {
            spdlog::error("Failed to get token accounts for {}: {}", wallet_address, e.what());
            return {};
        }
    }

    double get_sol_balance(const std::string& wallet_address) {
        try {
            nlohmann::json rpc_request = {
                {"jsonrpc", "2.0"},
                {"id", 1},
                {"method", "getBalance"},
                {"params", {wallet_address, {{"commitment", "confirmed"}}}}
            };

            auto response = make_rpc_call(rpc_request);
            if (!response || !response->contains("result")) {
                return 0.0;
            }

            // SOL balance is returned in lamports (1 SOL = 1e9 lamports)
            uint64_t lamports = response->at("result").at("value").get<uint64_t>();
            double sol_balance = static_cast<double>(lamports) / 1e9;
            
            spdlog::debug("SOL balance for {}: {}", wallet_address, sol_balance);
            return sol_balance;

        } catch (const std::exception& e) {
            spdlog::error("Failed to get SOL balance for {}: {}", wallet_address, e.what());
            return 0.0;
        }
    }

    bool is_healthy() const {
        try {
            nlohmann::json rpc_request = {
                {"jsonrpc", "2.0"},
                {"id", 1},
                {"method", "getHealth"}
            };

            auto response = make_rpc_call(rpc_request);
            return response && response->contains("result") && 
                   response->at("result").get<std::string>() == "ok";

        } catch (const std::exception& e) {
            spdlog::error("Solana health check failed: {}", e.what());
            return false;
        }
    }

private:
    std::optional<nlohmann::json> make_rpc_call(const nlohmann::json& request) const {
        try {
            httplib::SSLClient client(rpc_host_);
            client.set_connection_timeout(10, 0);
            client.set_read_timeout(30, 0);

            httplib::Headers headers = {
                {"Content-Type", "application/json"}
            };

            auto response = client.Post(rpc_path_.c_str(), headers, request.dump(), "application/json");
            
            if (!response || response->status != 200) {
                spdlog::error("Solana RPC call failed with status: {}", response ? response->status : 0);
                return std::nullopt;
            }

            auto json_response = nlohmann::json::parse(response->body);
            
            if (json_response.contains("error")) {
                spdlog::error("Solana RPC error: {}", json_response["error"].dump());
                return std::nullopt;
            }

            return json_response;

        } catch (const std::exception& e) {
            spdlog::error("Solana RPC call exception: {}", e.what());
            return std::nullopt;
        }
    }

    Config config_;
    std::string rpc_host_;
    std::string rpc_path_;
};

// Public interface implementation
SolanaClient::SolanaClient(const Config& config) 
    : pImpl_(std::make_unique<Impl>(config)) {}

SolanaClient::~SolanaClient() = default;

std::vector<TokenAccount> SolanaClient::get_token_accounts(const std::string& wallet_address) {
    return pImpl_->get_token_accounts(wallet_address);
}

double SolanaClient::get_sol_balance(const std::string& wallet_address) {
    return pImpl_->get_sol_balance(wallet_address);
}

bool SolanaClient::is_healthy() const {
    return pImpl_->is_healthy();
}