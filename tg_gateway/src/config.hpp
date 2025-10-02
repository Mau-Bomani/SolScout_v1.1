
#pragma once
#include <string>
#include <cstdint>

struct Config {
    std::string tg_bot_token;
    int64_t owner_telegram_id;
    std::string redis_url;
    std::string gateway_mode;
    std::string webhook_public_url;
    std::string listen_addr;
    int listen_port;
    int rate_limit_msgs_per_min;
    int global_actionable_max_per_hour;
    int guest_default_minutes;
    std::string stream_req;
    std::string stream_rep;
    std::string stream_alerts;
    std::string stream_audit;
    std::string service_name;
    std::string log_level;

    static Config from_env();
    void validate() const;
};
