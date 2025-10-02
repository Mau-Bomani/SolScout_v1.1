
#pragma once

#include "types.hpp"
#include "config.hpp"

class ConfidenceScorer {
public:
    explicit ConfidenceScorer(const Config& config);
    
    // Calculate confidence score from signal results
    int calculate_confidence(const SignalResult& signals);
    
    // Determine the alert band (actionable, heads-up, etc.)
    std::string determine_band(int confidence_score, bool entry_confirmed, bool net_edge_ok);
    
    // Apply risk regime adjustment
    int apply_risk_adjustment(int base_score, bool risk_on);

private:
    const Config& config_;
};
