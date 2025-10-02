#include "scoring.hpp"
#include <algorithm>
#include <cmath>

ConfidenceScorer::ConfidenceScorer(const Config& config) : config_(config) {}

int ConfidenceScorer::calculate_confidence(const SignalResult& signals) {
    // Check data quality first
    if (signals.data_quality < config_.min_dq_for_actionable) {
        return std::min(50, static_cast<int>(signals.data_quality * 100));
    }
    
    // Base weights for each signal
    const double weights[11] = {
        0.15,  // S1: Liquidity
        0.15,  // S2: Volume
        0.10,  // S3: Momentum 1h
        0.10,  // S4: Momentum 24h
        0.05,  // S5: Volatility
        0.05,  // S6: Price Discovery
        0.20,  // S7: Rug Risk
        0.10,  // S8: Tradability
        0.05,  // S9: Relative Strength
        0.05,  // S10: Route Quality
        0.00   // N1: Hygiene (applied separately)
    };
    
    // Calculate weighted sum
    double weighted_sum = 
        weights[0] * signals.s1_liquidity +
        weights[1] * signals.s2_volume +
        weights[2] * signals.s3_momentum_1h +
        weights[3] * signals.s4_momentum_24h +
        weights[4] * signals.s5_volatility +
        weights[5] * signals.s6_price_discovery +
        weights[6] * signals.s7_rug_risk +
        weights[7] * signals.s8_tradability +
        weights[8] * signals.s9_relative_strength +
        weights[9] * signals.s10_route_quality;
    
    // Convert to 0-100 scale
    int base_score = static_cast<int>(weighted_sum * 100);
    
    // Apply hygiene penalty if not on token list
    if (signals.n1_hygiene < 0.5) {
        base_score -= config_.hygiene_penalty;
    }
    
    // Cap rug risk for young tokens
    if (signals.s7_rug_risk < 0.5) {
        base_score = std::min(base_score, config_.max_rug_cap);
    }
    
    // Ensure score is in 0-100 range
    return std::max(0, std::min(100, base_score));
}

std::string ConfidenceScorer::determine_band(int confidence_score, bool entry_confirmed, bool net_edge_ok) {
    // Hard gates first
    if (!entry_confirmed) {
        return "watch";
    }
    
    if (!net_edge_ok) {
        return "watch";
    }
    
    // Determine band based on confidence score
    if (confidence_score >= config_.high_conviction_min) {
        return "high_conviction";
    } else if (confidence_score >= config_.actionable_base_threshold) {
        return "actionable";
    } else if (confidence_score >= config_.headsup_min && confidence_score <= config_.headsup_max) {
        return "heads_up";
    } else {
        return "watch";
    }
}

int ConfidenceScorer::apply_risk_adjustment(int base_score, bool risk_on) {
    // Apply risk regime adjustment
    if (risk_on) {
        return std::min(100, base_score + config_.risk_on_adj);
    } else {
        return std::max(0, base_score + config_.risk_off_adj);
    }
}
