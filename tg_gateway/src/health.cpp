
#include "health.hpp"

HealthChecker::HealthChecker(RedisBus& redis_bus) : redis_bus_(redis_bus) {}

HealthChecker::Status HealthChecker::check_health(const std::string& mode) {
    Status status;
    status.mode = mode;
    status.redis_connected = redis_bus_.is_connected();
    status.ok = status.redis_connected;
    
    if (!status.redis_connected) {
        status.last_error = "Redis connection failed";
    }
    
    return status;
}
