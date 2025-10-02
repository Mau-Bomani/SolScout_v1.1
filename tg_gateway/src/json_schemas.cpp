
#include "json_schemas.hpp"
#include "util.hpp"

nlohmann::json CommandRequest::to_json() const {
    return nlohmann::json{
        {"type", type},
        {"cmd", cmd},
        {"args", args},
        {"from", {
            {"tg_user_id", from.tg_user_id},
            {"role", from.role}
        }},
        {"corr_id", corr_id},
        {"ts", ts}
    };
}

CommandRequest CommandRequest::from_json(const nlohmann::json& j) {
    CommandRequest req;
    req.type = j.value("type", "command");
    req.cmd = j["cmd"];
    req.args = j.value("args", nlohmann::json::object());
    req.from.tg_user_id = j["from"]["tg_user_id"];
    req.from.role = j["from"]["role"];
    req.corr_id = j["corr_id"];
    req.ts = j["ts"];
    return req;
}

CommandReply CommandReply::from_json(const nlohmann::json& j) {
    CommandReply reply;
    reply.corr_id = j["corr_id"];
    reply.ok = j["ok"];
    reply.message = j["message"];
    reply.data = j.value("data", nlohmann::json::object());
    reply.ts = j["ts"];
    return reply;
}

Alert Alert::from_json(const nlohmann::json& j) {
    Alert alert;
    alert.severity = j["severity"];
    alert.symbol = j["symbol"];
    alert.price = j["price"];
    alert.confidence = j["confidence"];
    alert.lines = j["lines"].get<std::vector<std::string>>();
    alert.plan = j["plan"];
    alert.sol_path = j["sol_path"];
    alert.est_impact_pct = j["est_impact_pct"];
    alert.ts = j["ts"];
    return alert;
}

nlohmann::json AuditEvent::to_json() const {
    return nlohmann::json{
        {"event", event},
        {"actor", {
            {"tg_user_id", actor.tg_user_id},
            {"role", actor.role}
        }},
        {"detail", detail},
        {"ts", ts}
    };
}
