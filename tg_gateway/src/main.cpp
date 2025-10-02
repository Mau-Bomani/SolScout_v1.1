
#include "config.hpp"
#include "telegram_client.hpp"
#include "webhook_server.hpp"
#include "poller.hpp"
#include "redis_bus.hpp"
#include "parser.hpp"
#include "auth.hpp"
#include "rate_limiter.hpp"
#include "health.hpp"
#include "json_schemas.hpp"
#include "util.hpp"
#include <spdlog/spdlog.h>
#include <signal.h>
#include <unordered_map>
#include <chrono>
#include <thread>

struct PendingCommandInfo {
    int64_t chat_id;
    std::chrono::system_clock::time_point timestamp;
};

class TelegramGateway {
public:
    TelegramGateway(const Config& config)
        : config_(config)
        , telegram_client_(config)
        , redis_bus_(config)
        , auth_manager_(config)
        , rate_limiter_(config.rate_limit_msgs_per_min, config.global_actionable_max_per_hour)
        , webhook_server_(config)
        , poller_(config, telegram_client_)
        , health_checker_(redis_bus_)
        , running_(false) {
        cleanup_thread_ = std::thread(&TelegramGateway::cleanup_loop, this);
    }
    
    ~TelegramGateway() {
        if (cleanup_thread_.joinable()) {
            cleanup_thread_.join();
        }
    }
    
    bool initialize() {
        util::setup_logging(config_.log_level);
        
        if (!redis_bus_.connect()) {
            spdlog::error("Failed to connect to Redis");
            return false;
        }
        
        redis_bus_.start_reply_consumer([this](const CommandReply& reply) {
            handle_command_reply(reply);
        });
        
        redis_bus_.start_alert_consumer([this](const Alert& alert) {
            handle_alert(alert);
        });
        
        if (config_.gateway_mode == "webhook") {
            if (!setup_webhook()) {
                return false;
            }
        }
        
        return true;
    }
    
    void run() {
        running_ = true;

        // Start periodic cleanup
        cleanup_thread_ = std::thread(&TelegramGateway::cleanup_loop, this);

        if (config_.gateway_mode == "poll") {
            spdlog::info("Starting in polling mode");
            poller_.start([this](const TelegramUpdate& update) {
                handle_telegram_update_struct(update);
            });
            // Block until poller stops
            while (poller_.is_running()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        } else { // webhook mode
            spdlog::info("Starting in webhook mode on {}:{}", config_.listen_addr, config_.listen_port);
            webhook_server_.run([this](const nlohmann::json& update) {
                handle_telegram_update(update);
            });
            // Block until server stops
            while (webhook_server_.is_running()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }
    
    void shutdown() {
        running_ = false;
        webhook_server_.stop();
        poller_.stop();
        redis_bus_.stop_consumers();
        redis_bus_.disconnect();
        if (cleanup_thread_.joinable()) {
            cleanup_thread_.join();
        }
    }
    
private:
    Config config_;
    TelegramClient telegram_client_;
    RedisBus redis_bus_;
    AuthManager auth_manager_;
    RateLimiter rate_limiter_;
    WebhookServer webhook_server_;
    TelegramPoller poller_;
    HealthChecker health_checker_;
    std::atomic<bool> running_;
    std::unordered_map<std::string, PendingCommandInfo> pending_commands_;
    std::mutex pending_commands_mutex_;
    std::thread cleanup_thread_;
    
    bool setup_webhook() {
        std::string webhook_url = config_.webhook_public_url + "/telegram/webhook";
        if (!telegram_client_.set_webhook(webhook_url)) {
            spdlog::error("Failed to set webhook");
            return false;
        }
        return true;
    }
    
    void handle_telegram_update(const nlohmann::json& update_json) {
        try {
            auto update = TelegramUpdate::from_json(update_json);
            handle_telegram_update_struct(update);
        } catch (const std::exception& e) {
            spdlog::error("Failed to parse Telegram update: {}", e.what());
        }
    }
    
    void handle_telegram_update_struct(const TelegramUpdate& update) {
        if (update.message.text.empty()) {
            return;
        }
        
        int64_t user_id = update.message.from.id;
        int64_t chat_id = update.message.chat_id;
        
        // Rate limiting
        if (!rate_limiter_.check_user_rate_limit(user_id)) {
            telegram_client_.send_message(chat_id, "Rate limit exceeded. Please slow down.");
            return;
        }
        
        // Parse command
        auto parsed = CommandParser::parse(update.message.text);
        if (!parsed) {
            telegram_client_.send_message(chat_id, "Invalid command format. Use /help for available commands.");
            return;
        }
        
        handle_command(*parsed, user_id, chat_id);
    }
    
    void handle_command(const ParsedCommand& cmd, int64_t user_id, int64_t chat_id) {
        // Handle /start with PIN for guest access
        if (cmd.command == "start" && cmd.args.size() == 1) {
            handle_guest_login(cmd.args[0], user_id, chat_id);
            return;
        }
        
        // Check authentication
        Role user_role = auth_manager_.get_user_role(user_id);
        if (user_role == Role::UNKNOWN) {
            telegram_client_.send_message(chat_id, "Access denied. Contact the owner for access.");
            audit_auth_denied(user_id);
            return;
        }
        
        // Check command permissions
        if (!auth_manager_.is_command_allowed(cmd.command, user_role)) {
            telegram_client_.send_message(chat_id, "You don't have permission to use this command.");
            audit_auth_denied(user_id);
            return;
        }
        
        // Handle special commands locally
        if (handle_local_command(cmd, user_id, chat_id, user_role)) {
            return;
        }
        
        // Forward to other services
        forward_command(cmd, user_id, chat_id, user_role);
        audit_command_used(cmd.command, user_id, user_role);
    }
    
    bool handle_local_command(const ParsedCommand& cmd, int64_t user_id, int64_t chat_id, Role role) {
        if (cmd.command == "start") {
            telegram_client_.send_message(chat_id, "Welcome to SoulScout! Use /help for available commands.");
            return true;
        }
        
        if (cmd.command == "help") {
            std::string help_text = "Available commands:\n"
                "/balance - Show wallet balances\n"
                "/holdings - Show current positions\n"
                "/signals [window] - Show recent signals\n"
                "/health - System health check\n";
            
            if (role == Role::OWNER) {
                help_text += "/silence [minutes] - Silence alerts\n"
                    "/resume - Resume alerts\n"
                    "/add_wallet <address> - Add wallet to monitor\n"
                    "/remove_wallet <address> - Remove wallet\n"
                    "/guest [minutes] - Generate guest PIN\n";
            }
            
            telegram_client_.send_message(chat_id, help_text);
            return true;
        }
        
        if (cmd.command == "guest" && role == Role::OWNER) {
            int minutes = cmd.get_int_arg(0).value_or(config_.guest_default_minutes);
            std::string pin = util::generate_pin();
            
            if (redis_bus_.store_guest_pin(pin, user_id, minutes * 60)) {
                std::string message = fmt::format("Guest PIN: <code>{}</code>\nValid for {} minutes", pin, minutes);
                telegram_client_.send_message(chat_id, message);
            } else {
                telegram_client_.send_message(chat_id, "Failed to generate guest PIN");
            }
            return true;
        }
        
        return false;
    }
    
    void handle_guest_login(const std::string& pin, int64_t user_id, int64_t chat_id) {
        auto owner_id = redis_bus_.get_guest_pin_user(pin);
        if (!owner_id) {
            telegram_client_.send_message(chat_id, "Invalid or expired PIN");
            return;
        }
        
        // Set guest session (PIN TTL determines duration)
        auth_manager_.set_guest_session(user_id, config_.guest_default_minutes);
        redis_bus_.delete_guest_pin(pin);
        
        telegram_client_.send_message(chat_id, "Guest access granted! Use /help for available commands.");
        audit_guest_login(user_id);
    }
    
    void forward_command(const ParsedCommand& cmd, int64_t user_id, int64_t chat_id, Role role) {
        CommandRequest request;
        request.cmd = cmd.command;
        request.from.tg_user_id = user_id;
        request.from.role = (role == Role::OWNER) ? "owner" : "guest";
        request.corr_id = util::generate_uuid();
        request.ts = util::current_iso8601();
        
        // Build args
        if (cmd.command == "signals" && !cmd.args.empty()) {
            request.args["window"] = cmd.args[0];
        } else if ((cmd.command == "add_wallet" || cmd.command == "remove_wallet") && !cmd.args.empty()) {
            if (!util::is_valid_address(cmd.args[0])) {
                return; // Invalid address format
            }
            request.args["address"] = cmd.args[0];
        } else if (cmd.command == "silence" && !cmd.args.empty()) {
            auto minutes = cmd.get_int_arg(0);
            if (minutes) {
                request.args["minutes"] = *minutes;
            }
        }
        
        // Store pending command for reply correlation
        {
            std::lock_guard<std::mutex> lock(pending_commands_mutex_);
            pending_commands_[request.corr_id] = {chat_id, std::chrono::system_clock::now()};
        }
        
        redis_bus_.publish_command_request(request);
    }
    
    void handle_command_reply(const CommandReply& reply) {
        int64_t chat_id_to_reply = 0;
        // Check if we have this correlation ID
        {
            std::lock_guard<std::mutex> lock(pending_commands_mutex_);
            auto it = pending_commands_.find(reply.corr_id);
            if (it == pending_commands_.end()) {
                spdlog::warn("Received reply for unknown or expired correlation ID: {}", reply.corr_id);
                return; // Not our command or already timed out
            }
            chat_id_to_reply = it->second.chat_id;
            pending_commands_.erase(it);
        }
        
        // Send reply to the original user
        if (chat_id_to_reply != 0) {
            telegram_client_.send_message(chat_id_to_reply, reply.message);
        } else {
            spdlog::error("Could not find chat_id for correlation ID: {}", reply.corr_id);
        }
    }
    
    void handle_alert(const Alert& alert) {
        // Check global actionable limit
        if (alert.severity == "actionable" || alert.severity == "high_conviction") {
            if (!rate_limiter_.check_global_actionable_limit()) {
                spdlog::warn("Global actionable limit reached, skipping alert");
                return;
            }
            rate_limiter_.record_actionable();
        }
        
        // Format alert message
        std::string severity_emoji = "ℹ️";
        if (alert.severity == "actionable") severity_emoji = "⚠️";
        else if (alert.severity == "high_conviction") {
            severity_emoji = "🚨";
        }
        
        std::string formatted_message = fmt::format("{} {} ({})\n{}", 
            severity_emoji, 
            alert.title, 
            alert.severity, 
            alert.message);
        
        telegram_client_.send_message(config_.owner_telegram_id, formatted_message);
    }
    
    void audit_auth_denied(int64_t user_id) {
        // Log authentication denial
        spdlog::info("Authentication denied for user ID: {}", user_id);
        
        Audit audit_event;
        audit_event.event = "auth_denied";
        audit_event.actor.tg_user_id = user_id;
        audit_event.actor.role = "unknown";
        audit_event.detail = fmt::format("Access denied for user {}", user_id);
        audit_event.ts = util::current_iso8601();
        
        redis_bus_.publish_audit_event(audit_event);
    }
    
    void audit_command_used(const std::string& command, int64_t user_id, Role role) {
        std::string role_str = (role == Role::OWNER) ? "owner" : "guest";
        // Log command usage
        spdlog::info("User {} ({}) used command: {}", user_id, role_str, command);

        Audit audit_event;
        audit_event.event = "cmd_used";
        audit_event.actor.tg_user_id = user_id;
        audit_event.actor.role = role_str;
        audit_event.detail = fmt::format("User used command: /{}", command);
        audit_event.ts = util::current_iso8601();

        redis_bus_.publish_audit_event(audit_event);
    }
    
    void audit_guest_login(int64_t user_id) {
        // Log guest login
        spdlog::info("Guest login successful for user ID: {}", user_id);

        Audit audit_event;
        audit_event.event = "guest_login";
        audit_event.actor.tg_user_id = user_id;
        audit_event.actor.role = "guest";
        audit_event.detail = "Guest access granted via PIN";
        audit_event.ts = util::current_iso8601();

        redis_bus_.publish_audit_event(audit_event);
    }

    void cleanup_loop() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::minutes(1));
            
            // Cleanup expired guest sessions
            auth_manager_.cleanup_expired_sessions();

            // Cleanup old rate limit entries
            rate_limiter_.cleanup_old_entries();

            // Cleanup stale pending commands
            {
                auto now = std::chrono::system_clock::now();
                std::lock_guard<std::mutex> lock(pending_commands_mutex_);
                std::erase_if(pending_commands_, [&](const auto& item) {
                    auto const& [key, value] = item;
                    return (now - value.timestamp) > std::chrono::minutes(5);
                });
            }
        }
    }
};
