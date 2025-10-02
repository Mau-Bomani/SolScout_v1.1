#include "price_client.hpp"
#include "util.hpp"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <mutex>
#include <chrono>

class PriceClient::Impl {
public:
    Impl(const Config& config) : config_(config) {
        // Initialize well-known token info
        initialize_known_tokens();
        spdlog::info("Price client initialized");
    }

    double get_token_price(const std::string& mint_address) {
        try {
            // Check cache first
            {
                std::lock_guard<std::mutex> lock(cache_mutex_);
                auto it = price_cache_.find(mint_address);
                if (it != price_cache_.end()) {
                    auto now = std::chrono::steady_clock::now();
                    if (now - it->second.timestamp < std::chrono::minutes(5)) {
                        return it->second.price;
                    }
                }
            }

            // Fetch from Jupiter API
            double price = fetch_price_from_jupiter(mint_address);
            
            // Cache the result
            {
                std::lock_guard<std::mutex> lock(cache_mutex_);
                price_cache_[mint_address] = {price, std::chrono::steady_clock::now()};
            }

            return price;

        } catch (const std::exception& e) {
            spdlog::error("Failed to get token price for {}: {}", mint_address, e.what());
            return 0.0;
        }
    }

    double get_sol_price() {
        // SOL mint address
        const std::string sol_mint = "So11111111111111111111111111111111111111112";
        return get_token_price(sol_mint);
    }

    TokenInfo get_token_info(const std::string& mint_address) {
        try {
            // Check known tokens first
            {
                std::lock_guard<std::mutex> lock(cache_mutex_);
                auto it = token_info_cache_.find(mint_address);
                if (it != token_info_cache_.end()) {
                    return it->second;
                }
            }

            // Fetch from Jupiter API
            TokenInfo info = fetch_token_info_from_jupiter(mint_address);
            
            // Cache the result
            {
                std::lock_guard<std::mutex> lock(cache_mutex_);
                token_info_cache_[mint_address] = info;
            }

            return info;

        } catch (const std::exception& e) {
            spdlog::error("Failed to get token info for {}: {}", mint_address, e.what());
            return {"UNKNOWN", "Unknown Token", ""};
        }
    }

    bool is_healthy() const {
        try {
            // Test with SOL price fetch
            httplib::SSLClient client("price.jup.ag");
            client.set_connection_timeout(5, 0);
            client.set_read_timeout(10, 0);

            auto response = client.Get("/v4/price?ids=So11111111111111111111111111111111111111112");
            return response && response->status == 200;

        } catch (const std::exception& e) {
            spdlog::error("Price client health check failed: {}", e.what());
            return false;
        }
    }

private:
    struct PriceCacheEntry {
        double price;
        std::chrono::steady_clock::time_point timestamp;
    };

    void initialize_known_tokens() {
        // Add well-known Solana tokens
        token_info_cache_["So11111111111111111111111111111111111111112"] = {"SOL", "Solana", ""};
        token_info_cache_["EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v"] = {"USDC", "USD Coin", ""};
        token_info_cache_["Es9vMFrzaCERmJfrF4H2FYD4KCoNkY11McCe8BenwNYB"] = {"USDT", "Tether USD", ""};
        token_info_cache_["mSoLzYCxHdYgdzU16g5QSh3i5K3z3KZK7ytfqcJm7So"] = {"mSOL", "Marinade staked SOL", ""};
        token_info_cache_["7dHbWXmci3dT8UFYWYZweBLXgycu7Y3iL6trKn1Y7ARj"] = {"stSOL", "Lido Staked SOL", ""};
    }

    double fetch_price_from_jupiter(const std::string& mint_address) {
        try {
            httplib::SSLClient client("price.jup.ag");
            client.set_connection_timeout(10, 0);
            client.set_read_timeout(15, 0);

            std::string path = "/v4/price?ids=" + mint_address;
            auto response = client.Get(path.c_str());

            if (!response || response->status != 200) {
                spdlog::warn("Jupiter price API returned status {}", response ? response->status : 0);
                return 0.0;
            }

            auto json_response = nlohmann::json::parse(response->body);
            
            if (json_response.contains("data") && 
                json_response["data"].contains(mint_address) &&
                json_response["data"][mint_address].contains("price")) {
                
                double price = json_response["data"][mint_address]["price"].get<double>();
                spdlog::debug("Fetched price for {}: ${}", mint_address, price);
                return price;
            }

            spdlog::warn("Price not found in Jupiter response for {}", mint_address);
            return 0.0;

        } catch (const std::exception& e) {
            spdlog::error("Jupiter price fetch failed for {}: {}", mint_address, e.what());
            return 0.0;
        }
    }

    TokenInfo fetch_token_info_from_jupiter(const std::string& mint_address) {
        try {
            httplib::SSLClient client("token.jup.ag");
            client.set_connection_timeout(10, 0);
            client.set_read_timeout(15, 0);

            std::string path = "/strict/" + mint_address;
            auto response = client.Get(path.c_str());

            if (!response || response->status != 200) {
                spdlog::warn("Jupiter token API returned status {}", response ? response->status : 0);
                return {"UNKNOWN", "Unknown Token", ""};
            }

            auto json_response = nlohmann::json::parse(response->body);
            
            TokenInfo info;
            info.symbol = json_response.value("symbol", "UNKNOWN");
            info.name = json_response.value("name", "Unknown Token");
            info.image_url = json_response.value("logoURI", "");

            spdlog::debug("Fetched token info for {}: {} ({})", mint_address, info.symbol, info.name);
            return info;

        } catch (const std::exception& e) {
            spdlog::error("Jupiter token info fetch failed for {}: {}", mint_address, e.what());
            return {"UNKNOWN", "Unknown Token", ""};
        }
    }

    Config config_;
    std::unordered_map<std::string, PriceCacheEntry> price_cache_;
    std::unordered_map<std::string, TokenInfo> token_info_cache_;
    mutable std::mutex cache_mutex_;
};

// Public interface implementation
PriceClient::PriceClient(const Config& config) 
    : pImpl_(std::make_unique<Impl>(config)) {}

PriceClient::~PriceClient() = default;

double PriceClient::get_token_price(const std::string& mint_address) {
    return pImpl_->get_token_price(mint_address);
}

double PriceClient::get_sol_price() {
    return pImpl_->get_sol_price();
}

TokenInfo PriceClient::get_token_info(const std::string& mint_address) {
    return pImpl_->get_token_info(mint_address);
}

bool PriceClient::is_healthy() const {
    return pImpl_->is_healthy();
}