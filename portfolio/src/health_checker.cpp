#include "health_checker.hpp"
#include <spdlog/spdlog.h>
#include <httplib.h>
#include <thread>
#include <chrono>
#include <atomic>

class HealthChecker::Impl {
public:
    Impl(const std::string& host, int port) 
        : host_(host), port_(port), running_(false) {}

    ~Impl() {
        stop();
    }

    void start() {
        if (running_) {
            spdlog::warn("Health checker already running");
            return;
        }

        running_ = true;
        health_thread_ = std::thread([this]() {
            run_health_server();
        });

        spdlog::info("Health checker started on {}:{}", host_, port_);
    }

    void stop() {
        if (!running_) return;

        running_ = false;
        if (health_thread_.joinable()) {
            health_thread_.join();
        }

        spdlog::info("Health checker stopped");
    }

    bool is_running() const {
        return running_;
    }

private:
    void run_health_server() {
        httplib::Server server;

        server.Get("/health", [](const httplib::Request&, httplib::Response& res) {
            res.set_content("{"status":"healthy","service":"portfolio"}", "application/json");
        });

        server.Get("/ready", [](const httplib::Request&, httplib::Response& res) {
            res.set_content("{"status":"ready","service":"portfolio"}", "application/json");
        });

        spdlog::info("Health server listening on {}:{}", host_, port_);
        
        if (!server.listen(host_.c_str(), port_)) {
            spdlog::error("Failed to start health server on {}:{}", host_, port_);
        }
    }

    std::string host_;
    int port_;
    std::atomic<bool> running_;
    std::thread health_thread_;
};

// Public interface implementation
HealthChecker::HealthChecker(const std::string& host, int port) 
    : pImpl_(std::make_unique<Impl>(host, port)) {}

HealthChecker::~HealthChecker() = default;

void HealthChecker::start() {
    pImpl_->start();
}

void HealthChecker::stop() {
    pImpl_->stop();
}

bool HealthChecker::is_running() const {
    return pImpl_->is_running();
}
