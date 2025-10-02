
#pragma once
#include "redis_bus.hpp"
#include <string>

class HealthChecker {
public:
    explicit HealthChecker(RedisBus& redis_bus);
    
    struct Status {
        bool ok;
        bool redis_connected;
        std::string mode;
        std::string last_error;
    };
    
    Status check_health(const std::string& mode);
    
private:
    RedisBus& redis_bus_;
};
