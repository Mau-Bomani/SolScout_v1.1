#pragma once

#include "types.hpp"
#include "config.hpp"
#include "signals.hpp"
#include "scoring.hpp"
#include "entry_exit.hpp"
#include "throttles.hpp"
#include "regime.hpp"
#include "pg_store.hpp"
#include <string>
#include <optional>
#include <mutex>
#include <unordered_map>

class ApiSignalsHandler {
public:
    ApiSignalsHandler(
        const Config& config,
        SignalCalculator& signal_calculator,
        ConfidenceScorer& confidence_scorer,
        EntryExitChecker& entry_checker,
        ThrottleManager& throttle_manager,
        RegimeDetector& regime_detector,
        PostgresStore& pg_store
    );
    
    // Handle a signals request
    CommandReply handle_signals_request(const CommandRequest& request);
    
    // Get signals for a specific token
    std::optional<SignalResult> get_token_signals(const std::string& mint);
    
    // Get signals for a portfolio
    std::vector<PortfolioSignalResult> get_portfolio_signals(const std::string& wallet_address);
    
    // Cache a market update
    void cache_market_update(const MarketUpdate& update);
    
    // Clean up old cache entries
    void cleanup_cache();

private:
    const Config& config_;
    SignalCalculator& signal_calculator_;
    ConfidenceScorer& confidence_scorer_;
    EntryExitChecker& entry_checker_;
    ThrottleManager& throttle_manager_;
    RegimeDetector& regime_detector_;
    PostgresStore& pg_store_;
    
    std::mutex cache_mutex_;
    std::unordered_map<std::string, MarketUpdate> market_updates_cache_;
    std::unordered_map<std::string, SignalResult> signals_cache_;
    
    // Helper methods
    std::optional<MarketUpdate> get_cached_update(const std::string& mint);
    void cache_signals(const std::string& mint, const SignalResult& signals);
};
