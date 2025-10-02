#pragma once

#include "config.hpp"
#include <chrono>
#include <mutex>
#include <vector>

class RegimeDetector {
public:
    explicit RegimeDetector(const Config& config);
    
    // Check if we're in a risk-on regime
    bool is_risk_on() const;
    
    // Update the regime based on market data
    void update_regime(double sol_price, double sol_24h_change_pct);
    
    // Get the current regime as a string
    std::string get_regime_string() const;

private:
    struct RegimeDataPoint {
        double sol_price;
        double sol_24h_change_pct;
        std::chrono::system_clock::time_point timestamp;
    };
    
    const Config& config_;
    mutable std::mutex mutex_;
    std::vector<RegimeDataPoint> data_points_;
    bool risk_on_;
};
