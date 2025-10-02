
#include "types.hpp"
#include "util.hpp"

InboundAlert InboundAlert::from_json(const nlohmann::json& j) {
    InboundAlert alert;
    alert.severity = j.at("severity").get<std::string>();
    alert.symbol = j.at("symbol").get<std::string>();
    alert.price = j.at("price").get<double>();
    alert.confidence = j.at("confidence").get<int>();
    alert.lines = j.at("lines").get<std::vector<std::string>>();
    alert.plan = j.at("plan").get<std::string>();
    alert.sol_path = j.at("sol_path").get<std::string>();
    alert.est_impact_pct = j.at("est_impact_pct").get<double>();
    alert.timestamp = parse_iso8601(j.at("ts").get<std::string>());
    return alert;
}

nlohmann::json OutboundAlert::to_json() const {
    return {
        {"to", to},
        {"text", text},
        {"ts", format_iso8601(timestamp)},
        {"meta", meta}
    };
}

CommandRequest CommandRequest::from_json(const nlohmann::json& j) {
    CommandRequest req;
    req.type = j.at("type").get<std::string>();
    req.cmd = j.at("cmd").get<std::string>();
    req.args = j.at("args");
    req.from = j.at("from");
    req.corr_id = j.at("corr_id").get<std::string>();
    req.timestamp = parse_iso8601(j.at("ts").get<std::string>());
    return req;
}

nlohmann::json CommandReply::to_json() const {
    return {
        {"corr_id", corr_id},
        {"ok", ok},
        {"message", message},
        {"ts", format_iso8601(timestamp)}
    };
}
