
#include "webhook_server.hpp"
#include <spdlog/spdlog.h>

WebhookServer::WebhookServer(const Config& config) 
    : config_(config), running_(false) {
    server_ = std::make_unique<httplib::Server>();
    health_status_.ok = true;
    health_status_.redis_connected = false;
    health_status_.mode = config.gateway_mode;
}

WebhookServer::~WebhookServer() {
    stop();
}

void WebhookServer::start(std::function<void(const nlohmann::json&)> update_handler) {
    setup_routes(update_handler);
    running_ = true;
    
    server_thread_ = std::thread([this]() {
        spdlog::info("Starting webhook server on {}:{}", config_.listen_addr, config_.listen_port);
        server_->listen(config_.listen_addr.c_str(), config_.listen_port);
    });
}

void WebhookServer::stop() {
    if (running_) {
        running_ = false;
        server_->stop();
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }
}

bool WebhookServer::is_running() const {
    return running_;
}

void WebhookServer::set_health_status(const HealthStatus& status) {
    std::lock_guard<std::mutex> lock(health_mutex_);
    health_status_ = status;
}

// Enhanced error handling in webhook processing
void WebhookServer::handleWebhook(const httplib::Request& req, httplib::Response& res) {
    try {
        if (req.body.empty()) {
            spdlog::warn("Received empty webhook body");
            res.status = 400;
            res.set_content("{"error":"Empty body"}", "application/json");
            return;
        }

        nlohmann::json update;
        try {
            update = nlohmann::json::parse(req.body);
        } catch (const nlohmann::json::parse_error& e) {
            spdlog::warn("Failed to parse webhook JSON: {}", e.what());
            res.status = 400;
            res.set_content("{"error":"Invalid JSON"}", "application/json");
            return;
        }

        // Process the update
        if (update_callback_) {
            update_callback_(update);
        }

        res.status = 200;
        res.set_content("{"ok":true}", "application/json");
    } catch (const std::exception& e) {
        spdlog::error("Webhook processing error: {}", e.what());
        res.status = 500;
        res.set_content("{"error":"Internal server error"}", "application/json");
    }
}

void WebhookServer::setup_routes(std::function<void(const nlohmann::json&)> update_handler) {
    // Health endpoint
    server_->Get("/health", [this](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(health_mutex_);
        nlohmann::json health = {
            {"ok", health_status_.ok},
            {"redis", health_status_.redis_connected},
            {"mode", health_status_.mode}
        };
        
        res.set_content(health.dump(), "application/json");
        res.status = health_status_.ok ? 200 : 503;
    });
    
    // Telegram webhook endpoint
    server_->Post("/telegram/webhook", [update_handler](const httplib::Request& req, httplib::Response& res) {
        try {
            if (req.get_header_value("Content-Type").find("application/json") == std::string::npos) {
                res.status = 400;
                res.set_content("Content-Type must be application/json", "text/plain");
                return;
            }
            
            auto json_update = nlohmann::json::parse(req.body);
            update_handler(json_update);
            
            res.status = 200;
            res.set_content("OK", "text/plain");
        } catch (const std::exception& e) {
            spdlog::error("Webhook error: {}", e.what());
            res.status = 400;
            res.set_content("Bad Request", "text/plain");
        }
    });
    
    // Set error handler
    server_->set_error_handler([](const httplib::Request&, httplib::Response& res) {
        res.status = 404;
        res.set_content("Not Found", "text/plain");
    });
}
