
#include "json_schemas.hpp"
#include "util.hpp"

CommandRequest CommandRequest::from_json(const nlohmann::json& j) {
    CommandRequest req;
    req.cmd = j.at("cmd").get<std::string>();
    req.corr_id = j.at("corr_id").get<std::string>();
    req.ts = j.at("ts").get<std::string>();
    
    if (j.contains("from")) {
        const auto& from = j.at("from");
        req.from.tg_user_id = from.at("tg_user_id").get<int64_t>();
        if (from.contains("username")) {
            req.from.username = from.at("username").get<std::string>();
        }
        if (from.contains("role")) {
            req.from.role = from.at("role").get<std::string>();
        }
    }
    
    if (j.contains("args")) {
        for (const auto& [key, value] : j.at("args").items()) {
            req.args[key] = value.get<std::string>();
        }
    }
    
    return req;
}

nlohmann::json CommandReply::to_json() const {
    nlohmann::json j;
    j["corr_id"] = corr_id;
    j["message"] = message;
    j["timestamp"] = timestamp;
    return j;
}

nlohmann::json Audit::to_json() const {
    nlohmann::json j;
    j["event"] = event;
    j["service"] = service;
    j["actor"]["tg_user_id"] = actor.tg_user_id;
    j["actor"]["username"] = actor.username;
    j["actor"]["role"] = actor.role;
    j["detail"] = detail;
    j["timestamp"] = timestamp;
    return j;
}

Alert Alert::from_json(const nlohmann::json& j) {
    Alert alert;
    alert.title = j.at("title").get<std::string>();
    alert.message = j.at("message").get<std::string>();
    alert.severity = j.at("severity").get<std::string>();
    alert.timestamp = j.at("timestamp").get<std::string>();
    return alert;
}
