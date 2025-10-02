#include "jupiter_client.hpp"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <thread>
#include <algorithm>
#include <random>

class JupiterClient::Impl {
public:
    explicit Impl(const Config& config) 
        : config_(config), 
          rng_(std::random_device{}()),
          backoff_seconds_(config.base_backoff_seconds) {
        // Initialize USDC and USDT mint addresses
        usdc_mint_ = "EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v"; // USDC mint on Solana
        usdt_mint_ = "Es9vMFrzaCERmJfrF4H2FYD4KCoNkY11McCe8BenwNYB"; // USDT mint on Solana
    }
    
    std::optional<JupiterRoute> get_quote(
        const std::string& input_mint,
        const std::string& output_mint,
        double amount_in
    ) {
        try {
            spdlog::debug("Getting Jupiter quote for {} -> {}, amount: {}", 
                         input_mint, output_mint, amount_in);
            
            // Convert amount_in to an integer (Jupiter expects amounts in lamports/smallest unit)
            uint64_t amount_in_lamports = static_cast<uint64_t>(amount_in * 1e9); // Assuming 9 decimals
            
            // Prepare request
            auto url = config_.jupiter_api_url + "/quote";
            
            nlohmann::json request_data = {
                {"inputMint", input_mint},
                {"outputMint", output_mint},
                {"amount", std::to_string(amount_in_lamports)},
                {"slippageBps", 50}, // 0.5% slippage
                {"onlyDirectRoutes", false},
                {"asLegacyTransaction", false}
            };
            
            // Make the request with proper error handling and backoff
            auto response = make_request_with_backoff([&]() {
                return cpr::Post(
                    cpr::Url{url},
                    cpr::Header{{"Content-Type", "application/json"}, {"User-Agent", "SoulScout/1.1"}},
                    cpr::Body{request_data.dump()},
                    cpr::Timeout{30000}
                );
            });
            
            if (response.error) {
                spdlog::error("Failed to get Jupiter quote: {}", response.error.message);
                return std::nullopt;
            }
            
            if (response.status_code != 200) {
                spdlog::error("Failed to get Jupiter quote, status: {}", response.status_code);
                return std::nullopt;
            }
            
            auto json_res = nlohmann::json::parse(response.text);
            
            // Extract route information
            JupiterRoute route;
            route.input_token = input_mint;
            route.output_token = output_mint;
            route.in_amount = amount_in;
            
            if (json_res.contains("outAmount")) {
                // Convert outAmount from string to double and from lamports to tokens
                route.out_amount = std::stod(json_res["outAmount"].get<std::string>()) / 1e9;
            }
            
            if (json_res.contains("priceImpactPct")) {
                route.price_impact_pct = json_res["priceImpactPct"].get<double>() * 100.0; // Convert to percentage
            }
            
            if (json_res.contains("routePlan") && json_res["routePlan"].is_array()) {
                route.hop_count = json_res["routePlan"].size();
            } else {
                route.hop_count = 1; // Default to 1 if routePlan is not available
            }
            
            route.is_healthy = true; // If we got a valid response, consider the route healthy
            route.timestamp = std::chrono::system_clock::now();
            
            // Reset backoff on success
            backoff_seconds_ = config_.base_backoff_seconds;
            
            return route;
            
        } catch (const std::exception& e) {
            spdlog::error("Exception while getting Jupiter quote: {}", e.what());
            increase_backoff();
            return std::nullopt;
        }
    }
    
    bool check_route_health(
        const std::string& input_mint,
        const std::string& output_mint
    ) {
        // Use a small amount for the health check
        auto route = get_quote(input_mint, output_mint, 0.1);
        return route.has_value() && route->is_healthy;
    }
    
    std::optional<double> get_price_in_usdc(const std::string& input_mint, long long amount_in_smallest_unit) {
        if (input_mint == usdc_mint_) {
            return 1.0;
        }
        
        // Try USDC first
        auto quote = get_quote(input_mint, usdc_mint_, static_cast<double>(amount_in_smallest_unit) / 1e6);
        if (quote.has_value()) {
            return quote->out_amount;
        }
        
        // If USDC fails, try USDT
        quote = get_quote(input_mint, usdt_mint_, static_cast<double>(amount_in_smallest_unit) / 1e6);
        if (quote.has_value()) {
            return quote->out_amount;
        }
        
        return std::nullopt;
    }
    
    std::optional<double> get_usd_price(const std::string& token_mint) {
        // If the token is already USDC or USDT, return 1.0
        if (token_mint == usdc_mint_ || token_mint == usdt_mint_) {
            return 1.0;
        }
        
        // Try to get a quote for token -> USDC
        auto usdc_quote = get_quote(token_mint, usdc_mint_, 1.0);
        if (usdc_quote && usdc_quote->out_amount > 0) {
            return usdc_quote->out_amount;
        }
        
        // If USDC quote failed, try USDT
        auto usdt_quote = get_quote(token_mint, usdt_mint_, 1.0);
        if (usdt_quote && usdt_quote->out_amount > 0) {
            return usdt_quote->out_amount;
        }
        
        // If both failed, try CoinGecko as a fallback if API key is available
        if (!config_.coingecko_api_key.empty()) {
            return get_price_from_coingecko(token_mint);
        }
        
        return std::nullopt;
    }

private:
    std::optional<double> get_price_from_coingecko(const std::string& token_mint) {
        try {
            // Map Solana token address to CoinGecko ID if needed
            // This is a simplified approach; in a real implementation, you might need a more comprehensive mapping
            
            // Prepare request
            auto url = config_.coingecko_api_url + "/simple/token_price/solana";
            
            // Make the request with proper error handling and backoff
            auto response = make_request_with_backoff([&]() {
                return cpr::Get(
                    cpr::Url{url},
                    cpr::Parameters{
                        {"contract_addresses", token_mint},
                        {"vs_currencies", "usd"}
                    },
                    cpr::Header{
                        {"X-CG-Pro-API-Key", config_.coingecko_api_key},
                        {"User-Agent", "SoulScout/1.1"}
                    },
                    cpr::Timeout{30000}
                );
            });
            
            if (response.error || response.status_code != 200) {
                return std::nullopt;
            }
            
            auto json_res = nlohmann::json::parse(response.text);
            
            // Check if we got a price for the token
            if (json_res.contains(token_mint) && json_res[token_mint].contains("usd")) {
                return json_res[token_mint]["usd"].get<double>();
            }
            
        } catch (const std::exception& e) {
            spdlog::error("Exception while getting price from CoinGecko: {}", e.what());
        }
        
        return std::nullopt;
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
    std::string usdc_mint_;
    std::string usdt_mint_;
};

// --- PIMPL forward declarations ---
JupiterClient::JupiterClient(const Config& config) : pImpl_(std::make_unique<Impl>(config)) {}
JupiterClient::~JupiterClient() = default;
std::optional<JupiterRoute> JupiterClient::get_quote(const std::string& input_mint, const std::string& output_mint, double amount_in) {
    return pImpl_->get_quote(input_mint, output_mint, amount_in);
}
bool JupiterClient::check_route_health(const std::string& input_mint, const std::string& output_mint) {
    return pImpl_->check_route_health(input_mint, output_mint);
}
std::optional<double> JupiterClient::get_usd_price(const std::string& token_mint) {
    return pImpl_->get_usd_price(token_mint);
}
