#pragma once

#include "config.hpp"
#include "redis_bus.hpp"
#include "audit_logger.hpp"
#include "throttler.hpp"
#include "deduplicator.hpp"
#include "formatter.hpp"
#include "types.hpp"

#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

class NotifierService {
public:
    explicit NotifierService(const Config& config);
    ~NotifierService();

    void run();
    void stop();

private:
    // Main service loop
    void service_loop();

    // Message handlers
    void handle_inbound_alert(const InboundAlert& alert);
    void handle_command_request(const CommandRequest& request);

    // Helper to build status reply
    std::string get_status_report();

    Config config_;
    
    // Service components
    std::unique_ptr<RedisBus> redis_bus_;
    std::unique_ptr<AuditLogger> audit_logger_;
    std::shared_ptr<sw::redis::Redis> redis_client_; // Shared for throttler/deduplicator
    std::unique_ptr<Throttler> throttler_;
    std::unique_ptr<Deduplicator> deduplicator_;

    // Thread management
    std::atomic<bool> running_{false};
    std::thread service_thread_;

    // Queues for decoupling Redis threads from processing logic
    std::mutex alert_queue_mutex_;
    std::condition_variable alert_queue_cv_;
    std::queue<InboundAlert> alert_queue_;

    std::mutex command_queue_mutex_;
    std::condition_variable command_queue_cv_;
    std::queue<CommandRequest> command_queue_;
};
