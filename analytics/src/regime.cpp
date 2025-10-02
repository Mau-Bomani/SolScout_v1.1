
#include "regime.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <numeric>

RegimeDetector::RegimeDetector(const Config& config) 
    : config_(config), risk_on_(false) {}

bool RegimeDetector::is_risk_on() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return risk_on_;
}

void RegimeDetector::update_regime(double sol_price, double sol_24h_change_pct) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::system_clock::now();
    
    // Add new data point
    RegimeDataPoint point;
    point.sol_price = sol_price;
    point.sol_24h_change_pct = sol_24h_change_pct;
    point.timestamp = now;
    
    data_points_.push_back(point);
    
    // Remove old data points
    data_points_.erase(
        std::remove_if(
            data_points_.begin(),
            data_points_.end(),
            [now](const RegimeDataPoint& point) {
                auto elapsed = std::chrono::duration_cast<std::chrono::hours>(now - point.timestamp).count();
                return elapsed > 24; // Keep 24 hours of data
            }
        ),
        data_points_.end()
    );
    
    // Need at least a few data points to determine regime
    if (data_points_.size() < 3) {
        risk_on_ = false;
        return;
    }
    
    // Calculate average SOL price change
    double avg_change = std::accumulate(
        data_points_.begin(),
        data_points_.end(),
        0.0,
        [](double sum, const RegimeDataPoint& point) {
            return sum + point.sol_24h_change_pct;
        }
    ) / data_points_.size();
    
    // Calculate price momentum (last price vs average of previous prices)
    double current_price = data_points_.back().sol_price;
    double avg_price = 0.0;
    if (data_points_.size() > 1) {
        avg_price = std::accumulate(
            data_points_.begin(),
            data_points_.end() - 1,
            0.0,
            [](double sum, const RegimeDataPoint& point) {
                return sum + point.sol_price;
            }
        ) / (data_points_.size() - 1);
    } else {
        avg_price = current_price;
    }
    
    double price_momentum = ((current_price / avg_price) - 1.0) * 100.0;
    
    // Determine regime
    bool new_risk_on = (avg_change > config_.risk_on_sol_change_threshold) && 
                      (price_momentum > config_.risk_on_momentum_threshold);
    
    // Log regime change
    if (new_risk_on != risk_on_) {
        spdlog::info("Risk regime changed to {}: SOL avg change {:.2f}%, momentum {:.2f}%",
                    new_risk_on ? "RISK-ON" : "RISK-OFF", avg_change, price_momentum);
    }
    
    risk_on_ = new_risk_on;
}

std::string RegimeDetector::get_regime_string() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return risk_on_ ? "RISK-ON" : "RISK-OFF";
}
