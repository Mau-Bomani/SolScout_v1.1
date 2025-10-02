
#pragma once
#include "types.hpp"
#include <unordered_map>
#include <mutex>
#include <vector>

class OHLCVAggregator {
public:
    OHLCVAggregator();
    
    // Add a price point to the aggregator
    void add_price_point(const std::string& pool_id, double price, double volume, 
                        const std::chrono::system_clock::time_point& timestamp);
    
    // Get completed bars for a specific interval
    std::vector<OHLCVBar> get_completed_bars(int interval_minutes);
    
    // Get current incomplete bar for a pool
    std::optional<OHLCVBar> get_current_bar(const std::string& pool_id, int interval_minutes);
    
    // Force completion of current bars (useful for shutdown)
    std::vector<OHLCVBar> flush_all_bars();
    
    // Clean up old incomplete bars
    void cleanup_old_bars(std::chrono::hours max_age = std::chrono::hours(24));

private:
    struct PricePoint {
        double price;
        double volume;
        std::chrono::system_clock::time_point timestamp;
    };
    
    struct BarBuilder {
        std::string pool_id;
        int interval_minutes;
        std::chrono::system_clock::time_point bar_start;
        double open = 0.0;
        double high = 0.0;
        double low = 0.0;
        double close = 0.0;
        double volume = 0.0;
        bool has_data = false;
        
        void add_point(const PricePoint& point);
        OHLCVBar to_bar() const;
        bool is_complete(const std::chrono::system_clock::time_point& now) const;
    };
    
    std::string make_bar_key(const std::string& pool_id, int interval_minutes, 
                            const std::chrono::system_clock::time_point& bar_start);
    
    std::chrono::system_clock::time_point get_bar_start(
        const std::chrono::system_clock::time_point& timestamp, int interval_minutes);
    
    std::mutex mutex_;
    std::unordered_map<std::string, BarBuilder> active_bars_;
    std::vector<OHLCVBar> completed_bars_;
};
