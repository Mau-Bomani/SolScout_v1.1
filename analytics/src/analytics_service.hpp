
#pragma once

#include "config.hpp"
#include "redis_bus.hpp"
#include "pg_store.hpp"
#include "signals.hpp"
#include "scoring.hpp"
#include "entry_exit.hpp"
#include "throttles.hpp"
#include "regime.hpp"
#include "api_signals.hpp"
#include <atomic>
#include <thread>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <queue>

class AnalyticsService {
public:
    explicit AnalyticsService(const Config& config);
    ~AnalyticsService();
    
    // Start the service
    void run();
    
    // Stop the service
    void stop();

private:
    // Service thread function
    void service_thread_func();
    
    // Process market updates
    void process_market_update(const MarketUpdate& update);
    
    // Handle command requests
    void handle_command_request(const CommandRequest& request);
    
    // Generate and publish alerts
    void generate_alerts(const MarketUpdate& update, const SignalResult& signals);
    
    // Configuration
    Config config_;
    
    // Service components
    std::unique_ptr<RedisBus> redis_bus_;
    std::unique_ptr<PostgresStore> pg_store_;
    std::unique_ptr<SignalCalculator> signal_calculator_;
    std::unique_ptr<ConfidenceScorer> confidence_scorer_;
    std::unique_ptr<EntryExitChecker> entry_checker_;
    std::unique_ptr<ThrottleManager> throttle_manager_;
    std::unique_ptr<RegimeDetector> regime_detector_;
    std::unique_ptr<ApiSignalsHandler> api_signals_handler_;
    
    // Thread management
    std::atomic<bool> running_{false};
    std::thread service_thread_;
    
    // Market update queue
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::queue<MarketUpdate> update_queue_;
    
    // SOL price tracking
    double sol_price_{0.0};
    double sol_24h_change_pct_{0.0};
    std::mutex sol_mutex_;
};
