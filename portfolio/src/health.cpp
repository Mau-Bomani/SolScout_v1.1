
#include "health.hpp"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <thread>

class HealthChecker::Impl {
public:
    Impl(const Config& config, DatabaseManager* db_manager)
        : config_(config), db_manager_(db_manager), running_(false) {}

    ~Impl() {
        stop();
    }

    void start() {
        running_ = true;
        server_thread_ = std::thread([this]() {
            httplib::Server server;
            
            server.Get("/health", [this](const httplib::Request&, httplib::Response& res) {
                nlohmann::json health_status;
                health_status["service"] = config_.service_name;
                health_status["status"] = "healthy";
                health_status["timestamp"] = util::current_iso8601();
                
                // Check database health
                bool db_healthy = db_manager_->is_healthy();
                health_status["components"]["database"] = db_healthy ? "healthy" : "unhealthy";
                
                if (!db_healthy) {
                    health_status["status"] = "unhealthy";
                    res.status = 503;
                } else {
                    res.status = 200;
                }
                
                res.set_content(health_status.dump(2), "application/json");
            });
            
            spdlog::info("Health check server starting on {}:{}", config_.health_host, config_.health_port);
            server.listen(config_.health_host.c_str(), config_.health_port);
        });
    }

    void stop() {
        if (running_) {
            running_ = false;
            if (server_thread_.joinable()) {
                server_thread_.join();
            }
            spdlog::info("Health check server stopped");
        }
    }

private:
    Config config_;
    DatabaseManager* db_manager_;
    std::atomic<bool> running_;
    std::thread server_thread_;
};

HealthChecker::HealthChecker(const Config& config, DatabaseManager* db_manager)
    : pImpl_(std::make_unique<Impl>(config, db_manager)) {}

HealthChecker::~HealthChecker() = default;

void HealthChecker::start() {
    pImpl_->start();
}

void HealthChecker::stop() {
    pImpl_->stop();
}
