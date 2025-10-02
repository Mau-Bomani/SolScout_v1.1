#include "ohlcv_aggregator.hpp"
#include "util.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>

OHLCVAggregator::OHLCVAggregator() {
}

void OHLCVAggregator::add_price_point(const std::string& pool_id, double price, double volume,
                                     const std::chrono::system_clock::time_point& timestamp) {
    if (price <= 0.0 || volume < 0.0) {
        return; // Invalid data
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    PricePoint point{price, volume, timestamp};
    
    // Process for 5-minute bars
    auto bar_start_5m = get_bar_start(timestamp, 5);
    std::string key_5m = make_bar_key(pool_id, 5, bar_start_5m);
    
    auto it_5m = active_bars_.find(key_5m);
    if (it_5m == active_bars_.end()) {
        BarBuilder builder;
        builder.pool_id = pool_id;
        builder.interval_minutes = 5;
        builder.bar_start = bar_start_5m;
        builder.add_point(point);
        active_bars_[key_5m] = std::move(builder);
    } else {
        it_5m->second.add_point(point);
    }
    
    // Process for 15-minute bars
    auto bar_start_15m = get_bar_start(timestamp, 15);
    std::string key_15m = make_bar_key(pool_id, 15, bar_start_15m);
    
    auto it_15m = active_bars_.find(key_15m);
    if (it_15m == active_bars_.end()) {
        BarBuilder builder;
        builder.pool_id = pool_id;
        builder.interval_minutes = 15;
        builder.bar_start = bar_start_15m;
        builder.add_point(point);
        active_bars_[key_15m] = std::move(builder);
    } else {
        it_15m->second.add_point(point);
    }
    
    // Check for completed bars
    auto now = std::chrono::system_clock::now();
    auto it = active_bars_.begin();
    while (it != active_bars_.end()) {
        if (it->second.is_complete(now)) {
            if (it->second.has_data) {
                completed_bars_.push_back(it->second.to_bar());
            }
            it = active_bars_.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<OHLCVBar> OHLCVAggregator::get_completed_bars(int interval_minutes) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<OHLCVBar> bars;
    for (const auto& bar : completed_bars_) {
        if (bar.interval_minutes == interval_minutes) {
            bars.push_back(bar);
        }
    }
    
    // Clear returned bars to avoid memory buildup
    completed_bars_.erase(
        std::remove_if(completed_bars_.begin(), completed_bars_.end(),
            [interval_minutes](const OHLCVBar& bar) {
                return bar.interval_minutes == interval_minutes;
            }),
        completed_bars_.end()
    );
    
    return bars;
}

std::optional<OHLCVBar> OHLCVAggregator::get_current_bar(const std::string& pool_id, int interval_minutes) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::system_clock::now();
    auto bar_start = get_bar_start(now, interval_minutes);
    std::string key = make_bar_key(pool_id, interval_minutes, bar_start);
    
    auto it = active_bars_.find(key);
    if (it != active_bars_.end() && it->second.has_data) {
        return it->second.to_bar();
    }
    
    return std::nullopt;
}

std::vector<OHLCVBar> OHLCVAggregator::flush_all_bars() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<OHLCVBar> all_bars = completed_bars_;
    
    // Add all active bars with data
    for (const auto& [key, builder] : active_bars_) {
        if (builder.has_data) {
            all_bars.push_back(builder.to_bar());
        }
    }
    
    completed_bars_.clear();
    active_bars_.clear();
    
    return all_bars;
}

void OHLCVAggregator::cleanup_old_bars(std::chrono::hours max_age) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto cutoff = std::chrono::system_clock::now() - max_age;
    
    // Clean up active bars
    auto it = active_bars_.begin();
    while (it != active_bars_.end()) {
        if (it->second.bar_start < cutoff) {
            it = active_bars_.erase(it);
        } else {
            ++it;
        }
    }
    
    // Clean up completed bars
    completed_bars_.erase(
        std::remove_if(completed_bars_.begin(), completed_bars_.end(),
            [cutoff](const OHLCVBar& bar) {
                return bar.timestamp < cutoff;
            }),
        completed_bars_.end()
    );
}

std::string OHLCVAggregator::make_bar_key(const std::string& pool_id, int interval_minutes,
                                         const std::chrono::system_clock::time_point& bar_start) {
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(bar_start.time_since_epoch()).count();
    return pool_id + ":" + std::to_string(interval_minutes) + ":" + std::to_string(timestamp);
}

std::chrono::system_clock::time_point OHLCVAggregator::get_bar_start(
    const std::chrono::system_clock::time_point& timestamp, int interval_minutes) {
    return util::round_to_interval(timestamp, interval_minutes);
}

void OHLCVAggregator::BarBuilder::add_point(const PricePoint& point) {
    if (points_.empty()) {
        open_price_ = point.price;
        high_price_ = point.price;
        low_price_ = point.price;
        volume_ = point.volume;
        has_data = true;
    } else {
        high_price_ = std::max(high_price_, point.price);
        low_price_ = std::min(low_price_, point.price);
        volume_ += point.volume;
    }
    points_.push_back(point);
}

bool OHLCVAggregator::BarBuilder::is_complete(const std::chrono::system_clock::time_point& now) const {
    auto duration = std::chrono::duration_cast<std::chrono::minutes>(now - bar_start);
    return duration.count() >= interval_minutes;
}

OHLCVBar OHLCVAggregator::BarBuilder::to_bar() const {
    OHLCVBar bar;
    bar.pool_id = pool_id;
    bar.interval_minutes = interval_minutes;
    bar.timestamp = bar_start;
    bar.open_price = open_price_;
    bar.high_price = high_price_;
    bar.low_price = low_price_;
    bar.close_price = points_.back().price;
    bar.volume = volume_;
    return bar;
}
