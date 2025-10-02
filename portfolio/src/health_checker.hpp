
#pragma once
#include <string>
#include <atomic>
#include <thread>
#include <memory>

class HealthChecker {
public:
    HealthChecker(const std::string& host, int port);
    ~HealthChecker();
    
    void start();
    void stop();
    bool is_running() const;

    // Non-copyable
    HealthChecker(const HealthChecker&) = delete;
    HealthChecker& operator=(const HealthChecker&) = delete;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};
