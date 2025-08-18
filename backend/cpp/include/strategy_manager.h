#pragma once

#include "types.h"
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <functional>

namespace vfx {
namespace trading {

// Forward declarations
class OrderManager;
class MarketDataManager;

// Using StrategySignal from types.h, just add missing fields via inheritance
struct ExtendedStrategySignal : public StrategySignal {
    Quantity suggested_quantity;
    Price suggested_price;
    std::string signal_type;  // "ENTRY", "EXIT", "SCALE_IN", "SCALE_OUT"
    
    ExtendedStrategySignal() : StrategySignal() {}
};

class Strategy {
public:
    std::string name;
    bool active;
    double performance;  // Percentage return
    uint64_t trades_executed;
    uint64_t winning_trades;
    double total_pnl;
    
    Strategy(const std::string& n) 
        : name(n), active(false), performance(0.0), 
          trades_executed(0), winning_trades(0), total_pnl(0.0) {}
    
    virtual ~Strategy() = default;
    
    // Override this to implement strategy logic
    virtual std::vector<ExtendedStrategySignal> evaluate(const MarketTick& tick) = 0;
};

class StrategyManager {
private:
    std::unordered_map<std::string, std::unique_ptr<Strategy>> strategies_;
    std::shared_ptr<OrderManager> order_manager_;
    std::shared_ptr<MarketDataManager> market_data_;
    
    std::atomic<bool> running_{false};
    std::thread processing_thread_;
    mutable std::mutex strategies_mutex_;
    
    // Callbacks
    using SignalCallback = std::function<void(const ExtendedStrategySignal&)>;
    std::vector<SignalCallback> signal_callbacks_;
    
public:
    StrategyManager(std::shared_ptr<OrderManager> om, 
                   std::shared_ptr<MarketDataManager> md);
    ~StrategyManager();
    
    // Strategy management
    void register_strategy(std::unique_ptr<Strategy> strategy);
    void activate_strategy(const std::string& name);
    void deactivate_strategy(const std::string& name);
    void remove_strategy(const std::string& name);
    
    // Control
    void start();
    void stop();
    
    // Status
    std::vector<std::string> get_strategy_names() const;
    bool is_strategy_active(const std::string& name) const;
    double get_strategy_performance(const std::string& name) const;
    
    // Callbacks
    void on_signal(SignalCallback callback);
    
private:
    void process_strategies();
    void execute_signal(const ExtendedStrategySignal& signal);
    void update_performance(const std::string& strategy_name, const Execution& exec);
};

// Built-in strategies
class MomentumStrategy : public Strategy {
private:
    double momentum_threshold_;
    std::unordered_map<Symbol, std::vector<Price>> price_history_;
    
public:
    MomentumStrategy();
    std::vector<ExtendedStrategySignal> evaluate(const MarketTick& tick) override;
};

class MeanReversionStrategy : public Strategy {
private:
    double deviation_threshold_;
    std::unordered_map<Symbol, double> moving_averages_;
    
public:
    MeanReversionStrategy();
    std::vector<ExtendedStrategySignal> evaluate(const MarketTick& tick) override;
};

class ArbitrageStrategy : public Strategy {
private:
    double min_spread_;
    
public:
    ArbitrageStrategy();
    std::vector<ExtendedStrategySignal> evaluate(const MarketTick& tick) override;
};

} // namespace trading
} // namespace vfx