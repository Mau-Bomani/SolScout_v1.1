#include "config.hpp"
#include "service.hpp"
#include <spdlog/spdlog.h>
#include <signal.h>
#include <memory>

// Global pointer to the service to allow signal handler to access it
std::unique_ptr<Service> service_ptr;

void signal_handler(int signum) {
    spdlog::info("Caught signal {}, shutting down...", signum);
    if (service_ptr) {
        service_ptr->stop();
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

        // 3. Register signal handlers for graceful shutdown
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);

        // 4. Create and run the service
        service_ptr = std::make_unique<Service>(config);
        service_ptr->run();

    } catch (const std::exception& e) {
        spdlog::critical("A critical error occurred: {}", e.what());
        return 1;
    }

    spdlog::info("Ingestor has shut down gracefully.");
    return 0;
}