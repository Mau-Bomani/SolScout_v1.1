#include "config.hpp"
#include "notifier_service.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <csignal>
#include <condition_variable>
#include <mutex>

// For graceful shutdown
std::unique_ptr<NotifierService> service;
std::mutex shutdown_mutex;
std::condition_variable shutdown_cv;
bool shutdown_requested = false;

void signal_handler(int signum) {
    spdlog::warn("Signal {} received, initiating graceful shutdown.", signum);
    {
        std::lock_guard<std::mutex> lock(shutdown_mutex);
        if (shutdown_requested) return;
        shutdown_requested = true;
    }
    if (service) {
        service->stop();
    }
    shutdown_cv.notify_one();
}

int main() {
    // Setup logging
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>("main", console_sink);
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);

    // Register signal handlers for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    try {
        // Load configuration
        Config config;
        config.load_from_env();
        spdlog::info("Configuration loaded for service: {}", config.service_name);

        // Create and run the service
        service = std::make_unique<NotifierService>(config);
        service->run();

        // Wait for shutdown signal
        {
            std::unique_lock<std::mutex> lock(shutdown_mutex);
            shutdown_cv.wait(lock, [] { return shutdown_requested; });
        }

        spdlog::info("Notifier service has shut down. Exiting.");

    } catch (const std::exception& e) {
        spdlog::critical("A critical error occurred during initialization or runtime: {}", e.what());
        return 1;
    }

    return 0;
}