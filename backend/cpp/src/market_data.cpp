#include "market_data.h"
#include <iostream>
#include <algorithm>
#include <random>
#include <cmath>

namespace vfx {
namespace trading {

MarketDataManager::MarketDataManager() : gen_(rd_()) {
    // Initialize base prices for simulation
    base_prices_ = {
        {"BTC/USD", 68450.0},
        {"ETH/USD", 3892.0},
        {"AAPL", 175.0},
        {"TSLA", 245.0},
        {"NVDA", 875.0},
        {"SPY", 450.0},
        {"QQQ", 385.0},
        {"EURUSD", 1.0845},
        {"GBPUSD", 1.2634},
        {"USDJPY", 150.25}
    };
    
    last_stats_update_ = std::chrono::high_resolution_clock::now();
    std::cout << "📊 MarketDataManager initialized with " << base_prices_.size() << " symbols" << std::endl;
}

MarketDataManager::~MarketDataManager() {
    stop();
    std::cout << "📊 MarketDataManager destroyed" << std::endl;
}

void MarketDataManager::start() {
    if (running_) return;
    
    running_ = true;
    
    // Start processing threads
    data_processing_thread_ = std::thread(&MarketDataManager::process_market_data, this);
    simulation_thread_ = std::thread(&MarketDataManager::simulate_market_data, this);
    
    std::cout << "▶️ MarketDataManager started" << std::endl;
}

void MarketDataManager::stop() {
    if (!running_) return;
    
    running_ = false;
    incoming_cv_.notify_all();
    
    if (data_processing_thread_.joinable()) {
        data_processing_thread_.join();
    }
    
    if (simulation_thread_.joinable()) {
        simulation_thread_.join();
    }
    
    std::cout << "⏹️ MarketDataManager stopped" << std::endl;
}

void MarketDataManager::set_config(const MarketDataConfig& config) {
    config_ = config;
    
    // Auto-subscribe to configured symbols
    for (const auto& symbol : config.subscribed_symbols) {
        subscribe(symbol);
    }
    
    std::cout << "⚙️ MarketData config updated - " << config.subscribed_symbols.size() << " symbols" << std::endl;
}

bool MarketDataManager::subscribe(const Symbol& symbol, bool level1, bool level2, bool trades) {
    std::unique_lock<std::shared_mutex> lock(subscriptions_mutex_);
    
    auto subscription = std::make_shared<SymbolSubscription>(symbol);
    subscription->level1_enabled = level1;
    subscription->level2_enabled = level2;
    subscription->trades_enabled = trades;
    subscription->update_frequency_ms = config_.update_frequency_ms;
    
    subscriptions_[symbol] = subscription;
    
    // Initialize with base price if available
    if (base_prices_.find(symbol) != base_prices_.end()) {
        auto tick = generate_simulated_tick(symbol);
        inject_tick(tick);
    }
    
    std::cout << "📈 Subscribed to " << symbol << " (L1:" << level1 << " L2:" << level2 << " Trades:" << trades << ")" << std::endl;
    return true;
}

bool MarketDataManager::unsubscribe(const Symbol& symbol) {
    std::unique_lock<std::shared_mutex> lock(subscriptions_mutex_);
    
    auto it = subscriptions_.find(symbol);
    if (it == subscriptions_.end()) {
        return false;
    }
    
    subscriptions_.erase(it);
    
    // Clean up data
    {
        std::unique_lock<std::shared_mutex> data_lock(data_mutex_);
        latest_ticks_.erase(symbol);
        tick_history_.erase(symbol);
    }
    
    std::cout << "📉 Unsubscribed from " << symbol << std::endl;
    return true;
}

void MarketDataManager::subscribe_multiple(const std::vector<Symbol>& symbols) {
    for (const auto& symbol : symbols) {
        subscribe(symbol);
    }
}

std::vector<Symbol> MarketDataManager::get_subscribed_symbols() const {
    std::shared_lock<std::shared_mutex> lock(subscriptions_mutex_);
    
    std::vector<Symbol> symbols;
    for (const auto& [symbol, subscription] : subscriptions_) {
        symbols.push_back(symbol);
    }
    
    return symbols;
}

std::shared_ptr<MarketTick> MarketDataManager::get_latest_tick(const Symbol& symbol) const {
    std::shared_lock<std::shared_mutex> lock(data_mutex_);
    
    auto it = latest_ticks_.find(symbol);
    return (it != latest_ticks_.end()) ? it->second : nullptr;
}

std::vector<MarketTick> MarketDataManager::get_tick_history(const Symbol& symbol, uint32_t count) const {
    std::shared_lock<std::shared_mutex> lock(data_mutex_);
    
    std::vector<MarketTick> history;
    auto it = tick_history_.find(symbol);
    if (it == tick_history_.end()) {
        return history;
    }
    
    // Convert queue to vector (most recent first)
    auto queue_copy = it->second;
    while (!queue_copy.empty() && history.size() < count) {
        history.insert(history.begin(), queue_copy.front());
        queue_copy.pop();
    }
    
    return history;
}

Price MarketDataManager::get_last_price(const Symbol& symbol) const {
    auto tick = get_latest_tick(symbol);
    return tick ? tick->last_price : 0.0;
}

Price MarketDataManager::get_bid_price(const Symbol& symbol) const {
    auto tick = get_latest_tick(symbol);
    return tick ? tick->bid : 0.0;
}

Price MarketDataManager::get_ask_price(const Symbol& symbol) const {
    auto tick = get_latest_tick(symbol);
    return tick ? tick->ask : 0.0;
}

void MarketDataManager::inject_tick(const MarketTick& tick) {
    {
        std::lock_guard<std::mutex> lock(incoming_mutex_);
        incoming_ticks_.push(tick);
    }
    incoming_cv_.notify_one();
}

void MarketDataManager::inject_quote(const Symbol& symbol, Price bid, Price ask, Volume bid_size, Volume ask_size) {
    MarketTick tick;
    tick.symbol = symbol;
    tick.bid = bid;
    tick.ask = ask;
    tick.bid_size = bid_size;
    tick.ask_size = ask_size;
    tick.last_price = (bid + ask) / 2.0;
    tick.last_size = (bid_size + ask_size) / 2;
    tick.timestamp = std::chrono::high_resolution_clock::now();
    
    inject_tick(tick);
}

void MarketDataManager::inject_trade(const Symbol& symbol, Price price, Volume volume) {
    auto latest = get_latest_tick(symbol);
    
    MarketTick tick;
    tick.symbol = symbol;
    tick.last_price = price;
    tick.last_size = volume;
    tick.timestamp = std::chrono::high_resolution_clock::now();
    
    if (latest) {
        tick.bid = latest->bid;
        tick.ask = latest->ask;
        tick.bid_size = latest->bid_size;
        tick.ask_size = latest->ask_size;
    } else {
        // Estimate bid/ask from trade price
        double spread = price * 0.001; // 0.1% spread
        tick.bid = price - spread / 2;
        tick.ask = price + spread / 2;
        tick.bid_size = volume;
        tick.ask_size = volume;
    }
    
    inject_tick(tick);
}

void MarketDataManager::process_market_data() {
    std::cout << "🔄 Market data processing thread started" << std::endl;
    
    while (running_) {
        std::unique_lock<std::mutex> lock(incoming_mutex_);
        
        // Wait for data or shutdown signal
        incoming_cv_.wait(lock, [this] { 
            return !incoming_ticks_.empty() || !running_; 
        });
        
        // Process all incoming ticks
        while (!incoming_ticks_.empty() && running_) {
            auto tick = incoming_ticks_.front();
            incoming_ticks_.pop();
            lock.unlock();
            
            process_tick(tick);
            
            lock.lock();
        }
    }
    
    std::cout << "🔄 Market data processing thread stopped" << std::endl;
}

void MarketDataManager::simulate_market_data() {
    std::cout << "🎲 Market data simulation thread started" << std::endl;
    
    while (running_) {
        // Generate ticks for all subscribed symbols
        auto symbols = get_subscribed_symbols();
        
        for (const auto& symbol : symbols) {
            if (!running_) break;
            
            // Check if we have a base price for simulation
            if (base_prices_.find(symbol) != base_prices_.end()) {
                auto tick = generate_simulated_tick(symbol);
                inject_tick(tick);
            }
        }
        
        // Sleep based on update frequency
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.update_frequency_ms));
    }
    
    std::cout << "🎲 Market data simulation thread stopped" << std::endl;
}

void MarketDataManager::process_tick(const MarketTick& tick) {
    try {
        // Store the tick
        store_tick(tick);
        
        // Update subscription statistics
        {
            std::shared_lock<std::shared_mutex> lock(subscriptions_mutex_);
            auto it = subscriptions_.find(tick.symbol);
            if (it != subscriptions_.end()) {
                it->second->tick_count++;
            }
        }
        
        // Notify callbacks
        notify_tick(tick);
        notify_quote(tick.symbol, tick.bid, tick.ask);
        
        if (tick.last_size > 0) {
            notify_trade(tick.symbol, tick.last_price, tick.last_size);
        }
        
        total_ticks_processed_++;
        
    } catch (const std::exception& e) {
        std::cout << "❌ Error processing tick for " << tick.symbol << ": " << e.what() << std::endl;
    }
}

void MarketDataManager::store_tick(const MarketTick& tick) {
    std::unique_lock<std::shared_mutex> lock(data_mutex_);
    
    // Update latest tick
    latest_ticks_[tick.symbol] = std::make_shared<MarketTick>(tick);
    
    // Add to history
    auto& history = tick_history_[tick.symbol];
    history.push(tick);
    
    // Limit history size
    const uint32_t MAX_HISTORY = 1000;
    while (history.size() > MAX_HISTORY) {
        history.pop();
    }
}

MarketTick MarketDataManager::generate_simulated_tick(const Symbol& symbol) {
    auto base_it = base_prices_.find(symbol);
    if (base_it == base_prices_.end()) {
        // Default tick
        return MarketTick(symbol, 100.0, 100.1, 1000, 1000, 100.05, 500);
    }
    
    Price base_price = base_it->second;
    double volatility = get_symbol_volatility(symbol);
    
    // Generate new price with random walk
    Price new_price = apply_market_movement(base_price, volatility);
    
    // Update base price for next iteration
    base_prices_[symbol] = new_price;
    
    // Generate bid/ask spread
    double spread_pct = 0.001; // 0.1% default spread
    if (symbol.find("USD") != std::string::npos) {
        spread_pct = 0.0001; // 0.01% for forex
    } else if (symbol.find("BTC") != std::string::npos || symbol.find("ETH") != std::string::npos) {
        spread_pct = 0.0005; // 0.05% for crypto
    }
    
    double spread = new_price * spread_pct;
    Price bid = new_price - spread / 2;
    Price ask = new_price + spread / 2;
    
    // Generate volumes
    Volume bid_size = generate_random_volume();
    Volume ask_size = generate_random_volume();
    Volume trade_size = generate_random_volume() / 2;
    
    return MarketTick(symbol, bid, ask, bid_size, ask_size, new_price, trade_size);
}

Price MarketDataManager::apply_market_movement(Price base_price, double volatility) {
    // Use geometric Brownian motion for realistic price movement
    std::normal_distribution<double> normal(0.0, 1.0);
    
    double dt = config_.update_frequency_ms / 1000.0; // Convert to seconds
    double drift = 0.0; // No trend
    double random_shock = normal(gen_);
    
    double price_change = base_price * (drift * dt + volatility * sqrt(dt) * random_shock);
    Price new_price = base_price + price_change;
    
    // Ensure price doesn't go negative
    return std::max(new_price, base_price * 0.01);
}

Volume MarketDataManager::generate_random_volume() {
    std::uniform_int_distribution<Volume> volume_dist(100, 10000);
    return volume_dist(gen_);
}

double MarketDataManager::get_symbol_volatility(const Symbol& symbol) const {
    // Different volatilities for different asset classes
    if (symbol.find("BTC") != std::string::npos || symbol.find("ETH") != std::string::npos) {
        return 0.02; // 2% crypto volatility
    } else if (symbol.find("USD") != std::string::npos) {
        return 0.001; // 0.1% forex volatility
    } else if (symbol == "TSLA") {
        return 0.015; // 1.5% high volatility stock
    } else {
        return 0.008; // 0.8% normal stock volatility
    }
}

void MarketDataManager::notify_tick(const MarketTick& tick) {
    if (tick_callback_) {
        try {
            tick_callback_(tick);
        } catch (const std::exception& e) {
            std::cout << "❌ Error in tick callback: " << e.what() << std::endl;
        }
    }
}

void MarketDataManager::notify_quote(const Symbol& symbol, Price bid, Price ask) {
    if (quote_callback_) {
        try {
            quote_callback_(symbol, bid, ask);
        } catch (const std::exception& e) {
            std::cout << "❌ Error in quote callback: " << e.what() << std::endl;
        }
    }
}

void MarketDataManager::notify_trade(const Symbol& symbol, Price price, Volume volume) {
    if (trade_callback_) {
        try {
            trade_callback_(symbol, price, volume);
        } catch (const std::exception& e) {
            std::cout << "❌ Error in trade callback: " << e.what() << std::endl;
        }
    }
}

MarketDataStats MarketDataManager::get_statistics() const {
    MarketDataStats stats;
    stats.total_ticks_received = total_ticks_processed_;
    stats.ticks_per_second = ticks_per_second_;
    stats.last_update = std::chrono::high_resolution_clock::now();
    
    {
        std::shared_lock<std::shared_mutex> lock(subscriptions_mutex_);
        stats.total_symbols = subscriptions_.size();
        
        for (const auto& [symbol, subscription] : subscriptions_) {
            stats.symbol_tick_counts[symbol] = subscription->tick_count;
        }
    }
    
    // Calculate average latency (simplified)
    stats.avg_latency_us = 50.0; // Placeholder
    
    return stats;
}

void MarketDataManager::set_base_price(const Symbol& symbol, Price price) {
    base_prices_[symbol] = price;
    std::cout << "💰 Set base price for " << symbol << ": $" << price << std::endl;
}

void MarketDataManager::add_simulation_symbol(const Symbol& symbol, Price base_price) {
    set_base_price(symbol, base_price);
    subscribe(symbol);
}

bool MarketDataManager::is_market_open() const {
    // Simple implementation - assume always open for now
    // In real implementation, check market hours
    return true;
}

bool MarketDataManager::is_symbol_active(const Symbol& symbol) const {
    std::shared_lock<std::shared_mutex> lock(subscriptions_mutex_);
    return subscriptions_.find(symbol) != subscriptions_.end();
}

// SimulatedMarketFeed implementation
bool SimulatedMarketFeed::connect() {
    if (connected_) return true;
    
    connected_ = true;
    running_ = true;
    simulation_thread_ = std::thread(&SimulatedMarketFeed::simulate_data, this);
    
    std::cout << "🔌 SimulatedMarketFeed connected" << std::endl;
    return true;
}

void SimulatedMarketFeed::disconnect() {
    if (!connected_) return;
    
    connected_ = false;
    running_ = false;
    
    if (simulation_thread_.joinable()) {
        simulation_thread_.join();
    }
    
    std::cout << "🔌 SimulatedMarketFeed disconnected" << std::endl;
}

bool SimulatedMarketFeed::subscribe(const Symbol& symbol) {
    if (!connected_) return false;
    
    auto it = std::find(subscribed_symbols_.begin(), subscribed_symbols_.end(), symbol);
    if (it == subscribed_symbols_.end()) {
        subscribed_symbols_.push_back(symbol);
        std::cout << "📊 SimulatedFeed subscribed to " << symbol << std::endl;
    }
    
    return true;
}

bool SimulatedMarketFeed::unsubscribe(const Symbol& symbol) {
    auto it = std::find(subscribed_symbols_.begin(), subscribed_symbols_.end(), symbol);
    if (it != subscribed_symbols_.end()) {
        subscribed_symbols_.erase(it);
        std::cout << "📊 SimulatedFeed unsubscribed from " << symbol << std::endl;
        return true;
    }
    
    return false;
}

void SimulatedMarketFeed::simulate_data() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> price_change(-0.01, 0.01); // ±1%
    
    std::unordered_map<Symbol, Price> prices = {
        {"BTC/USD", 68450.0},
        {"ETH/USD", 3892.0},
        {"AAPL", 175.0}
    };
    
    while (running_) {
        for (const auto& symbol : subscribed_symbols_) {
            if (!running_) break;
            
            auto& price = prices[symbol];
            price *= (1.0 + price_change(gen));
            
            double spread = price * 0.001;
            MarketTick tick(symbol, price - spread/2, price + spread/2, 
                           1000, 1000, price, 500);
            
            if (data_handler_) {
                data_handler_(tick);
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}


} // namespace trading
} // namespace vfx