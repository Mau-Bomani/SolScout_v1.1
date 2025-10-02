
#include "analytics_service.hpp"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>

using json = nlohmann::json;

AnalyticsService::AnalyticsService(const Config& config)
    : config_(config) {
    
    // Initialize components
    redis_bus_ = std::make_unique<RedisBus>(config_);
    pg_store_ = std::make_unique<PostgresStore>(config_);
    signal_calculator_ = std::make_unique<SignalCalculator>(config_);
    confidence_scorer_ = std::make_unique<ConfidenceScorer>(config_);
    entry_checker_ = std::make_unique<EntryExitChecker>(config_);
    throttle_manager_ = std::make_unique<ThrottleManager>(config_);
    regime_detector_ = std::make_unique<RegimeDetector>(config_);
    
    // Initialize API signals handler
    api_signals_handler_ = std::make_unique<ApiSignalsHandler>(
        config_,
        *signal_calculator_,
        *confidence_scorer_,
        *entry_checker_,
        *throttle_manager_,
        *regime_detector_,
        *pg_store_
    );
}

AnalyticsService::~AnalyticsService() {
    stop();
}

void AnalyticsService::run() {
    if (running_) {
        spdlog::warn("Analytics service is already running");
        return;
    }
    
    running_ = true;
    
    // Subscribe to market updates
    redis_bus_->subscribe_market_updates([this](const MarketUpdate& update) {
        // Queue the update for processing
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            update_queue_.push(update);
        }
        queue_cv_.notify_one();
        
        // Track SOL price for regime detection
        if (update.mint_base == config_.sol_mint) {
            std::lock_guard<std::mutex> lock(sol_mutex_);
            sol_price_ = update.price_usd;
            
            // Calculate 24h change if we have historical data
            auto it = update.bars.find("15m");
            if (it != update.bars.end()) {
                const auto& bar_15m = it->second;
                sol_24h_change_pct_ = ((bar_15m.close / bar_15m.open) - 1.0) * 100.0;
            }
            
            // Update risk regime
            regime_detector_->update_regime(sol_price_, sol_24h_change_pct_);
        }
    });
    
    // Subscribe to command requests
    redis_bus_->subscribe_command_requests([this](const CommandRequest& request) {
        handle_command_request(request);
    });
    
    // Start service thread
    service_thread_ = std::thread(&AnalyticsService::service_thread_func, this);
    
    spdlog::info("Analytics service started");
}

void AnalyticsService::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    // Stop Redis subscriptions
    redis_bus_->stop_subscribers();
    
    // Notify queue processor to exit
    queue_cv_.notify_all();
    
    // Wait for service thread to finish
    if (service_thread_.joinable()) {
        service_thread_.join();
    }
    
    spdlog::info("Analytics service stopped");
}

void AnalyticsService::service_thread_func() {
    spdlog::info("Analytics service thread started");
    
    while (running_) {
        MarketUpdate update;
        bool has_update = false;
        
        // Get next update from queue
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (update_queue_.empty()) {
                // Wait for new updates or stop signal
                queue_cv_.wait_for(lock, std::chrono::seconds(1), [this] {
                    return !running_ || !update_queue_.empty();
                });
                
                if (!running_) {
                    break;
                }
                
                if (update_queue_.empty()) {
                    continue;
                }
            }
            
            update = update_queue_.front();
            update_queue_.pop();
            has_update = true;
        }
        
        if (has_update) {
            process_market_update(update);
        }
    }
    
    spdlog::info("Analytics service thread stopped");
}

void AnalyticsService::process_market_update(const MarketUpdate& update) {
    try {
        // Skip SOL updates for signal processing (we only use SOL for regime detection)
        if (update.mint_base == config_.sol_mint) {
            return;
        }
        
        // Cache the market update
        api_signals_handler_->cache_market_update(update);
        
        // Get token metadata
        auto metadata = pg_store_->get_token_metadata(update.mint_base);
        
        // Get token list mints
        auto token_list_mints = pg_store_->get_token_list_mints();
        
        // Calculate signals
        SignalResult signals = signal_calculator_->calculate_signals(update, metadata, token_list_mints);
        
        // Calculate confidence score
        signals.confidence_score = confidence_scorer_->calculate_confidence(signals);
        
        // Apply risk regime adjustment
        bool risk_on = regime_detector_->is_risk_on();
        signals.confidence_score = confidence_scorer_->apply_risk_adjustment(signals.confidence_score, risk_on);
        
        // Check entry conditions
        signals.entry_confirmed = entry_checker_->check_entry_conditions(update, signals);
        
        // Check net edge
        signals.net_edge_ok = entry_checker_->check_net_edge(update, signals);
        
        // Determine band
        signals.band = confidence_scorer_->determine_band(
            signals.confidence_score, signals.entry_confirmed, signals.net_edge_ok);
        
        // Generate alerts if needed
        generate_alerts(update, signals);
        
    } catch (const std::exception& e) {
        spdlog::error("Error processing market update: {}", e.what());
    }
}

void AnalyticsService::handle_command_request(const CommandRequest& request) {
    try {
        // Check if this is a signals request
        if (request.command == "get_signals") {
            // Handle signals request
            CommandReply reply = api_signals_handler_->handle_signals_request(request);
            
            // Publish reply
            redis_bus_->publish_command_reply(reply);
        }
    } catch (const std::exception& e) {
        spdlog::error("Error handling command request: {}", e.what());
        
        // Send error reply
        CommandReply reply;
        reply.corr_id = request.corr_id;
        reply.status = "error";
        reply.data = fmt::format(R"({{"error": "{}"}})", e.what());
        redis_bus_->publish_command_reply(reply);
    }
}

void AnalyticsService::generate_alerts(const MarketUpdate& update, const SignalResult& signals) {
    // Skip alerts for bands that don't meet criteria
    if (signals.band == "watch") {
        return;
    }
    
    // Check if we should throttle this alert
    if (throttle_manager_->should_throttle(update.mint_base, signals.band)) {
        return;
    }
    
    // Create alert
    Alert alert;
    alert.mint = update.mint_base;
    alert.symbol = update.symbol_base;
    alert.price_usd = update.price_usd;
    alert.liq_usd = update.liq_usd;
    alert.vol24h_usd = update.vol24h_usd;
    alert.confidence_score = signals.confidence_score;
    alert.band = signals.band;
    alert.reasons = signals.reasons;
    alert.timestamp = std::chrono::system_clock::now();
    
    // Publish alert
    if (redis_bus_->publish_alert(alert)) {
        // Record alert for throttling
        throttle_manager_->record_alert(update.mint_base, signals.band);
        
        spdlog::info("Published {} alert for {}: confidence {}, reasons: {}",
                    signals.band, update.symbol_base, signals.confidence_score,
                    fmt::join(signals.reasons, ", "));
    } else {
        spdlog::error("Failed to publish alert for {}", update.symbol_base);
    }
}
