
#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <nlohmann/json.hpp>

// Matches the structure from the analytics service
struct InboundAlert {
    std::string severity;
    std::string symbol;
    double price;
    int confidence;
    std::vector<std::string> lines;
    std::string plan;
    std::string sol_path;
    double est_impact_pct;
    std::chrono::system_clock::time_point timestamp;

    static InboundAlert from_json(const nlohmann::json& j);
};

struct OutboundAlert {
    std::string to; // "owner"
    std::string text;
    std::chrono::system_clock::time_point timestamp;
    nlohmann::json meta;

    nlohmann::json to_json() const;
};

struct CommandRequest {
    std::string type;
    std::string cmd;
    nlohmann::json args;
    nlohmann::json from;
    std::string corr_id;
    std::chrono::system_clock::time_point timestamp;

    static CommandRequest from_json(const nlohmann::json& j);
};

struct CommandReply {
    std::string corr_id;
    bool ok;
    std::string message;
    std::chrono::system_clock::time_point timestamp;

    nlohmann::json to_json() const;
};
