#pragma once

#include "types.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <queue>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <thread>
#include <chrono>
#include <random>

namespace vfx {
namespace trading {

// Market data callback types
using TickCallback = std::function<void(const MarketTick&)>;
using QuoteCallback = std::function<void(const Symbol&, Price bid, Price ask)>;
using TradeCallback = std::function<void(const Symbol&, Price price, Volume volume)>;

// Market data statistics
struct MarketDataStats {
    uint64_t total_ticks_received;
    uint64_t ticks_per_second;
    uint64_t total_symbols;
    double avg_latency_us;
    Timestamp last_update;
    std::unordered_map<Symbol, uint64_t> symbol_tick_counts;
};

// Symbol subscription info
struct SymbolSubscription {
    Symbol symbol;
    bool level1_enabled;
    bool level2_enabled;
    bool trades_enabled;
    uint32_t update_frequency_ms;
    Timestamp subscribed_at;
    uint64_t tick_count;
    
    SymbolSubscription(const Symbol& sym) 
        : symbol(sym), level1_enabled(true), level2_enabled(false), 
          trades_enabled(true), update_frequency_ms(100), 
          subscribed_at(std::chrono::high_resolution_clock::now()),
          tick_count(0) {}
};

class MarketDataManager {
private:
    // Market data storage
    std::unordered_map<Symbol, std::shared_ptr<MarketTick>> latest_ticks_;
    std::unordered_map<Symbol, std::queue<MarketTick>> tick_history_;
    std::unordered_map<Symbol, std::shared_ptr<SymbolSubscription>> subscriptions_;
    
    // Thread safety
    mutable std::shared_mutex data_mutex_;
    mutable std::shared_mutex subscriptions_mutex_;
    
    // Data processing
    std::queue<MarketTick> incoming_ticks_;
    std::mutex incoming_mutex_;
    std::condition_variable incoming_cv_;
    
    // Processing threads
    std::thread data_processing_thread_;
    std::thread simulation_thread_;
    std::atomic<bool> running_{false};
    
    // Configuration
    MarketDataConfig config_;
    
    // Callbacks
    TickCallback tick_callback_;
    QuoteCallback quote_callback_;
    TradeCallback trade_callback_;
    
    // Performance metrics
    std::atomic<uint64_t> total_ticks_processed_{0};
    std::atomic<uint64_t> ticks_per_second_{0};
    std::chrono::high_resolution_clock::time_point last_stats_update_;
    
    // Market simulation
    std::unordered_map<Symbol, Price> base_prices_;
    std::random_device rd_;
    std::mt19937 gen_;
    
public:
    MarketDataManager();
    ~MarketDataManager();
    
    // Lifecycle
    void start();
    void stop();
    bool is_running() const { return running_; }
    
    // Configuration
    void set_config(const MarketDataConfig& config);
    const MarketDataConfig& get_config() const { return config_; }
    
    // Subscription management
    bool subscribe(const Symbol& symbol, bool level1 = true, bool level2 = false, bool trades = true);
    bool unsubscribe(const Symbol& symbol);
    void subscribe_multiple(const std::vector<Symbol>& symbols);
    void unsubscribe_all();
    
    std::vector<Symbol> get_subscribed_symbols() const;
    std::shared_ptr<SymbolSubscription> get_subscription(const Symbol& symbol) const;
    
    // Market data queries
    std::shared_ptr<MarketTick> get_latest_tick(const Symbol& symbol) const;
    std::vector<MarketTick> get_tick_history(const Symbol& symbol, uint32_t count = 100) const;
    
    Price get_last_price(const Symbol& symbol) const;
    Price get_bid_price(const Symbol& symbol) const;
    Price get_ask_price(const Symbol& symbol) const;
    
    // Market data injection (for external feeds)
    void inject_tick(const MarketTick& tick);
    void inject_quote(const Symbol& symbol, Price bid, Price ask, Volume bid_size, Volume ask_size);
    void inject_trade(const Symbol& symbol, Price price, Volume volume);
    
    // Callbacks
    void set_tick_callback(TickCallback callback) { tick_callback_ = callback; }
    void set_quote_callback(QuoteCallback callback) { quote_callback_ = callback; }
    void set_trade_callback(TradeCallback callback) { trade_callback_ = callback; }
    
    // Statistics
    MarketDataStats get_statistics() const;
    void reset_statistics();
    
    // Market simulation (for testing)
    void enable_simulation(bool enabled = true);
    void set_base_price(const Symbol& symbol, Price price);
    void add_simulation_symbol(const Symbol& symbol, Price base_price);
    void remove_simulation_symbol(const Symbol& symbol);
    
    // Utility functions
    bool is_market_open() const;
    bool is_symbol_active(const Symbol& symbol) const;
    double get_symbol_volatility(const Symbol& symbol) const;
    
private:
    // Internal processing
    void start_simulation();
    void process_market_data();
    void simulate_market_data();
    void process_tick(const MarketTick& tick);
    
    // Data management
    void store_tick(const MarketTick& tick);
    void update_statistics();
    void cleanup_old_data();
    
    // Simulation helpers
    MarketTick generate_simulated_tick(const Symbol& symbol);
    Price apply_market_movement(Price base_price, double volatility);
    Volume generate_random_volume();
    
    // Notification helpers
    void notify_tick(const MarketTick& tick);
    void notify_quote(const Symbol& symbol, Price bid, Price ask);
    void notify_trade(const Symbol& symbol, Price price, Volume volume);
    
    // Threading utilities
    void ensure_subscription_exists(const Symbol& symbol);
};

// Market data feed interface (for future external feeds)
class IMarketDataFeed {
public:
    virtual ~IMarketDataFeed() = default;
    
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;
    
    virtual bool subscribe(const Symbol& symbol) = 0;
    virtual bool unsubscribe(const Symbol& symbol) = 0;
    
    virtual void set_data_handler(std::function<void(const MarketTick&)> handler) = 0;
};

// Simple simulated feed implementation
class SimulatedMarketFeed : public IMarketDataFeed {
private:
    std::atomic<bool> connected_{false};
    std::function<void(const MarketTick&)> data_handler_;
    std::vector<Symbol> subscribed_symbols_;
    std::thread simulation_thread_;
    std::atomic<bool> running_{false};
    
public:
    SimulatedMarketFeed() = default;
    ~SimulatedMarketFeed() { disconnect(); }
    
    bool connect() override;
    void disconnect() override;
    bool is_connected() const override { return connected_; }
    
    bool subscribe(const Symbol& symbol) override;
    bool unsubscribe(const Symbol& symbol) override;
    
    void set_data_handler(std::function<void(const MarketTick&)> handler) override {
        data_handler_ = handler;
    }
    
private:
    void simulate_data();
};

} // namespace trading
} // namespace vfx