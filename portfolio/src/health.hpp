
#pragma once
#include "config.hpp"
#include "database_manager.hpp"
#include <memory>

class HealthChecker {
public:
    HealthChecker(const Config& config, DatabaseManager* db_manager);
    ~HealthChecker();
    
    void start();
    void stop();

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};
