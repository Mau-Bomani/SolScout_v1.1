#pragma once

#include "config.hpp"
#include "types.hpp"
#include <pqxx/pqxx>
#include <memory>
#include <mutex>

class AuditLogger {
public:
    explicit AuditLogger(const Config& config);
    ~AuditLogger();

    void log_event(const AuditEvent& event);
    bool check_health();

private:
    bool connect();

    const Config& config_;
    std::unique_ptr<pqxx::connection> conn_;
    std::mutex conn_mutex_;
};
