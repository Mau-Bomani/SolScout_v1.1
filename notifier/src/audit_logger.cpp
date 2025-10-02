
#include "audit_logger.hpp"
#include "util.hpp"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

AuditLogger::AuditLogger(const Config& config) : config_(config) {
    connect();
}

AuditLogger::~AuditLogger() {
    if (conn_ && conn_->is_open()) {
        conn_->disconnect();
    }
}

bool AuditLogger::connect() {
    try {
        conn_ = std::make_unique<pqxx::connection>(config_.pg_conn_str);
        spdlog::info("Successfully connected to audit database.");
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to connect to audit database: {}", e.what());
        conn_.reset();
        return false;
    }
}

bool AuditLogger::check_health() {
    std::lock_guard<std::mutex> lock(conn_mutex_);
    if (!conn_ || !conn_->is_open()) {
        if (!connect()) {
            return false;
        }
    }
    try {
        pqxx::nontransaction n(*conn_);
        n.exec("SELECT 1");
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Audit database health check failed: {}", e.what());
        return false;
    }
}

void AuditLogger::log_event(const AuditEvent& event) {
    std::lock_guard<std::mutex> lock(conn_mutex_);
    if (!check_health()) {
        spdlog::error("Cannot log audit event, database connection is down.");
        return;
    }

    try {
        pqxx::work w(*conn_);
        w.exec_params(
            "INSERT INTO notifier_audit_log (timestamp, mint, symbol, severity, confidence, outcome, details, raw_alert) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8)",
            format_iso8601(event.timestamp),
            event.mint,
            event.symbol,
            event.severity,
            event.confidence,
            event.outcome,
            event.details,
            event.raw_alert.dump()
        );
        w.commit();
    } catch (const std::exception& e) {
        spdlog::error("Failed to write audit log to database: {}", e.what());
    }
}
