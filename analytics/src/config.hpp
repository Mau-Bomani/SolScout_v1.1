
#pragma once

#include <string>
#include <chrono>

struct Config {
    // Redis configuration
    std::string redis_url = "redis://redis:6379";
    std::string stream_market = "soul.market.updates";
    std::string stream_alerts = "soul.alerts";
    std::string stream_req = "soul.cmd.requests";
    std::string stream_rep = "soul.cmd.replies";
    
    // PostgreSQL configuration
    std::string pg_dsn = "postgresql://user:pass@postgres:5432/soulsct";
    
    // Analytics thresholds
    int actionable_base_threshold = 70;
    int risk_on_adj = -10;
    int risk_off_adj = 10;
    int global_actionable_max_per_hour = 5;
    int cooldown_actionable_hours = 6;
    int cooldown_headsup_hours = 1;
    int watch_window_min = 120;
    int reentry_guard_hours = 12;
    
    // Service configuration
    std::string service_name = "analytics";
    std::string listen_addr = "0.0.0.0";
    int listen_port = 8083;
    std::string log_level = "info";
    
    // Hard gates
    double min_liquidity_actionable = 150000.0;
    double min_liquidity_headsup = 25000.0;
    double min_volume_actionable = 500000.0;
    double min_volume_headsup = 50000.0;
    double max_impact_pct = 1.5;
    double max_spread_pct = 2.5;
    int max_route_hops = 3;
    double max_route_deviation = 0.8;
    
    // Age and risk
    int min_age_hours = 24;
    int young_token_hours = 72;
    int min_c_young_risky = 80;
    
    // Momentum/entry
    double min_m1h_pct = 1.0;
    double max_m1h_pct = 12.0;
    double min_m24h_pct = 2.0;
    double max_m24h_pct = 60.0;
    
    // FDV/Liq
    double min_fdv_liq = 2.0;
    double max_fdv_liq = 150.0;
    double preferred_min_fdv_liq = 5.0;
    double preferred_max_fdv_liq = 50.0;
    
    // Rug heuristics
    double max_top_holder_pct = 25.0;
    int min_c_top_holder_override = 85;
    double min_s1s2_top_holder_override = 0.8;
    
    // Token list hygiene
    int hygiene_penalty = 10;
    
    // Data Quality
    double dq_start = 1.0;
    double dq_penalty_per_missing = 0.08;
    double min_dq_for_actionable = 0.7;
    
    // Confidence scoring
    int max_rug_cap = 55;
    
    // Net edge check
    double max_upside_cap = 15.0;
    double net_edge_k_factor = 2.0;
    double lag_penalty = 0.3;
    
    // Alert bands
    int headsup_min = 60;
    int headsup_max = 69;
    int high_conviction_min = 85;
    
    // Sizing
    double atr_risk_pct = 0.6;
    double liquidity_size_factor = 0.008;
    int max_positions = 3;
    double max_deployed_pct = 35.0;
    double default_deployed_pct = 30.0;
    double min_sol_free_pct = 5.0;
    double max_sol_free_pct = 10.0;
    
    // Thread pool
    int thread_pool_size = 4;
    
    // Load from environment variables
    void load_from_env();
};
