
#include "notifier_service.hpp"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

NotifierService::NotifierService(const Config& config) : config_(config) {
    try {
        redis_client_ = std::make_shared<sw::redis::Redis>(config_.redis_uri);
    } catch (const std::exception& e) {
        spdlog::critical("Failed to connect to Redis: {}", e.what());
        throw;
    }

    redis_bus_ = std::make_unique<RedisBus>(config_);
    audit_logger_ = std::make_unique<AuditLogger>(config_);
    throttler_ = std::make_unique<Throttler>(config_, redis_client_);
    deduplicator_ = std::make_unique<Deduplicator>(config_, redis_client_);
}

NotifierService::~NotifierService() {
    stop();
}

void NotifierService::run() {
    if (running_) return;
    running_ = true;

    // Start Redis consumers
    redis_bus_->start_consumers(
        [this](const InboundAlert& alert) {
            std::lock_guard<std::mutex> lock(alert_queue_mutex_);
            alert_queue_.push(alert);
            alert_queue_cv_.notify_one();
        },
        [this](const CommandRequest& req) {
            std::lock_guard<std::mutex> lock(command_queue_mutex_);
            command_queue_.push(req);
            command_queue_cv_.notify_one();
        }
    );

    // Start main processing thread
    service_thread_ = std::thread(&NotifierService::service_loop, this);
    spdlog::info("NotifierService started.");
}

void NotifierService::stop() {
    if (!running_) return;
    running_ = false;

    alert_queue_cv_.notify_all();
    command_queue_cv_.notify_all();

    redis_bus_->stop();
    if (service_thread_.joinable()) {
        service_thread_.join();
    }
    spdlog::info("NotifierService stopped.");
}

void NotifierService::service_loop() {
    while (running_) {
        // Process alerts
        {
            std::unique_lock<std::mutex> lock(alert_queue_mutex_);
            alert_queue_cv_.wait(lock, [this] { return !alert_queue_.empty() || !running_; });
            if (!running_ && alert_queue_.empty()) break;

            if (!alert_queue_.empty()) {
                InboundAlert alert = alert_queue_.front();
                alert_queue_.pop();
                lock.unlock(); // Unlock before processing
                handle_inbound_alert(alert);
            }
        }

        // Process commands
        {
            std::unique_lock<std::mutex> lock(command_queue_mutex_);
            if (!command_queue_.empty()) {
                CommandRequest req = command_queue_.front();
                command_queue_.pop();
                lock.unlock(); // Unlock before processing
                handle_command_request(req);
            }
        }
    }
}

void NotifierService::handle_inbound_alert(const InboundAlert& alert) {
    AuditEvent event;
    event.timestamp = std::chrono::system_clock::now();
    event.mint = alert.mint;
    event.symbol = alert.symbol;
    event.severity = alert.severity;
    event.confidence = alert.confidence;
    event.raw_alert = alert.to_json();

    // Policy checks
    if (throttler_->is_muted()) {
        event.outcome = "MUTED";
        event.details = "Global mute is active.";
    } else if (throttler_->is_globally_throttled(alert.severity)) {
        event.outcome = "THROTTLED";
        event.details = "Global throttle for 'actionable' alerts is active.";
    } else if (deduplicator_->is_duplicate(alert)) {
        event.outcome = "DUPLICATE";
        event.details = fmt::format("Duplicate alert within {}s.", config_.dedupe_period_sec);
    } else {
        // All checks passed, send the alert
        OutboundAlert outbound_alert;
        outbound_alert.chat_id = config_.telegram_chat_id;
        outbound_alert.message = Formatter::format_alert_message(alert);
        
        if (redis_bus_->publish_outbound_alert(outbound_alert)) {
            event.outcome = "SENT";
            event.details = "Alert sent to tg_gateway.";
            spdlog::info("Forwarded '{}' alert for {} to tg_gateway.", alert.severity, alert.symbol);

            // Record for global throttling if applicable
            if (alert.severity == "actionable") {
                throttler_->record_actionable_alert();
            }
        } else {
            event.outcome = "PUBLISH_FAILED";
            event.details = "Failed to publish outbound alert to Redis.";
            spdlog::error("Failed to publish outbound alert for {} to Redis.", alert.symbol);
        }
    }

    audit_logger_->log_event(event);
}

void NotifierService::handle_command_request(const CommandRequest& request) {
    CommandReply reply;
    reply.correlation_id = request.correlation_id;
    reply.chat_id = request.chat_id;
    reply.timestamp = std::chrono::system_clock::now();

    spdlog::info("Processing command '{}' from chat_id {}", request.command, request.chat_id);

    if (request.command == "/status") {
        reply.message = get_status_report();
    } else if (request.command == "/mute") {
        int minutes = 60; // Default mute time
        if (!request.args.empty()) {
            try {
                minutes = std::stoi(request.args[0]);
            } catch (const std::exception&) { /* Use default */ }
        }
        throttler_->set_mute(minutes);
        reply.message = fmt::format("🔇 Notifications muted for {} minutes.", minutes);
    } else if (request.command == "/unmute") {
        throttler_->clear_mute();
        reply.message = "🔊 Notifications have been unmuted.";
    } else {
        reply.message = fmt::format("Unknown command: {}", request.command);
    }

    if (!redis_bus_->publish_command_reply(reply)) {
        spdlog::error("Failed to publish command reply for correlation_id {}", reply.correlation_id);
    }
}

std::string NotifierService::get_status_report() {
    bool redis_ok = false;
    try {
        redis_client_->ping();
        redis_ok = true;
    } catch (const std::exception&) {}

    bool db_ok = audit_logger_->check_health();
    bool is_muted = throttler_->is_muted();

    return fmt::format(
        "**Notifier Service Status**\n\n"
        "**Mute Status**: {}\n"
        "**Redis Connection**: {}\n"
        "**Database Connection**: {}",
        is_muted ? "🔇 Muted" : "🔊 Active",
        redis_ok ? "✅ OK" : "❌ Error",
        db_ok ? "✅ OK" : "❌ Error"
    );
}
