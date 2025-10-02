
#pragma once

#include "types.hpp"
#include "config.hpp"
#include <chrono>

class EntryExitChecker {
public:
    explicit EntryExitChecker(const Config& config);
    
    // Check if entry conditions are met
    bool check_entry_conditions(const MarketUpdate& update, const SignalResult& signals);
    
    // Check if the net edge is positive
    bool check_net_edge(const MarketUpdate& update, const SignalResult& signals);
    
    // Calculate position size recommendation
    double calculate_position_size(
        const MarketUpdate& update, 
        const SignalResult& signals,
        double portfolio_value,
        int active_positions
    );

private:
    const Config& config_;
};
