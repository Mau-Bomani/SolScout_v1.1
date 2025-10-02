#include "config.hpp"
#include "analytics_service.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <csignal>
#include <memory>
#include <atomic>
#include <thread>
#include <chrono>

// Global atomic flag to handle termination signals
std::atomic<bool> g_terminate_flag(false);

// Signal handler function
void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        spdlog::info("Termination signal received. Shutting down...");
        g_terminate_flag = true;
    }
}

int main(int argc, char* argv[]) {
    // Set up logger
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>("analytics_service", console_sink);
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::debug); // Default, will be overridden by config
    spdlog::flush_on(spdlog::level::info);

    spdlog::info("Starting SolScout Analytics Service...");

    // Load configuration
    std::string config_path = "config.json"; // Default config file name
    if (argc > 1) {
        config_path = argv[1];
    }

    Config config;
    try {
        config.load(config_path);
        spdlog::set_level(spdlog::level::from_str(config.log_level));
        spdlog::info("Configuration loaded from {}", config_path);
    } catch (const std::exception& e) {
        spdlog::critical("Failed to load configuration: {}", e.what());
        return 1;
    }

    // Register signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Create and run the service
    std::unique_ptr<AnalyticsService> service;
    try {
        service = std::make_unique<AnalyticsService>(config);
        service->run();
    } catch (const std::exception& e) {
        spdlog::critical("Failed to initialize or start the service: {}", e.what());
        return 1;
    }

    // Wait for termination signal
    while (!g_terminate_flag) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Stop the service
    if (service) {
        service->stop();
    }

    spdlog::info("SolScout Analytics Service has shut down gracefully.");
    spdlog::shutdown();
    return 0;
}