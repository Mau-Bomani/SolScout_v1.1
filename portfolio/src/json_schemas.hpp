
#pragma once
#include <string>
#include <unordered_map>
#include <cstdint>
#include <nlohmann/json.hpp>

struct Actor {
    int64_t tg_user_id = 0;
    std::string username;
    std::string role;
};

struct CommandRequest {
    std::string cmd;
    Actor from;
    std::string corr_id;
    std::string ts;
    std::unordered_map<std::string, std::string> args;
    
    static CommandRequest from_json(const nlohmann::json& j);
};

struct CommandReply {
    std::string corr_id;
    std::string message;
    std::string timestamp;
    
    nlohmann::json to_json() const;
};

struct Audit {
    std::string event;
    std::string service;
    Actor actor;
    std::string detail;
    std::string timestamp;
    
    nlohmann::json to_json() const;
};

struct Alert {
    std::string title;
    std::string message;
    std::string severity;
    std::string timestamp;
    
    static Alert from_json(const nlohmann::json& j);
};
