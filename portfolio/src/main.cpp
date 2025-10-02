#include "config.hpp"
#include "portfolio_service.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <signal.h>
#include <memory>

std::unique_ptr<PortfolioService> service;

void signal_handler(int signum) {
    spdlog::info("Signal {} received, shutting down...", signum);
    if (service) {
        service->stop();
    }
}

int main() {
    try {
        // 1. Load configuration
        Config config = Config::from_env();
        config.validate();

        // 2. Setup logging
        spdlog::set_level(spdlog::level::from_str(config.log_level));
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [tid %t] %v");
        spdlog::info("Log level set to '{}'", config.log_level);
        spdlog::info("Starting {}...", config.service_name);

        // 3. Setup signal handling for graceful shutdown
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);

        // 4. Initialize and run the service
        service = std::make_unique<PortfolioService>(config);
        service->run();

        spdlog::info("{} has shut down.", config.service_name);

    } catch (const std::exception& e) {
        spdlog::critical("Fatal error during initialization or runtime: {}", e.what());
        return 1;
    }

    return 0;
}