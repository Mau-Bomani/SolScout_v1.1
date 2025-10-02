
#include "config.hpp"
#include <cstdlib>
#include <charconv>

// Helper to get environment variables
std::string get_env(const char* name, const std::string& default_val) {
    const char* value = std::getenv(name);
    return value ? value : default_val;
}

int get_env_int(const char* name, int default_val) {
    const char* value = std::getenv(name);
    if (value) {
        int int_val;
        auto result = std::from_chars(value, value + std::strlen(value), int_val);
        if (result.ec == std::errc()) {
            return int_val;
        }
    }
    return default_val;
}

void Config::load_from_env() {
    redis_url = get_env("REDIS_URL", redis_url);
    stream_alerts_in = get_env("STREAM_ALERTS_IN", stream_alerts_in);
    stream_alerts_out = get_env("STREAM_ALERTS_OUT", stream_alerts_out);
    stream_req = get_env("STREAM_REQ", stream_req);
    stream_rep = get_env("STREAM_REP", stream_rep);

    pg_dsn = get_env("PG_DSN", pg_dsn);

    global_actionable_max_per_hour = get_env_int("GLOBAL_ACTIONABLE_MAX_PER_HOUR", global_actionable_max_per_hour);
    dedup_ttl_seconds = get_env_int("DEDUP_TTL_SECONDS", dedup_ttl_seconds);

    mute_default_minutes = get_env_int("MUTE_DEFAULT_MINUTES", mute_default_minutes);
    owner_telegram_id = get_env("OWNER_TELEGRAM_ID", owner_telegram_id);

    user_tz = get_env("USER_TZ", user_tz);
    service_name = get_env("SERVICE_NAME", service_name);
    listen_addr = get_env("LISTEN_ADDR", listen_addr);
    listen_port = get_env_int("LISTEN_PORT", listen_port);
    log_level = get_env("LOG_LEVEL", log_level);
    thread_pool_size = get_env_int("THREAD_POOL_SIZE", thread_pool_size);
}
