
#include "api_signals.hpp"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <chrono>

using json = nlohmann::json;

ApiSignalsHandler::ApiSignalsHandler(
    const Config& config,
    SignalCalculator& signal_calculator,
    ConfidenceScorer& confidence_scorer,
    EntryExitChecker& entry_checker,
    ThrottleManager& throttle_manager,
    RegimeDetector& regime_detector,
    PostgresStore& pg_store
) : config_(config),
    signal_calculator_(signal_calculator),
    confidence_scorer_(confidence_scorer),
    entry_checker_(entry_checker),
    throttle_manager_(throttle_manager),
    regime_detector_(regime_detector),
    pg_store_(pg_store) {}

CommandReply ApiSignalsHandler::handle_signals_request(const CommandRequest& request) {
    CommandReply reply;
    reply.corr_id = request.corr_id;
    reply.status = "success";
    
    try {
        json params = json::parse(request.params);
        
        if (params.contains("mint")) {
            // Single token signals request
            std::string mint = params["mint"].get<std::string>();
            auto signals_opt = get_token_signals(mint);
            
            if (signals_opt) {
                json result = {
                    {"mint", mint},
                    {"confidence", signals_opt->confidence_score},
                    {"band", signals_opt->band},
                    {"signals", {
                        {"s1_liquidity", signals_opt->s1_liquidity},
                        {"s2_volume", signals_opt->s2_volume},
                        {"s3_momentum_1h", signals_opt->s3_momentum_1h},
                        {"s4_momentum_24h", signals_opt->s4_momentum_24h},
                        {"s5_volatility", signals_opt->s5_volatility},
                        {"s6_price_discovery", signals_opt->s6_price_discovery},
                        {"s7_rug_risk", signals_opt->s7_rug_risk},
                        {"s8_tradability", signals_opt->s8_tradability},
                        {"s9_relative_strength", signals_opt->s9_relative_strength},
                        {"s10_route_quality", signals_opt->s10_route_quality},
                        {"n1_hygiene", signals_opt->n1_hygiene}
                    }},
                    {"data_quality", signals_opt->data_quality},
                    {"entry_confirmed", signals_opt->entry_confirmed},
                    {"net_edge_ok", signals_opt->net_edge_ok},
                    {"reasons", signals_opt->reasons},
                    {"risk_regime", regime_detector_.get_regime_string()}
                };
                
                reply.data = result.dump();
            } else {
                reply.status = "error";
                reply.data = R"({"error": "Token not found or no signals available"})";
            }
        } else if (params.contains("wallet")) {
            // Portfolio signals request
            std::string wallet = params["wallet"].get<std::string>();
            auto portfolio_signals = get_portfolio_signals(wallet);
            
            json result = json::array();
            for (const auto& ps : portfolio_signals) {
                result.push_back({
                    {"mint", ps.mint},
                    {"symbol", ps.symbol},
                    {"amount", ps.amount},
                    {"value_usd", ps.value_usd},
                    {"confidence", ps.confidence_score},
                    {"band", ps.band},
                    {"entry_price", ps.entry_price},
                    {"current_price", ps.current_price},
                    {"pnl_pct", ps.pnl_pct},
                    {"hold_time_hours", ps.hold_time_hours},
                    {"risk_regime", regime_detector_.get_regime_string()}
                });
            }
            
            reply.data = result.dump();
        } else {
            reply.status = "error";
            reply.data = R"({"error": "Missing required parameter: mint or wallet"})";
        }
    } catch (const std::exception& e) {
        spdlog::error("Error handling signals request: {}", e.what());
        reply.status = "error";
        reply.data = fmt::format(R"({{"error": "{}"}})", e.what());
    }
    
    return reply;
}

std::optional<SignalResult> ApiSignalsHandler::get_token_signals(const std::string& mint) {
    // Check cache first
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = signals_cache_.find(mint);
        if (it != signals_cache_.end()) {
            return it->second;
        }
    }
    
    // Get market update from cache
    auto update_opt = get_cached_update(mint);
    if (!update_opt) {
        spdlog::warn("No market update found for mint: {}", mint);
        return std::nullopt;
    }
    
    // Get token metadata
    auto metadata = pg_store_.get_token_metadata(mint);
    
    // Get token list mints
    auto token_list_mints = pg_store_.get_token_list_mints();
    
    // Calculate signals
    SignalResult signals = signal_calculator_.calculate_signals(*update_opt, metadata, token_list_mints);
    
    // Calculate confidence score
    signals.confidence_score = confidence_scorer_.calculate_confidence(signals);
    
    // Apply risk regime adjustment
    bool risk_on = regime_detector_.is_risk_on();
    signals.confidence_score = confidence_scorer_.apply_risk_adjustment(signals.confidence_score, risk_on);
    
    // Check entry conditions
    signals.entry_confirmed = entry_checker_.check_entry_conditions(*update_opt, signals);
    
    // Check net edge
    signals.net_edge_ok = entry_checker_.check_net_edge(*update_opt, signals);
    
    // Determine band
    signals.band = confidence_scorer_.determine_band(
        signals.confidence_score, signals.entry_confirmed, signals.net_edge_ok);
    
    // Cache the signals
    cache_signals(mint, signals);
    
    return signals;
}

std::vector<PortfolioSignalResult> ApiSignalsHandler::get_portfolio_signals(const std::string& wallet_address) {
    std::vector<PortfolioSignalResult> results;
    
    // Get portfolio from database
    auto portfolio_opt = pg_store_.get_portfolio(wallet_address);
    if (!portfolio_opt) {
        spdlog::warn("No portfolio found for wallet: {}", wallet_address);
        return results;
    }
    
    // Process each holding
    for (const auto& holding : portfolio_opt->holdings) {
        PortfolioSignalResult result;
        result.mint = holding.mint;
        result.symbol = holding.symbol;
        result.amount = holding.amount;
        result.value_usd = holding.value_usd;
        result.entry_price = holding.entry_price;
        
        // Get current price and calculate PnL
        auto update_opt = get_cached_update(holding.mint);
        if (update_opt) {
            result.current_price = update_opt->price_usd;
            result.pnl_pct = ((result.current_price / result.entry_price) - 1.0) * 100.0;
            
            // Calculate hold time
            auto now = std::chrono::system_clock::now();
            result.hold_time_hours = std::chrono::duration_cast<std::chrono::hours>(
                now - holding.first_acquired).count();
            
            // Get signals
            auto signals_opt = get_token_signals(holding.mint);
            if (signals_opt) {
                result.confidence_score = signals_opt->confidence_score;
                result.band = signals_opt->band;
            } else {
                result.confidence_score = 0;
                result.band = "unknown";
            }
        } else {
            result.current_price = result.entry_price;
            result.pnl_pct = 0.0;
            result.hold_time_hours = 0;
            result.confidence_score = 0;
            result.band = "unknown";
        }
        
        results.push_back(result);
    }
    
    return results;
}

void ApiSignalsHandler::cache_market_update(const MarketUpdate& update) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    // Store in cache
    market_updates_cache_[update.mint_base] = update;
    
    // Clean up old entries periodically
    static int counter = 0;
    if (++counter % 100 == 0) {
        cleanup_cache();
    }
}

void ApiSignalsHandler::cleanup_cache() {
    auto now = std::chrono::system_clock::now();
    
    // Remove market updates older than the cache TTL
    for (auto it = market_updates_cache_.begin(); it != market_updates_cache_.end();) {
        auto age = std::chrono::duration_cast<std::chrono::minutes>(now - it->second.timestamp).count();
        if (age > config_.cache_ttl_minutes) {
            it = market_updates_cache_.erase(it);
        } else {
            ++it;
        }
    }
    
    // Remove signal results older than the cache TTL
    for (auto it = signals_cache_.begin(); it != signals_cache_.end();) {
        auto age = std::chrono::duration_cast<std::chrono::minutes>(now - it->second.timestamp).count();
        if (age > config_.cache_ttl_minutes) {
            it = signals_cache_.erase(it);
        } else {
            ++it;
        }
    }
}

std::optional<MarketUpdate> ApiSignalsHandler::get_cached_update(const std::string& mint) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    auto it = market_updates_cache_.find(mint);
    if (it != market_updates_cache_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

void ApiSignalsHandler::cache_signals(const std::string& mint, const SignalResult& signals) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    signals_cache_[mint] = signals;
    signals_cache_[mint].timestamp = std::chrono::system_clock::now();
}
