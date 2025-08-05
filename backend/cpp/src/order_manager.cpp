#include "order_manager.h"
#include "market_data.h"
#include "risk_manager.h"
#include <iostream>
#include <algorithm>
#include <random>

namespace vfx {
namespace trading {

// OrderBook implementation
void OrderBook::add_order(std::shared_ptr<Order> order) {
    std::unique_lock<std::shared_mutex> lock(mutex);
    
    if (order->side == Side::BUY) {
        // Buy orders: higher prices first (reverse order)
        buy_orders.emplace(order->price, order);
    } else {
        // Sell orders: lower prices first (normal order)
        sell_orders.emplace(order->price, order);
    }
}

void OrderBook::remove_order(std::shared_ptr<Order> order) {
    std::unique_lock<std::shared_mutex> lock(mutex);
    
    auto& orders = (order->side == Side::BUY) ? buy_orders : sell_orders;
    
    auto range = orders.equal_range(order->price);
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second->id == order->id) {
            orders.erase(it);
            break;
        }
    }
}

std::vector<std::shared_ptr<Order>> OrderBook::get_orders_at_price(Side side, Price price) const {
    std::shared_lock<std::shared_mutex> lock(mutex);
    
    std::vector<std::shared_ptr<Order>> result;
    const auto& orders = (side == Side::BUY) ? buy_orders : sell_orders;
    
    auto range = orders.equal_range(price);
    for (auto it = range.first; it != range.second; ++it) {
        result.push_back(it->second);
    }
    
    return result;
}

Price OrderBook::get_best_bid() const {
    std::shared_lock<std::shared_mutex> lock(mutex);
    return buy_orders.empty() ? 0.0 : buy_orders.rbegin()->first;
}

Price OrderBook::get_best_ask() const {
    std::shared_lock<std::shared_mutex> lock(mutex);
    return sell_orders.empty() ? 0.0 : sell_orders.begin()->first;
}

void OrderBook::clear() {
    std::unique_lock<std::shared_mutex> lock(mutex);
    buy_orders.clear();
    sell_orders.clear();
}

// OrderManager implementation
OrderManager::OrderManager(std::shared_ptr<MarketDataManager> market_data,
                          std::shared_ptr<RiskManager> risk_manager)
    : market_data_(market_data), risk_manager_(risk_manager) {
    
    std::cout << "🏗️ OrderManager initialized" << std::endl;
}

OrderManager::~OrderManager() {
    stop();
    std::cout << "🏗️ OrderManager destroyed" << std::endl;
}

void OrderManager::start() {
    if (running_) return;
    
    running_ = true;
    processing_thread_ = std::thread(&OrderManager::process_orders, this);
    
    std::cout << "▶️ OrderManager started" << std::endl;
}

void OrderManager::stop() {
    if (!running_) return;
    
    running_ = false;
    pending_cv_.notify_all();
    
    if (processing_thread_.joinable()) {
        processing_thread_.join();
    }
    
    std::cout << "⏹️ OrderManager stopped" << std::endl;
}

OrderId OrderManager::submit_order(ClientId client_id, const Symbol& symbol, 
                                  Side side, OrderType type, Quantity quantity, 
                                  Price price, StrategyId strategy_id, TimeInForce tif) {
    
    auto order = std::make_shared<Order>(next_order_id_++, client_id, symbol, 
                                        side, type, quantity, price);
    order->strategy_id = strategy_id;
    order->time_in_force = tif;
    
    return submit_order(order);
}

OrderId OrderManager::submit_order(std::shared_ptr<Order> order) {
    // Validate order
    std::string error_reason;
    if (!validate_order(*order, error_reason)) {
        order->status = OrderStatus::REJECTED;
        notify_rejection(*order, error_reason);
        std::cout << "❌ Order rejected: " << error_reason << std::endl;
        return 0;
    }
    
    // Store order
    {
        std::unique_lock<std::shared_mutex> lock(orders_mutex_);
        orders_[order->id] = order;
        client_orders_[order->client_id].push_back(order->id);
        
        if (order->strategy_id > 0) {
            strategy_orders_[order->strategy_id].push_back(order->id);
        }
    }
    
    // Add to processing queue
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending_orders_.push(order);
    }
    pending_cv_.notify_one();
    
    std::cout << "📝 Order " << order->id << " submitted: " 
              << to_string(order->side) << " " << order->quantity 
              << " " << order->symbol << " @ " << order->price << std::endl;
    
    total_orders_processed_++;
    return order->id;
}

bool OrderManager::cancel_order(OrderId order_id) {
    std::shared_lock<std::shared_mutex> lock(orders_mutex_);
    
    auto it = orders_.find(order_id);
    if (it == orders_.end()) {
        return false;
    }
    
    auto order = it->second;
    if (!order->can_cancel()) {
        return false;
    }
    
    // Remove from order book
    remove_order_from_book(order);
    
    // Update status
    update_order_status(order, OrderStatus::CANCELLED);
    
    std::cout << "❌ Order " << order_id << " cancelled" << std::endl;
    return true;
}

bool OrderManager::cancel_all_orders(ClientId client_id) {
    std::shared_lock<std::shared_mutex> lock(orders_mutex_);
    
    auto client_it = client_orders_.find(client_id);
    if (client_it == client_orders_.end()) {
        return false;
    }
    
    uint32_t cancelled_count = 0;
    for (OrderId order_id : client_it->second) {
        auto order_it = orders_.find(order_id);
        if (order_it != orders_.end() && order_it->second->can_cancel()) {
            remove_order_from_book(order_it->second);
            update_order_status(order_it->second, OrderStatus::CANCELLED);
            cancelled_count++;
        }
    }
    
    std::cout << "❌ Cancelled " << cancelled_count << " orders for client " << client_id << std::endl;
    return cancelled_count > 0;
}

bool OrderManager::cancel_orders_for_symbol(const Symbol& symbol) {
    std::shared_lock<std::shared_mutex> lock(orders_mutex_);
    
    uint32_t cancelled_count = 0;
    for (const auto& [order_id, order] : orders_) {
        if (order->symbol == symbol && order->can_cancel()) {
            remove_order_from_book(order);
            update_order_status(order, OrderStatus::CANCELLED);
            cancelled_count++;
        }
    }
    
    std::cout << "❌ Cancelled " << cancelled_count << " orders for " << symbol << std::endl;
    return cancelled_count > 0;
}

bool OrderManager::cancel_orders_for_strategy(StrategyId strategy_id) {
    std::shared_lock<std::shared_mutex> lock(orders_mutex_);
    
    auto strategy_it = strategy_orders_.find(strategy_id);
    if (strategy_it == strategy_orders_.end()) {
        return false;
    }
    
    uint32_t cancelled_count = 0;
    for (OrderId order_id : strategy_it->second) {
        auto order_it = orders_.find(order_id);
        if (order_it != orders_.end() && order_it->second->can_cancel()) {
            remove_order_from_book(order_it->second);
            update_order_status(order_it->second, OrderStatus::CANCELLED);
            cancelled_count++;
        }
    }
    
    std::cout << "❌ Cancelled " << cancelled_count << " orders for strategy " << strategy_id << std::endl;
    return cancelled_count > 0;
}

std::shared_ptr<Order> OrderManager::get_order(OrderId order_id) const {
    std::shared_lock<std::shared_mutex> lock(orders_mutex_);
    
    auto it = orders_.find(order_id);
    return (it != orders_.end()) ? it->second : nullptr;
}

std::vector<std::shared_ptr<Order>> OrderManager::get_active_orders() const {
    std::shared_lock<std::shared_mutex> lock(orders_mutex_);
    
    std::vector<std::shared_ptr<Order>> active_orders;
    for (const auto& [order_id, order] : orders_) {
        if (!order->is_complete()) {
            active_orders.push_back(order);
        }
    }
    
    return active_orders;
}

void OrderManager::process_orders() {
    std::cout << "🔄 Order processing thread started" << std::endl;
    
    while (running_) {
        std::unique_lock<std::mutex> lock(pending_mutex_);
        
        // Wait for orders or shutdown signal
        pending_cv_.wait(lock, [this] { 
            return !pending_orders_.empty() || !running_; 
        });
        
        // Process all pending orders
        while (!pending_orders_.empty() && running_) {
            auto order = pending_orders_.front();
            pending_orders_.pop();
            lock.unlock();
            
            process_order(order);
            
            lock.lock();
        }
    }
    
    std::cout << "🔄 Order processing thread stopped" << std::endl;
}

void OrderManager::process_order(std::shared_ptr<Order> order) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // Check risk limits
        if (risk_manager_ && !check_risk_limits(*order)) {
            update_order_status(order, OrderStatus::REJECTED);
            notify_rejection(*order, "Risk limits exceeded");
            return;
        }
        
        // Process based on order type
        switch (order->type) {
            case OrderType::MARKET:
                execute_market_order(order);
                break;
                
            case OrderType::LIMIT:
                execute_limit_order(order);
                break;
                
            case OrderType::STOP:
            case OrderType::STOP_LIMIT:
                // Add to order book for monitoring
                add_order_to_book(order);
                break;
                
            default:
                update_order_status(order, OrderStatus::REJECTED);
                notify_rejection(*order, "Unsupported order type");
                return;
        }
        
        notify_order_update(*order);
        
    } catch (const std::exception& e) {
        std::cout << "❌ Error processing order " << order->id << ": " << e.what() << std::endl;
        update_order_status(order, OrderStatus::REJECTED);
        notify_rejection(*order, e.what());
    }
    
    // Performance tracking
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    // Update average processing time (simplified)
}

bool OrderManager::validate_order(const Order& order, std::string& error_reason) const {
    // Basic validation
    if (order.quantity <= 0) {
        error_reason = "Invalid quantity";
        return false;
    }
    
    if (order.symbol.empty()) {
        error_reason = "Invalid symbol";
        return false;
    }
    
    if (order.type == OrderType::LIMIT && order.price <= 0) {
        error_reason = "Invalid price for limit order";
        return false;
    }
    
    if (order.quantity > config_.max_position_size) {
        error_reason = "Quantity exceeds maximum position size";
        return false;
    }
    
    return true;
}

bool OrderManager::check_risk_limits(const Order& order) const {
    if (!config_.enable_risk_checks || !risk_manager_) {
        return true;
    }
    
    // Delegate to risk manager
    return risk_manager_->check_order_risk(order);
}

void OrderManager::execute_market_order(std::shared_ptr<Order> order) {
    // Get current market price
    Price execution_price = get_market_price(order->symbol, order->side);
    
    if (execution_price <= 0) {
        update_order_status(order, OrderStatus::REJECTED);
        notify_rejection(*order, "No market price available");
        return;
    }
    
    // Add some realistic slippage
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> slippage(-0.001, 0.001); // ±0.1%
    execution_price *= (1.0 + slippage(gen));
    
    // Execute full quantity immediately
    create_execution(order, order->quantity, execution_price);
    update_order_status(order, OrderStatus::FILLED);
    
    std::cout << "✅ Market order " << order->id << " executed: " 
              << order->quantity << " @ $" << execution_price << std::endl;
}

void OrderManager::execute_limit_order(std::shared_ptr<Order> order) {
    // Try to match immediately
    if (try_match_order(order)) {
        if (order->remaining_quantity() <= 0) {
            update_order_status(order, OrderStatus::FILLED);
        } else {
            update_order_status(order, OrderStatus::PARTIALLY_FILLED);
            // Add remaining quantity to order book
            add_order_to_book(order);
        }
    } else {
        // No immediate match, add to order book
        add_order_to_book(order);
        update_order_status(order, OrderStatus::PENDING);
    }
}

bool OrderManager::try_match_order(std::shared_ptr<Order> order) {
    auto* book = get_or_create_order_book(order->symbol);
    if (!book) return false;
    
    std::unique_lock<std::shared_mutex> book_lock(book->mutex);
    
    bool matched = false;
    auto& opposite_orders = (order->side == Side::BUY) ? book->sell_orders : book->buy_orders;
    
    auto it = opposite_orders.begin();
    while (it != opposite_orders.end() && order->remaining_quantity() > 0) {
        auto& opposite_order = it->second;
        
        // Check if prices cross
        bool can_match = false;
        if (order->side == Side::BUY) {
            can_match = price_greater(order->price, opposite_order->price) || 
                       price_equal(order->price, opposite_order->price);
        } else {
            can_match = price_less(order->price, opposite_order->price) || 
                       price_equal(order->price, opposite_order->price);
        }
        
        if (!can_match) break;
        
        // Calculate trade quantity
        Quantity trade_qty = std::min(order->remaining_quantity(), 
                                     opposite_order->remaining_quantity());
        Price trade_price = opposite_order->price; // Price improvement for aggressor
        
        // Create executions for both orders
        create_execution(order, trade_qty, trade_price);
        create_execution(opposite_order, trade_qty, trade_price);
        
        // Update order statuses
        if (opposite_order->remaining_quantity() <= 0) {
            update_order_status(opposite_order, OrderStatus::FILLED);
            it = opposite_orders.erase(it);
        } else {
            update_order_status(opposite_order, OrderStatus::PARTIALLY_FILLED);
            ++it;
        }
        
        matched = true;
        
        std::cout << "🤝 Orders matched: " << trade_qty << " @ $" << trade_price << std::endl;
    }
    
    return matched;
}

OrderBook* OrderManager::get_or_create_order_book(const Symbol& symbol) {
    std::unique_lock<std::shared_mutex> lock(books_mutex_);
    
    auto it = order_books_.find(symbol);
    if (it == order_books_.end()) {
        order_books_[symbol] = std::make_unique<OrderBook>(symbol);
        std::cout << "📚 Created order book for " << symbol << std::endl;
    }
    
    return order_books_[symbol].get();
}

void OrderManager::add_order_to_book(std::shared_ptr<Order> order) {
    auto* book = get_or_create_order_book(order->symbol);
    if (book) {
        book->add_order(order);
    }
}

void OrderManager::remove_order_from_book(std::shared_ptr<Order> order) {
    std::shared_lock<std::shared_mutex> lock(books_mutex_);
    
    auto it = order_books_.find(order->symbol);
    if (it != order_books_.end()) {
        it->second->remove_order(order);
    }
}

void OrderManager::create_execution(std::shared_ptr<Order> order, Quantity quantity, Price price) {
    Execution execution(next_execution_id_++, order->id, order->client_id, 
                       order->symbol, order->side, quantity, price);
    
    // Update order
    order->filled_quantity += quantity;
    order->updated_at = std::chrono::high_resolution_clock::now();
    
    // Store execution
    {
        std::unique_lock<std::shared_mutex> lock(executions_mutex_);
        executions_.push_back(execution);
        
        // Limit execution history
        if (executions_.size() > 10000) {
            executions_.erase(executions_.begin(), executions_.begin() + 5000);
        }
    }
    
    total_executions_++;
    notify_execution(execution);
}

void OrderManager::update_order_status(std::shared_ptr<Order> order, OrderStatus new_status) {
    order->status = new_status;
    order->updated_at = std::chrono::high_resolution_clock::now();
}

void OrderManager::notify_order_update(const Order& order) {
    if (order_update_callback_) {
        try {
            order_update_callback_(order);
        } catch (const std::exception& e) {
            std::cout << "❌ Error in order update callback: " << e.what() << std::endl;
        }
    }
}

void OrderManager::notify_execution(const Execution& execution) {
    if (execution_callback_) {
        try {
            execution_callback_(execution);
        } catch (const std::exception& e) {
            std::cout << "❌ Error in execution callback: " << e.what() << std::endl;
        }
    }
}

void OrderManager::notify_rejection(const Order& order, const std::string& reason) {
    if (rejection_callback_) {
        try {
            rejection_callback_(order, reason);
        } catch (const std::exception& e) {
            std::cout << "❌ Error in rejection callback: " << e.what() << std::endl;
        }
    }
}

Price OrderManager::get_market_price(const Symbol& symbol, Side side) const {
    if (!market_data_) {
        // Fallback to simple prices for testing
        static std::unordered_map<Symbol, Price> test_prices = {
            {"BTC/USD", 68450.0},
            {"ETH/USD", 3892.0},
            {"AAPL", 175.0},
            {"TSLA", 245.0},
            {"NVDA", 875.0}
        };
        
        auto it = test_prices.find(symbol);
        return (it != test_prices.end()) ? it->second : 100.0;
    }
    
    // Get from market data manager
    auto tick = market_data_->get_latest_tick(symbol);
    if (!tick) return 0.0;
    
    return (side == Side::BUY) ? tick->ask : tick->bid;
}

const OrderBook* OrderManager::get_order_book(const Symbol& symbol) const {
    std::shared_lock<std::shared_mutex> lock(books_mutex_);
    
    auto it = order_books_.find(symbol);
    return (it != order_books_.end()) ? it->second.get() : nullptr;
}

Price OrderManager::get_best_bid(const Symbol& symbol) const {
    const auto* book = get_order_book(symbol);
    return book ? book->get_best_bid() : 0.0;
}

Price OrderManager::get_best_ask(const Symbol& symbol) const {
    const auto* book = get_order_book(symbol);
    return book ? book->get_best_ask() : 0.0;
}

std::vector<Execution> OrderManager::get_recent_executions(uint32_t count) const {
    std::shared_lock<std::shared_mutex> lock(executions_mutex_);
    
    std::vector<Execution> recent;
    uint32_t start_idx = (executions_.size() > count) ? executions_.size() - count : 0;
    
    for (uint32_t i = start_idx; i < executions_.size(); ++i) {
        recent.push_back(executions_[i]);
    }
    
    return recent;
}

OrderManager::Statistics OrderManager::get_statistics() const {
    Statistics stats;
    stats.total_orders = total_orders_processed_;
    stats.total_executions = total_executions_;
    stats.orders_per_second = orders_per_second_;
    
    {
        std::shared_lock<std::shared_mutex> lock(orders_mutex_);
        stats.active_orders = 0;
        for (const auto& [order_id, order] : orders_) {
            if (!order->is_complete()) {
                stats.active_orders++;
            }
        }
    }
    
    {
        std::shared_lock<std::shared_mutex> lock(books_mutex_);
        stats.active_symbols = order_books_.size();
    }
    
    stats.avg_processing_time_us = 50.0; // Placeholder
    
    return stats;
}

void OrderManager::emergency_stop_all_trading() {
    std::cout << "🚨 EMERGENCY STOP - Cancelling all orders" << std::endl;
    
    std::shared_lock<std::shared_mutex> lock(orders_mutex_);
    
    uint32_t cancelled_count = 0;
    for (const auto& [order_id, order] : orders_) {
        if (order->can_cancel()) {
            remove_order_from_book(order);
            update_order_status(order, OrderStatus::CANCELLED);
            cancelled_count++;
        }
    }
    
    std::cout << "🚨 Emergency stop completed - " << cancelled_count << " orders cancelled" << std::endl;
}

} // namespace trading
} // namespace vfx