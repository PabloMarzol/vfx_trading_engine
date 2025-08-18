#include "strategy_manager.h"
#include "order_manager.h"
#include "market_data.h"
#include <iostream>
#include <algorithm>
#include <numeric>
#include <cmath>

namespace vfx {
namespace trading {

StrategyManager::StrategyManager(std::shared_ptr<OrderManager> om, 
                               std::shared_ptr<MarketDataManager> md)
    : order_manager_(om), market_data_(md) {
    
    // Register built-in strategies
    register_strategy(std::make_unique<MomentumStrategy>());
    register_strategy(std::make_unique<MeanReversionStrategy>());
    register_strategy(std::make_unique<ArbitrageStrategy>());
    
    std::cout << "📊 Strategy Manager initialized with 3 built-in strategies" << std::endl;
}

StrategyManager::~StrategyManager() {
    stop();
}

void StrategyManager::register_strategy(std::unique_ptr<Strategy> strategy) {
    std::lock_guard<std::mutex> lock(strategies_mutex_);
    std::string name = strategy->name;
    strategies_[name] = std::move(strategy);
    std::cout << "✅ Strategy registered: " << name << std::endl;
}

void StrategyManager::activate_strategy(const std::string& name) {
    std::lock_guard<std::mutex> lock(strategies_mutex_);
    auto it = strategies_.find(name);
    if (it != strategies_.end()) {
        it->second->active = true;
        std::cout << "🚀 Strategy activated: " << name << std::endl;
    }
}

void StrategyManager::deactivate_strategy(const std::string& name) {
    std::lock_guard<std::mutex> lock(strategies_mutex_);
    auto it = strategies_.find(name);
    if (it != strategies_.end()) {
        it->second->active = false;
        std::cout << "⏸️ Strategy deactivated: " << name << std::endl;
    }
}

void StrategyManager::remove_strategy(const std::string& name) {
    std::lock_guard<std::mutex> lock(strategies_mutex_);
    strategies_.erase(name);
    std::cout << "🗑️ Strategy removed: " << name << std::endl;
}

void StrategyManager::start() {
    if (running_) return;
    
    running_ = true;
    processing_thread_ = std::thread(&StrategyManager::process_strategies, this);
    std::cout << "✅ Strategy Manager started" << std::endl;
}

void StrategyManager::stop() {
    if (!running_) return;
    
    running_ = false;
    if (processing_thread_.joinable()) {
        processing_thread_.join();
    }
    std::cout << "⏹️ Strategy Manager stopped" << std::endl;
}

std::vector<std::string> StrategyManager::get_strategy_names() const {
    std::lock_guard<std::mutex> lock(strategies_mutex_);
    std::vector<std::string> names;
    for (const auto& [name, strategy] : strategies_) {
        names.push_back(name);
    }
    return names;
}

bool StrategyManager::is_strategy_active(const std::string& name) const {
    std::lock_guard<std::mutex> lock(strategies_mutex_);
    auto it = strategies_.find(name);
    return (it != strategies_.end()) && it->second->active;
}

double StrategyManager::get_strategy_performance(const std::string& name) const {
    std::lock_guard<std::mutex> lock(strategies_mutex_);
    auto it = strategies_.find(name);
    if (it != strategies_.end()) {
        return it->second->performance;
    }
    return 0.0;
}

void StrategyManager::on_signal(SignalCallback callback) {
    signal_callbacks_.push_back(callback);
}

void StrategyManager::process_strategies() {
    while (running_) {
        try {
            // Get latest market data
            if (market_data_) {
                auto symbols = market_data_->get_subscribed_symbols();
                
                for (const auto& symbol : symbols) {
                    auto tick_ptr = market_data_->get_latest_tick(symbol);
                    if (tick_ptr && !tick_ptr->symbol.empty()) {
                        // Evaluate each active strategy
                        std::lock_guard<std::mutex> lock(strategies_mutex_);
                        for (auto& [name, strategy] : strategies_) {
                            if (strategy->active) {
                                auto signals = strategy->evaluate(*tick_ptr);  // Dereference shared_ptr
                                for (const auto& signal : signals) {
                                    execute_signal(signal);
                                    
                                    // Notify callbacks
                                    for (const auto& callback : signal_callbacks_) {
                                        callback(signal);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } catch (const std::exception& e) {
            std::cerr << "Strategy processing error: " << e.what() << std::endl;
        }
    }
}

void StrategyManager::execute_signal(const ExtendedStrategySignal& signal) {
    if (!order_manager_) return;
    
    // Convert signal to order
    if (signal.confidence > 0.7) {  // Only execute high confidence signals
        ClientId client_id = 1;  // Default client
        OrderType order_type = (signal.suggested_price > 0) ? OrderType::LIMIT : OrderType::MARKET;
        
        auto order_id = order_manager_->submit_order(
            client_id,
            signal.symbol,
            signal.side,
            order_type,
            signal.suggested_quantity,
            signal.suggested_price
        );
        
        if (order_id > 0) {
            std::cout << "📈 Strategy " << signal.strategy_name 
                     << " executed: " << signal.signal_type 
                     << " " << signal.symbol << std::endl;
        }
    }
}

void StrategyManager::update_performance(const std::string& strategy_name, const Execution& exec) {
    std::lock_guard<std::mutex> lock(strategies_mutex_);
    auto it = strategies_.find(strategy_name);
    if (it != strategies_.end()) {
        it->second->trades_executed++;
        // Simplified P&L calculation
        double pnl = (exec.side == Side::BUY) ? -exec.price * exec.quantity : exec.price * exec.quantity;
        it->second->total_pnl += pnl;
        if (pnl > 0) it->second->winning_trades++;
        it->second->performance = (it->second->total_pnl / 1000000.0) * 100; // Percentage return on 1M base
    }
}

// Momentum Strategy Implementation
MomentumStrategy::MomentumStrategy() 
    : Strategy("MOMENTUM_SCALPER"), momentum_threshold_(0.002) {  // 0.2% threshold
}

std::vector<ExtendedStrategySignal> MomentumStrategy::evaluate(const MarketTick& tick) {
    std::vector<ExtendedStrategySignal> signals;
    
    // Store price history
    price_history_[tick.symbol].push_back(tick.last_price);
    if (price_history_[tick.symbol].size() > 20) {
        price_history_[tick.symbol].erase(price_history_[tick.symbol].begin());
    }
    
    // Need at least 10 prices for momentum calculation
    if (price_history_[tick.symbol].size() >= 10) {
        auto& prices = price_history_[tick.symbol];
        double recent_avg = std::accumulate(prices.end() - 5, prices.end(), 0.0) / 5.0;
        double older_avg = std::accumulate(prices.begin(), prices.begin() + 5, 0.0) / 5.0;
        
        double momentum = (recent_avg - older_avg) / older_avg;
        
        if (std::abs(momentum) > momentum_threshold_) {
            ExtendedStrategySignal signal;
            signal.strategy_name = name;
            signal.symbol = tick.symbol;
            signal.side = (momentum > 0) ? Side::BUY : Side::SELL;
            signal.confidence = std::min(std::abs(momentum) / momentum_threshold_, 1.0);
            signal.suggested_quantity = 100.0 * signal.confidence;
            signal.suggested_price = 0;  // Market order
            signal.signal_type = "ENTRY";
            signal.timestamp = std::chrono::high_resolution_clock::now();
            
            signals.push_back(signal);
        }
    }
    
    return signals;
}

// Mean Reversion Strategy Implementation
MeanReversionStrategy::MeanReversionStrategy()
    : Strategy("MEAN_REVERSION"), deviation_threshold_(0.02) {  // 2% deviation
}

std::vector<ExtendedStrategySignal> MeanReversionStrategy::evaluate(const MarketTick& tick) {
    std::vector<ExtendedStrategySignal> signals;
    
    // Update moving average
    if (moving_averages_.find(tick.symbol) == moving_averages_.end()) {
        moving_averages_[tick.symbol] = tick.last_price;
    } else {
        // Exponential moving average
        moving_averages_[tick.symbol] = 0.95 * moving_averages_[tick.symbol] + 0.05 * tick.last_price;
    }
    
    double ma = moving_averages_[tick.symbol];
    double deviation = (tick.last_price - ma) / ma;
    
    if (std::abs(deviation) > deviation_threshold_) {
        ExtendedStrategySignal signal;
        signal.strategy_name = name;
        signal.symbol = tick.symbol;
        signal.side = (deviation < 0) ? Side::BUY : Side::SELL;  // Buy when below MA, sell when above
        signal.confidence = std::min(std::abs(deviation) / deviation_threshold_, 1.0) * 0.8;
        signal.suggested_quantity = 50.0 * signal.confidence;
        signal.suggested_price = ma;  // Limit order at MA
        signal.signal_type = "ENTRY";
        signal.timestamp = std::chrono::high_resolution_clock::now();
        
        signals.push_back(signal);
    }
    
    return signals;
}

// Arbitrage Strategy Implementation
ArbitrageStrategy::ArbitrageStrategy()
    : Strategy("ARBITRAGE_HUNTER"), min_spread_(0.0001) {  // 0.01% minimum spread
}

std::vector<ExtendedStrategySignal> ArbitrageStrategy::evaluate(const MarketTick& tick) {
    std::vector<ExtendedStrategySignal> signals;
    
    // Check bid-ask spread for arbitrage opportunity
    double spread = (tick.ask_price - tick.bid_price) / tick.bid_price;
    
    if (spread > min_spread_) {
        // Potential arbitrage opportunity
        ExtendedStrategySignal buy_signal;
        buy_signal.strategy_name = name;
        buy_signal.symbol = tick.symbol;
        buy_signal.side = Side::BUY;
        buy_signal.confidence = std::min(spread / min_spread_, 1.0) * 0.9;
        buy_signal.suggested_quantity = 200.0 * buy_signal.confidence;
        buy_signal.suggested_price = tick.bid_price;
        buy_signal.signal_type = "ENTRY";
        buy_signal.timestamp = std::chrono::high_resolution_clock::now();
        
        signals.push_back(buy_signal);
        
        // Corresponding sell signal
        ExtendedStrategySignal sell_signal = buy_signal;
        sell_signal.side = Side::SELL;
        sell_signal.suggested_price = tick.ask_price;
        
        signals.push_back(sell_signal);
    }
    
    return signals;
}

} // namespace trading
} // namespace vfx