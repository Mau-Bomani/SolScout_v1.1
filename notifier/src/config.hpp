
#pragma once

#include <string>
#include <cstdint>

struct Config {
    // Redis configuration
    std::string redis_url = "redis://redis:6379";
    std::string stream_alerts_in = "soul.alerts";
    std::string stream_alerts_out = "soul.outbound.alerts";
    std::string stream_req = "soul.cmd.requests";
    std::string stream_rep = "soul.cmd.replies";

    // PostgreSQL configuration
    std::string pg_dsn; // Optional, e.g., "postgresql://user:pass@postgres:5432/soulsct"

    // Throttling and Deduplication
    int global_actionable_max_per_hour = 5;
    int dedup_ttl_seconds = 21600; // 6 hours

    // Mute configuration
    int mute_default_minutes = 30;
    std::string owner_telegram_id;

    // General
    std::string user_tz = "America/Denver";
    std::string service_name = "notifier";
    std::string listen_addr = "0.0.0.0";
    int listen_port = 8084;
    std::string log_level = "info";
    int thread_pool_size = 4;

    // Load from environment variables
    void load_from_env();
};
