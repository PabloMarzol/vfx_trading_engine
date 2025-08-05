#pragma once

#include "types.h"
#include <memory>
#include <unordered_map>
#include <map>
#include <vector>
#include <queue>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <thread>
#include <string>


namespace vfx {
namespace trading {

// Forward declarations
class MarketDataManager;
class RiskManager;

// Order book structure for internal matching
struct OrderBook {
    Symbol symbol;
    std::multimap<Price, std::shared_ptr<Order>> buy_orders;   // Price descending
    std::multimap<Price, std::shared_ptr<Order>> sell_orders;  // Price ascending
    mutable std::shared_mutex mutex;
    
    OrderBook(const Symbol& sym) : symbol(sym) {}
    
    void add_order(std::shared_ptr<Order> order);
    void remove_order(std::shared_ptr<Order> order);
    std::vector<std::shared_ptr<Order>> get_orders_at_price(Side side, Price price) const;
    Price get_best_bid() const;
    Price get_best_ask() const;
    void clear();
};

// Execution callback types
using OrderCallback = std::function<void(const Order&)>;
using ExecutionCallback = std::function<void(const Execution&)>;
using RejectCallback = std::function<void(const Order&, const std::string& reason)>;

class OrderManager {
private:
    // Core data structures
    std::unordered_map<OrderId, std::shared_ptr<Order>> orders_;
    std::unordered_map<Symbol, std::unique_ptr<OrderBook>> order_books_;
    std::unordered_map<ClientId, std::vector<OrderId>> client_orders_;
    std::unordered_map<StrategyId, std::vector<OrderId>> strategy_orders_;
    
    // Thread safety
    mutable std::shared_mutex orders_mutex_;
    mutable std::shared_mutex books_mutex_;
    
    // Order processing
    std::queue<std::shared_ptr<Order>> pending_orders_;
    std::mutex pending_mutex_;
    std::condition_variable pending_cv_;
    
    // Execution tracking
    std::vector<Execution> executions_;
    mutable std::shared_mutex executions_mutex_;
    
    // ID generation
    std::atomic<OrderId> next_order_id_{1000};
    std::atomic<uint64_t> next_execution_id_{1};
    
    // Processing thread
    std::thread processing_thread_;
    std::atomic<bool> running_{false};
    
    // Dependencies
    std::shared_ptr<MarketDataManager> market_data_;
    std::shared_ptr<RiskManager> risk_manager_;
    
    // Configuration
    TradingConfig config_;
    
    // Callbacks
    OrderCallback order_update_callback_;
    ExecutionCallback execution_callback_;
    RejectCallback rejection_callback_;
    
    // Performance metrics
    std::atomic<uint64_t> total_orders_processed_{0};
    std::atomic<uint64_t> total_executions_{0};
    std::atomic<uint64_t> orders_per_second_{0};
    
public:
    OrderManager(std::shared_ptr<MarketDataManager> market_data,
                 std::shared_ptr<RiskManager> risk_manager);
    ~OrderManager();
    
    // Lifecycle
    void start();
    void stop();
    bool is_running() const { return running_; }
    
    // Configuration
    void set_config(const TradingConfig& config) { config_ = config; }
    const TradingConfig& get_config() const { return config_; }
    
    // Order submission
    OrderId submit_order(ClientId client_id, const Symbol& symbol, Side side, 
                        OrderType type, Quantity quantity, Price price = 0.0,
                        StrategyId strategy_id = 0, TimeInForce tif = TimeInForce::GTC);
    
    OrderId submit_order(std::shared_ptr<Order> order);
    
    // Order modification
    bool cancel_order(OrderId order_id);
    bool cancel_all_orders(ClientId client_id);
    bool cancel_orders_for_symbol(const Symbol& symbol);
    bool cancel_orders_for_strategy(StrategyId strategy_id);
    
    bool modify_order(OrderId order_id, Quantity new_quantity, Price new_price);
    
    // Order queries
    std::shared_ptr<Order> get_order(OrderId order_id) const;
    std::vector<std::shared_ptr<Order>> get_orders_for_client(ClientId client_id) const;
    std::vector<std::shared_ptr<Order>> get_orders_for_strategy(StrategyId strategy_id) const;
    std::vector<std::shared_ptr<Order>> get_orders_for_symbol(const Symbol& symbol) const;
    std::vector<std::shared_ptr<Order>> get_active_orders() const;
    
    // Execution queries
    std::vector<Execution> get_executions_for_order(OrderId order_id) const;
    std::vector<Execution> get_executions_for_client(ClientId client_id) const;
    std::vector<Execution> get_recent_executions(uint32_t count = 100) const;
    
    // Order book queries
    const OrderBook* get_order_book(const Symbol& symbol) const;
    Price get_best_bid(const Symbol& symbol) const;
    Price get_best_ask(const Symbol& symbol) const;
    
    // Callbacks
    void set_order_update_callback(OrderCallback callback) { 
        order_update_callback_ = callback; 
    }
    void set_execution_callback(ExecutionCallback callback) { 
        execution_callback_ = callback; 
    }
    void set_rejection_callback(RejectCallback callback) { 
        rejection_callback_ = callback; 
    }
    
    // Statistics
    struct Statistics {
        uint64_t total_orders;
        uint64_t active_orders;
        uint64_t total_executions;
        uint64_t orders_per_second;
        double avg_processing_time_us;
        uint32_t active_symbols;
    };
    
    Statistics get_statistics() const;
    
    // Emergency functions
    void emergency_stop_all_trading();
    void pause_trading_for_symbol(const Symbol& symbol);
    void resume_trading_for_symbol(const Symbol& symbol);
    
private:
    // Internal processing
    void process_orders();
    void process_order(std::shared_ptr<Order> order);
    
    // Order validation
    bool validate_order(const Order& order, std::string& error_reason) const;
    bool check_risk_limits(const Order& order) const;
    
    // Order execution
    void execute_market_order(std::shared_ptr<Order> order);
    void execute_limit_order(std::shared_ptr<Order> order);
    bool try_match_order(std::shared_ptr<Order> order);
    
    // Order book management
    OrderBook* get_or_create_order_book(const Symbol& symbol);
    void add_order_to_book(std::shared_ptr<Order> order);
    void remove_order_from_book(std::shared_ptr<Order> order);
    
    // Execution handling
    void create_execution(std::shared_ptr<Order> order, Quantity quantity, Price price);
    void update_order_status(std::shared_ptr<Order> order, OrderStatus new_status);
    
    // Notifications
    void notify_order_update(const Order& order);
    void notify_execution(const Execution& execution);
    void notify_rejection(const Order& order, const std::string& reason);
    
    // Utilities
    Price get_market_price(const Symbol& symbol, Side side) const;
    bool should_reject_order(const Order& order) const;
    void cleanup_completed_orders();
    
    // Threading utilities
    void ensure_order_book_exists(const Symbol& symbol);
};

} // namespace trading
} // namespace vfx