
#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

struct CommandRequest {
    std::string type = "command";
    std::string cmd;
    nlohmann::json args;
    struct {
        int64_t tg_user_id;
        std::string role;
    } from;
    std::string corr_id;
    std::string ts;
    
    nlohmann::json to_json() const;
    static CommandRequest from_json(const nlohmann::json& j);
};

struct CommandReply {
    std::string corr_id;
    bool ok;
    std::string message;
    nlohmann::json data;
    std::string ts;
    
    static CommandReply from_json(const nlohmann::json& j);
};

struct Alert {
    std::string severity;
    std::string symbol;
    double price;
    double confidence;
    std::vector<std::string> lines;
    std::string plan;
    std::string sol_path;
    double est_impact_pct;
    std::string ts;
    
    static Alert from_json(const nlohmann::json& j);
};

struct AuditEvent {
    std::string event;
    struct {
        int64_t tg_user_id;
        std::string role;
    } actor;
    nlohmann::json detail;
    std::string ts;
    
    nlohmann::json to_json() const;
};
