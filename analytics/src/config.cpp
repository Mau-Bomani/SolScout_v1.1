
#include "config.hpp"
#include <cstdlib>
#include <spdlog/spdlog.h>

namespace {
    std::string get_env(const char* name, const std::string& default_value) {
        const char* value = std::getenv(name);
        return value ? value : default_value;
    }
    
    int get_env_int(const char* name, int default_value) {
        const char* value = std::getenv(name);
        if (value) {
            try {
                return std::stoi(value);
            } catch (...) {
                spdlog::warn("Invalid integer value for {}: {}", name, value);
            }
        }
        return default_value;
    }
    
    double get_env_double(const char* name, double default_value) {
        const char* value = std::getenv(name);
        if (value) {
            try {
                return std::stod(value);
            } catch (...) {
                spdlog::warn("Invalid double value for {}: {}", name, value);
            }
        }
        return default_value;
    }
}

void Config::load_from_env() {
    // Redis configuration
    redis_url = get_env("REDIS_URL", redis_url);
    stream_market = get_env("STREAM_MARKET", stream_market);
    stream_alerts = get_env("STREAM_ALERTS", stream_alerts);
    stream_req = get_env("STREAM_REQ", stream_req);
    stream_rep = get_env("STREAM_REP", stream_rep);
    
    // PostgreSQL configuration
    pg_dsn = get_env("PG_DSN", pg_dsn);
    
    // Analytics thresholds
    actionable_base_threshold = get_env_int("ACTIONABLE_BASE_THRESHOLD", actionable_base_threshold);
    risk_on_adj = get_env_int("RISK_ON_ADJ", risk_on_adj);
    risk_off_adj = get_env_int("RISK_OFF_ADJ", risk_off_adj);
    global_actionable_max_per_hour = get_env_int("GLOBAL_ACTIONABLE_MAX_PER_HOUR", global_actionable_max_per_hour);
    cooldown_actionable_hours = get_env_int("COOLDOWN_ACTIONABLE_HOURS", cooldown_actionable_hours);
    cooldown_headsup_hours = get_env_int("COOLDOWN_HEADSUP_HOURS", cooldown_headsup_hours);
    watch_window_min = get_env_int("WATCH_WINDOW_MIN", watch_window_min);
    reentry_guard_hours = get_env_int("REENTRY_GUARD_HOURS", reentry_guard_hours);
    
    // Service configuration
    service_name = get_env("SERVICE_NAME", service_name);
    listen_addr = get_env("LISTEN_ADDR", listen_addr);
    listen_port = get_env_int("LISTEN_PORT", listen_port);
    log_level = get_env("LOG_LEVEL", log_level);
}
