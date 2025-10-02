#include "signals.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>
#include <fmt/format.h>

SignalCalculator::SignalCalculator(const Config& config) : config_(config) {}

SignalResult SignalCalculator::calculate_signals(
    const MarketUpdate& update,
    const std::optional<TokenMetadata>& metadata,
    const std::vector<std::string>& token_list_mints
) {
    SignalResult result;
    
    // Calculate individual signals
    result.s1_liquidity = calculate_s1_liquidity(update);
    result.s2_volume = calculate_s2_volume(update);
    result.s3_momentum_1h = calculate_s3_momentum_1h(update);
    result.s4_momentum_24h = calculate_s4_momentum_24h(update);
    result.s5_volatility = calculate_s5_volatility(update);
    result.s6_price_discovery = calculate_s6_price_discovery(update);
    result.s7_rug_risk = calculate_s7_rug_risk(update, metadata);
    result.s8_tradability = calculate_s8_tradability(update);
    result.s9_relative_strength = calculate_s9_relative_strength(update);
    result.s10_route_quality = calculate_s10_route_quality(update);
    result.n1_hygiene = calculate_n1_hygiene(update.mint_base, token_list_mints);
    
    // Calculate data quality
    result.data_quality = calculate_data_quality(update);
    
    // Generate reasons
    result.reasons = generate_reasons(update, metadata, result);
    
    return result;
}

double SignalCalculator::calculate_s1_liquidity(const MarketUpdate& update) {
    // S1: Liquidity score
    if (update.liq_usd <= 0) {
        return 0.0;
    }
    
    // Normalize liquidity on a 0-1 scale
    // 0.0 at 0 USD
    // 0.5 at min_liquidity_actionable (150k)
    // 0.8 at 500k
    // 0.9 at 1M
    // 1.0 at 2M+
    
    if (update.liq_usd < config_.min_liquidity_headsup) {
        return 0.0;
    } else if (update.liq_usd < config_.min_liquidity_actionable) {
        return 0.3 + 0.2 * (update.liq_usd - config_.min_liquidity_headsup) / 
                          (config_.min_liquidity_actionable - config_.min_liquidity_headsup);
    } else if (update.liq_usd < 500000) {
        return 0.5 + 0.3 * (update.liq_usd - config_.min_liquidity_actionable) / 
                          (500000 - config_.min_liquidity_actionable);
    } else if (update.liq_usd < 1000000) {
        return 0.8 + 0.1 * (update.liq_usd - 500000) / 500000;
    } else if (update.liq_usd < 2000000) {
        return 0.9 + 0.1 * (update.liq_usd - 1000000) / 1000000;
    } else {
        return 1.0;
    }
}

double SignalCalculator::calculate_s2_volume(const MarketUpdate& update) {
    // S2: Volume score
    if (update.vol24h_usd <= 0) {
        return 0.0;
    }
    
    // Normalize volume on a 0-1 scale
    // 0.0 at 0 USD
    // 0.5 at min_volume_actionable (500k)
    // 0.8 at 2M
    // 0.9 at 5M
    // 1.0 at 10M+
    
    if (update.vol24h_usd < config_.min_volume_headsup) {
        return 0.0;
    } else if (update.vol24h_usd < config_.min_volume_actionable) {
        return 0.3 + 0.2 * (update.vol24h_usd - config_.min_volume_headsup) / 
                          (config_.min_volume_actionable - config_.min_volume_headsup);
    } else if (update.vol24h_usd < 2000000) {
        return 0.5 + 0.3 * (update.vol24h_usd - config_.min_volume_actionable) / 
                          (2000000 - config_.min_volume_actionable);
    } else if (update.vol24h_usd < 5000000) {
        return 0.8 + 0.1 * (update.vol24h_usd - 2000000) / 3000000;
    } else if (update.vol24h_usd < 10000000) {
        return 0.9 + 0.1 * (update.vol24h_usd - 5000000) / 5000000;
    } else {
        return 1.0;
    }
}

double SignalCalculator::calculate_s3_momentum_1h(const MarketUpdate& update) {
    // S3: 1-hour momentum score
    auto it = update.bars.find("5m");
    if (it == update.bars.end()) {
        return 0.5; // Neutral if no data
    }
    
    const auto& bar_5m = it->second;
    
    // Calculate 1h momentum using 5m bar
    double m1h_pct = ((bar_5m.close / bar_5m.open) - 1.0) * 100.0;
    
    // Normalize momentum on a 0-1 scale
    // 0.0 at -10% or worse
    // 0.3 at -5%
    // 0.5 at 0%
    // 0.7 at +1% (min_m1h_pct)
    // 0.9 at +6%
    // 1.0 at +12% (max_m1h_pct) or better
    
    if (m1h_pct <= -10.0) {
        return 0.0;
    } else if (m1h_pct <= -5.0) {
        return 0.0 + 0.3 * (m1h_pct + 10.0) / 5.0;
    } else if (m1h_pct <= 0.0) {
        return 0.3 + 0.2 * (m1h_pct + 5.0) / 5.0;
    } else if (m1h_pct < config_.min_m1h_pct) {
        return 0.5 + 0.2 * m1h_pct / config_.min_m1h_pct;
    } else if (m1h_pct <= 6.0) {
        return 0.7 + 0.2 * (m1h_pct - config_.min_m1h_pct) / (6.0 - config_.min_m1h_pct);
    } else if (m1h_pct <= config_.max_m1h_pct) {
        return 0.9 + 0.1 * (m1h_pct - 6.0) / (config_.max_m1h_pct - 6.0);
    } else {
        return 1.0;
    }
}

double SignalCalculator::calculate_s4_momentum_24h(const MarketUpdate& update) {
    // S4: 24-hour momentum score
    // For simplicity, we'll use the 24h change directly
    // In a real implementation, you might want to use OHLCV data
    
    // Normalize momentum on a 0-1 scale
    // 0.0 at -30% or worse
    // 0.3 at -10%
    // 0.5 at 0%
    // 0.7 at +2% (min_m24h_pct)
    // 0.9 at +20%
    // 1.0 at +60% (max_m24h_pct) or better
    
    // Assuming we have a 24h change percentage
    double m24h_pct = 0.0;
    
    // Try to extract from bars if available
    auto it = update.bars.find("15m");
    if (it != update.bars.end()) {
        const auto& bar_15m = it->second;
        m24h_pct = ((bar_15m.close / bar_15m.open) - 1.0) * 100.0;
    }
    
    if (m24h_pct <= -30.0) {
        return 0.0;
    } else if (m24h_pct <= -10.0) {
        return 0.0 + 0.3 * (m24h_pct + 30.0) / 20.0;
    } else if (m24h_pct <= 0.0) {
        return 0.3 + 0.2 * (m24h_pct + 10.0) / 10.0;
    } else if (m24h_pct < config_.min_m24h_pct) {
        return 0.5 + 0.2 * m24h_pct / config_.min_m24h_pct;
    } else if (m24h_pct <= 20.0) {
        return 0.7 + 0.2 * (m24h_pct - config_.min_m24h_pct) / (20.0 - config_.min_m24h_pct);
    } else if (m24h_pct <= config_.max_m24h_pct) {
        return 0.9 + 0.1 * (m24h_pct - 20.0) / (config_.max_m24h_pct - 20.0);
    } else {
        return 1.0;
    }
}

double SignalCalculator::calculate_s5_volatility(const MarketUpdate& update) {
    // S5: Volatility score
    // For simplicity, we'll use the high-low range from the 15m bar
    
    auto it = update.bars.find("15m");
    if (it == update.bars.end()) {
        return 0.5; // Neutral if no data
    }
    
    const auto& bar_15m = it->second;
    
    // Calculate volatility as (high-low)/low
    double volatility = ((bar_15m.high - bar_15m.low) / bar_15m.low) * 100.0;
    
    // Normalize volatility on a 0-1 scale
    // 0.0 at 0% (no volatility)
    // 0.5 at 5%
    // 0.8 at 10%
    // 1.0 at 20%+
    
    if (volatility <= 0.0) {
        return 0.0;
    } else if (volatility <= 5.0) {
        return 0.0 + 0.5 * volatility / 5.0;
    } else if (volatility <= 10.0) {
        return 0.5 + 0.3 * (volatility - 5.0) / 5.0;
    } else if (volatility <= 20.0) {
        return 0.8 + 0.2 * (volatility - 10.0) / 10.0;
    } else {
        return 1.0;
    }
}

double SignalCalculator::calculate_s6_price_discovery(const MarketUpdate& update) {
    // S6: Price discovery score
    // This is a more complex signal that might involve looking at price action patterns
    // For simplicity, we'll use a combination of volume and volatility
    
    double s2 = calculate_s2_volume(update);
    double s5 = calculate_s5_volatility(update);
    
    // Price discovery is good when there's high volume and moderate volatility
    return 0.4 * s2 + 0.6 * std::min(s5, 0.8);
}

double SignalCalculator::calculate_s7_rug_risk(const MarketUpdate& update, const std::optional<TokenMetadata>& metadata) {
    // S7: Rug risk score (higher is better, meaning lower risk)
    
    // Start with a base score
    double score = 0.7;
    
    // If we have metadata, adjust based on token properties
    if (metadata) {
        // Age factor: younger tokens are riskier
        double age_factor = std::min(1.0, update.age_hours / 720.0); // 30 days = 720 hours
        
        // Top holder concentration: higher is riskier
        double holder_factor = 1.0;
        if (metadata->top_holder_pct > 0) {
            holder_factor = std::max(0.0, 1.0 - metadata->top_holder_pct / 100.0);
        }
        
        // Risky authorities
        double auth_factor = metadata->risky_authorities ? 0.7 : 1.0;
        
        // Combine factors
        score = 0.7 * age_factor * holder_factor * auth_factor;
    } else {
        // Without metadata, be more conservative
        score = 0.5;
    }
    
    // Cap at 0.9 - there's always some risk
    return std::min(0.9, score);
}

double SignalCalculator::calculate_s8_tradability(const MarketUpdate& update) {
    // S8: Tradability score based on spread and impact
    
    // Check if spread and impact are within acceptable ranges
    if (update.spread_pct > config_.max_spread_pct || 
        update.impact_1pct_pct > config_.max_impact_pct) {
        return 0.0;
    }
    
    // Normalize spread (lower is better)
    double spread_score = 1.0 - (update.spread_pct / config_.max_spread_pct);
    
    // Normalize impact (lower is better)
    double impact_score = 1.0 - (update.impact_1pct_pct / config_.max_impact_pct);
    
    // Combine scores (weighted average)
    return 0.4 * spread_score + 0.6 * impact_score;
}

double SignalCalculator::calculate_s9_relative_strength(const MarketUpdate& update) {
    // S9: Relative strength compared to market
    // This would typically compare the token's performance to SOL or a basket
    // For simplicity, we'll use a placeholder value
    return 0.7;
}

double SignalCalculator::calculate_s10_route_quality(const MarketUpdate& update) {
    // S10: Route quality score
    
    // Check if route meets requirements
    if (!update.route.ok || 
        update.route.hops > config_.max_route_hops || 
        update.route.deviation_pct > config_.max_route_deviation) {
        return 0.0;
    }
    
    // Normalize hops (fewer is better)
    double hops_score = 1.0 - ((double)update.route.hops - 1.0) / (config_.max_route_hops - 1.0);
    
    // Normalize deviation (lower is better)
    double deviation_score = 1.0 - (update.route.deviation_pct / config_.max_route_deviation);
    
    // Combine scores (weighted average)
    return 0.3 * hops_score + 0.7 * deviation_score;
}

double SignalCalculator::calculate_n1_hygiene(const std::string& mint, const std::vector<std::string>& token_list_mints) {
    // N1: Token list hygiene
    // Check if the token is on a widely mirrored list
    
    for (const auto& list_mint : token_list_mints) {
        if (list_mint == mint) {
            return 1.0; // Token is on a recognized list
        }
    }
    
    return 0.0; // Token is not on any recognized list
}

double SignalCalculator::calculate_data_quality(const MarketUpdate& update) {
    // Start with perfect data quality
    double dq = config_.dq_start;
    
    // Check for missing or reconstructed data
    if (update.liq_usd <= 0) {
        dq -= config_.dq_penalty_per_missing;
    }
    
    if (update.vol24h_usd <= 0) {
        dq -= config_.dq_penalty_per_missing;
    }
    
    if (update.bars.find("5m") == update.bars.end()) {
        dq -= config_.dq_penalty_per_missing;
    }
    
    if (update.bars.find("15m") == update.bars.end()) {
        dq -= config_.dq_penalty_per_missing;
    }
    
    if (update.spread_pct <= 0) {
        dq -= config_.dq_penalty_per_missing;
    }
    
    if (update.impact_1pct_pct <= 0) {
        dq -= config_.dq_penalty_per_missing;
    }
    
    // Cap at 0
    return std::max(0.0, dq);
}

std::vector<std::string> SignalCalculator::generate_reasons(
    const MarketUpdate& update,
    const std::optional<TokenMetadata>& metadata,
    const SignalResult& result
) {
    std::vector<std::string> reasons;
    
    // Liquidity reason
    if (update.liq_usd >= config_.min_liquidity_actionable) {
        reasons.push_back(fmt::format("Liq ${:.1f}k", update.liq_usd / 1000.0));
    } else if (update.liq_usd >= config_.min_liquidity_headsup) {
        reasons.push_back(fmt::format("Liq ${:.1f}k (low)", update.liq_usd / 1000.0));
    }
    
    // Volume reason
    if (update.vol24h_usd >= config_.min_volume_actionable) {
        reasons.push_back(fmt::format("Vol24h ${:.1f}M", update.vol24h_usd / 1000000.0));
    } else if (update.vol24h_usd >= config_.min_volume_headsup) {
        reasons.push_back(fmt::format("Vol24h ${:.1f}k (low)", update.vol24h_usd / 1000.0));
    }
    
    // Momentum reasons
    auto it_5m = update.bars.find("5m");
    if (it_5m != update.bars.end()) {
        const auto& bar_5m = it_5m->second;
        double m1h_pct = ((bar_5m.close / bar_5m.open) - 1.0) * 100.0;
        
        if (m1h_pct >= config_.min_m1h_pct) {
            reasons.push_back(fmt::format("m1h +{:.1f}%", m1h_pct));
        } else if (m1h_pct <= -5.0) {
            reasons.push_back(fmt::format("m1h {:.1f}%", m1h_pct));
        }
    }
    
    auto it_15m = update.bars.find("15m");
    if (it_15m != update.bars.end()) {
        const auto& bar_15m = it_15m->second;
        double m24h_pct = ((bar_15m.close / bar_15m.open) - 1.0) * 100.0;
        
        if (m24h_pct >= config_.min_m24h_pct) {
            reasons.push_back(fmt::format("m24h +{:.1f}%", m24h_pct));
        } else if (m24h_pct <= -10.0) {
            reasons.push_back(fmt::format("m24h {:.1f}%", m24h_pct));
        }
    }
    
    // Age reason
    if (update.age_hours < config_.young_token_hours) {
        reasons.push_back(fmt::format("age {:.1f}h (young)", update.age_hours));
    } else {
        int days = static_cast<int>(update.age_hours / 24);
        reasons.push_back(fmt::format("age {}d", days));
    }
    
    // Tradability reason
    if (result.s8_tradability >= 0.8) {
        reasons.push_back(fmt::format("spread {:.2f}%, impact {:.2f}%", 
                                     update.spread_pct, update.impact_1pct_pct));
    } else if (update.spread_pct > config_.max_spread_pct || 
               update.impact_1pct_pct > config_.max_impact_pct) {
        reasons.push_back(fmt::format("poor liquidity: spread {:.2f}%, impact {:.2f}%", 
                                     update.spread_pct, update.impact_1pct_pct));
    }
    
    // Route reason
    if (update.route.ok && update.route.hops <= config_.max_route_hops && 
        update.route.deviation_pct <= config_.max_route_deviation) {
        reasons.push_back(fmt::format("route {} hops, dev {:.2f}%", 
                                     update.route.hops, update.route.deviation_pct));
    } else {
        reasons.push_back("route issues");
    }
    
    // Token metadata reasons
    if (metadata) {
        // FDV/Liq ratio
        double fdv_liq_ratio = 0.0;
        if (update.liq_usd > 0) {
            // This is a simplified calculation; in reality you'd need market cap data
            fdv_liq_ratio = 10.0; // Placeholder
            
            if (fdv_liq_ratio > config_.max_fdv_liq) {
                reasons.push_back(fmt::format("FDV/Liq {:.1f} (high)", fdv_liq_ratio));
            } else if (fdv_liq_ratio < config_.min_fdv_liq) {
                reasons.push_back(fmt::format("FDV/Liq {:.1f} (low)", fdv_liq_ratio));
            } else if (fdv_liq_ratio >= config_.preferred_min_fdv_liq && 
                      fdv_liq_ratio <= config_.preferred_max_fdv_liq) {
                reasons.push_back(fmt::format("FDV/Liq {:.1f} (good)", fdv_liq_ratio));
            }
        }
        
        // Top holder concentration
        if (metadata->top_holder_pct > config_.max_top_holder_pct) {
            reasons.push_back(fmt::format("top holder {:.1f}% (high)", metadata->top_holder_pct));
        }
        
        // Risky authorities
        if (metadata->risky_authorities) {
            reasons.push_back("risky authorities");
        }
        
        // Token list status
        if (!metadata->on_token_list) {
            reasons.push_back("not on token list");
        }
    }
    
    // Data quality reason
    if (result.data_quality < config_.min_dq_for_actionable) {
        reasons.push_back(fmt::format("DQ {:.2f} (low)", result.data_quality));
    }
    
    return reasons;
}

std::string SignalResult::to_string() const {
    std::string result = fmt::format("Confidence: {}, Band: {}\n", confidence_score, band);
    result += "Signals:\n";
    result += fmt::format("  S1 (Liquidity): {:.2f}\n", s1_liquidity);
    result += fmt::format("  S2 (Volume): {:.2f}\n", s2_volume);
    result += fmt::format("  S3 (Momentum 1h): {:.2f}\n", s3_momentum_1h);
    result += fmt::format("  S4 (Momentum 24h): {:.2f}\n", s4_momentum_24h);
    result += fmt::format("  S5 (Volatility): {:.2f}\n", s5_volatility);
    result += fmt::format("  S6 (Price Discovery): {:.2f}\n", s6_price_discovery);
    result += fmt::format("  S7 (Rug Risk): {:.2f}\n", s7_rug_risk);
    result += fmt::format("  S8 (Tradability): {:.2f}\n", s8_tradability);
    result += fmt::format("  S9 (Relative Strength): {:.2f}\n", s9_relative_strength);
    result += fmt::format("  S10 (Route Quality): {:.2f}\n", s10_route_quality);
    result += fmt::format("  N1 (Hygiene): {:.2f}\n", n1_hygiene);
    result += fmt::format("Data Quality: {:.2f}\n", data_quality);
    
    result += "Reasons:\n";
    for (const auto& reason : reasons) {
        result += fmt::format("  - {}\n", reason);
    }
    
    result += fmt::format("Entry Confirmed: {}\n", entry_confirmed ? "Yes" : "No");
    result += fmt::format("Net Edge OK: {}\n", net_edge_ok ? "Yes" : "No");
    
    return result;
}
