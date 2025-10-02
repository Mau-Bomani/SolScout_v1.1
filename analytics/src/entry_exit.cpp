
#include "entry_exit.hpp"
#include <algorithm>
#include <cmath>

EntryExitChecker::EntryExitChecker(const Config& config) : config_(config) {}

bool EntryExitChecker::check_entry_conditions(const MarketUpdate& update, const SignalResult& signals) {
    // Check minimum age
    if (update.age_hours < config_.min_age_hours) {
        return false;
    }
    
    // Check minimum liquidity
    if (update.liq_usd < config_.min_liquidity_actionable) {
        return false;
    }
    
    // Check minimum volume
    if (update.vol24h_usd < config_.min_volume_actionable) {
        return false;
    }
    
    // Check spread and impact
    if (update.spread_pct > config_.max_spread_pct || 
        update.impact_1pct_pct > config_.max_impact_pct) {
        return false;
    }
    
    // Check route quality
    if (!update.route.ok || 
        update.route.hops > config_.max_route_hops || 
        update.route.deviation_pct > config_.max_route_deviation) {
        return false;
    }
    
    // Check momentum thresholds
    auto it_5m = update.bars.find("5m");
    if (it_5m != update.bars.end()) {
        const auto& bar_5m = it_5m->second;
        double m1h_pct = ((bar_5m.close / bar_5m.open) - 1.0) * 100.0;
        
        if (m1h_pct < config_.min_m1h_pct || m1h_pct > config_.max_m1h_pct) {
            return false;
        }
    } else {
        // No 1h data, can't confirm entry
        return false;
    }
    
    auto it_15m = update.bars.find("15m");
    if (it_15m != update.bars.end()) {
        const auto& bar_15m = it_15m->second;
        double m24h_pct = ((bar_15m.close / bar_15m.open) - 1.0) * 100.0;
        
        if (m24h_pct < config_.min_m24h_pct || m24h_pct > config_.max_m24h_pct) {
            return false;
        }
    } else {
        // No 24h data, can't confirm entry
        return false;
    }
    
    // Check data quality
    if (signals.data_quality < config_.min_dq_for_actionable) {
        return false;
    }
    
    // Special case for young and risky tokens
    if (update.age_hours < config_.young_token_hours && signals.s7_rug_risk < 0.5) {
        // For young and risky tokens, require a higher confidence score
        if (signals.confidence_score < config_.min_c_young_risky) {
            return false;
        }
    }
    
    // All conditions met
    return true;
}

bool EntryExitChecker::check_net_edge(const MarketUpdate& update, const SignalResult& signals) {
    // Calculate upside potential (simplified)
    double upside_potential = 0.0;
    
    // Use momentum as a proxy for upside potential
    auto it_5m = update.bars.find("5m");
    if (it_5m != update.bars.end()) {
        const auto& bar_5m = it_5m->second;
        double m1h_pct = ((bar_5m.close / bar_5m.open) - 1.0) * 100.0;
        upside_potential = std::min(m1h_pct * 2.0, config_.max_upside_cap);
    }
    
    // Calculate downside risk (simplified)
    double downside_risk = update.impact_1pct_pct * 2.0;
    
    // Add spread cost
    downside_risk += update.spread_pct;
    
    // Add lag penalty
    downside_risk += config_.lag_penalty;
    
    // Calculate net edge
    double net_edge = upside_potential - (config_.net_edge_k_factor * downside_risk);
    
    // Net edge must be positive
    return net_edge > 0.0;
}

double EntryExitChecker::calculate_position_size(
    const MarketUpdate& update, 
    const SignalResult& signals,
    double portfolio_value,
    int active_positions
) {
    // Base size as a percentage of portfolio
    double base_pct = config_.default_deployed_pct / config_.max_positions;
    
    // Adjust for active positions
    if (active_positions >= config_.max_positions) {
        return 0.0; // No more positions allowed
    }
    
    // Adjust for confidence
    double confidence_factor = signals.confidence_score / 100.0;
    
    // Adjust for liquidity
    double liquidity_factor = std::min(1.0, update.liq_usd * config_.liquidity_size_factor / portfolio_value);
    
    // Calculate final size
    double position_size_pct = base_pct * confidence_factor * liquidity_factor;
    
    // Ensure we don't exceed max deployed percentage
    double max_additional_pct = config_.max_deployed_pct - (active_positions * base_pct);
    position_size_pct = std::min(position_size_pct, max_additional_pct);
    
    // Convert to absolute value
    return portfolio_value * position_size_pct / 100.0;
}
