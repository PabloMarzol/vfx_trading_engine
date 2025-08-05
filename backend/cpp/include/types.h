#pragma once

#include <cstdint>
#include <string>
#include <chrono>
#include <memory>
#include <vector>
#include <unordered_map>
#include <cmath>

namespace vfx {
namespace trading {

// Type aliases for clarity and performance
using Price = double;
using Quantity = double;
using OrderId = uint64_t;
using ClientId = uint32_t;
using StrategyId = uint32_t;
using Timestamp = std::chrono::high_resolution_clock::time_point;
using Duration = std::chrono::milliseconds;

// Market data types
using Symbol = std::string;
using Volume = uint64_t;
using TickId = uint64_t;

// Enumerations
enum class Side : uint8_t {
    BUY = 0,
    SELL = 1
};

enum class OrderType : uint8_t {
    MARKET = 0,
    LIMIT = 1,
    STOP = 2,
    STOP_LIMIT = 3
};

enum class OrderStatus : uint8_t {
    PENDING = 0,
    PARTIALLY_FILLED = 1,
    FILLED = 2,
    CANCELLED = 3,
    REJECTED = 4,
    EXPIRED = 5
};

enum class TimeInForce : uint8_t {
    GTC = 0,  // Good Till Cancelled
    IOC = 1,  // Immediate Or Cancel
    FOK = 2,  // Fill Or Kill
    DAY = 3   // Day order
};

enum class ExecutionType : uint8_t {
    NEW = 0,
    PARTIAL_FILL = 1,
    FILL = 2,
    CANCELLED = 3,
    REPLACED = 4,
    REJECTED = 5
};

enum class RiskLevel : uint8_t {
    LOW = 0,
    MEDIUM = 1,
    HIGH = 2,
    CRITICAL = 3
};

// Market data tick structure
struct MarketTick {
    Symbol symbol;
    Price bid;
    Price ask;
    Volume bid_size;
    Volume ask_size;
    Price last_price;
    Volume last_size;
    Timestamp timestamp;
    
    MarketTick() = default;
    MarketTick(const Symbol& sym, Price b, Price a, Volume bs, Volume as, 
               Price lp, Volume ls)
        : symbol(sym), bid(b), ask(a), bid_size(bs), ask_size(as), 
          last_price(lp), last_size(ls), 
          timestamp(std::chrono::high_resolution_clock::now()) {}
};

// Order structure
struct Order {
    OrderId id;
    ClientId client_id;
    StrategyId strategy_id;
    Symbol symbol;
    Side side;
    OrderType type;
    TimeInForce time_in_force;
    Quantity quantity;
    Quantity filled_quantity;
    Price price;
    Price stop_price;
    OrderStatus status;
    Timestamp created_at;
    Timestamp updated_at;
    std::string external_id;  // For MT5 integration
    
    Order() = default;
    Order(OrderId oid, ClientId cid, const Symbol& sym, Side s, OrderType ot, 
          Quantity qty, Price p = 0.0)
        : id(oid), client_id(cid), strategy_id(0), symbol(sym), side(s), 
          type(ot), time_in_force(TimeInForce::GTC), quantity(qty), 
          filled_quantity(0.0), price(p), stop_price(0.0), 
          status(OrderStatus::PENDING), 
          created_at(std::chrono::high_resolution_clock::now()),
          updated_at(created_at) {}
    
    // Calculate remaining quantity
    Quantity remaining_quantity() const {
        return quantity - filled_quantity;
    }
    
    // Check if order is complete
    bool is_complete() const {
        return status == OrderStatus::FILLED || 
               status == OrderStatus::CANCELLED || 
               status == OrderStatus::REJECTED ||
               status == OrderStatus::EXPIRED;
    }
    
    // Check if order can be cancelled
    bool can_cancel() const {
        return status == OrderStatus::PENDING || 
               status == OrderStatus::PARTIALLY_FILLED;
    }
};

// Execution/Fill structure
struct Execution {
    uint64_t execution_id;
    OrderId order_id;
    ClientId client_id;
    Symbol symbol;
    Side side;
    Quantity quantity;
    Price price;
    Timestamp timestamp;
    ExecutionType type;
    std::string execution_venue;
    
    Execution() = default;
    Execution(uint64_t eid, OrderId oid, ClientId cid, const Symbol& sym, 
              Side s, Quantity qty, Price p)
        : execution_id(eid), order_id(oid), client_id(cid), symbol(sym), 
          side(s), quantity(qty), price(p), 
          timestamp(std::chrono::high_resolution_clock::now()),
          type(ExecutionType::FILL), execution_venue("VFX") {}
};

// Position structure
struct Position {
    ClientId client_id;
    Symbol symbol;
    Quantity quantity;  // Positive = long, Negative = short
    Price avg_price;
    Price unrealized_pnl;
    Price realized_pnl;
    Timestamp last_updated;
    
    Position() = default;
    Position(ClientId cid, const Symbol& sym)
        : client_id(cid), symbol(sym), quantity(0.0), avg_price(0.0),
          unrealized_pnl(0.0), realized_pnl(0.0),
          last_updated(std::chrono::high_resolution_clock::now()) {}
    
    // Check if position is flat
    bool is_flat() const {
        return std::abs(quantity) < 1e-8;
    }
    
    // Check if position is long
    bool is_long() const {
        return quantity > 1e-8;
    }
    
    // Check if position is short
    bool is_short() const {
        return quantity < -1e-8;
    }
};

// Risk metrics structure
struct RiskMetrics {
    ClientId client_id;
    Price total_exposure;
    Price used_margin;
    Price free_margin;
    Price equity;
    double margin_level;  // Percentage
    RiskLevel risk_level;
    uint32_t open_positions;
    Timestamp last_calculated;
    
    RiskMetrics() = default;
    RiskMetrics(ClientId cid)
        : client_id(cid), total_exposure(0.0), used_margin(0.0), 
          free_margin(0.0), equity(0.0), margin_level(0.0),
          risk_level(RiskLevel::LOW), open_positions(0),
          last_calculated(std::chrono::high_resolution_clock::now()) {}
};

// Strategy signal structure
struct StrategySignal {
    StrategyId strategy_id;
    Symbol symbol;
    Side side;
    Quantity quantity;
    Price target_price;
    Price stop_loss;
    Price take_profit;
    double confidence;
    Timestamp generated_at;
    std::string strategy_name;
    std::unordered_map<std::string, double> metadata;
    
    StrategySignal() = default;
    StrategySignal(StrategyId sid, const Symbol& sym, Side s, Quantity qty, 
                   double conf, const std::string& name)
        : strategy_id(sid), symbol(sym), side(s), quantity(qty),
          target_price(0.0), stop_loss(0.0), take_profit(0.0),
          confidence(conf), 
          generated_at(std::chrono::high_resolution_clock::now()),
          strategy_name(name) {}
};

// Configuration structures
struct TradingConfig {
    bool allow_short_selling = true;
    Price max_position_size = 1000000.0;
    Price max_daily_loss = 50000.0;
    double max_leverage = 10.0;
    uint32_t max_orders_per_minute = 100;
    Duration order_timeout = Duration(30000);  // 30 seconds
    bool enable_risk_checks = true;
    bool enable_strategy_trading = true;
};

struct MarketDataConfig {
    std::vector<Symbol> subscribed_symbols;
    uint32_t update_frequency_ms = 100;
    bool enable_level2_data = false;
    bool enable_tick_data = true;
    std::string data_provider = "internal";
};

// Utility functions
inline std::string to_string(Side side) {
    return side == Side::BUY ? "BUY" : "SELL";
}

inline std::string to_string(OrderType type) {
    switch (type) {
        case OrderType::MARKET: return "MARKET";
        case OrderType::LIMIT: return "LIMIT";
        case OrderType::STOP: return "STOP";
        case OrderType::STOP_LIMIT: return "STOP_LIMIT";
        default: return "UNKNOWN";
    }
}

inline std::string to_string(OrderStatus status) {
    switch (status) {
        case OrderStatus::PENDING: return "PENDING";
        case OrderStatus::PARTIALLY_FILLED: return "PARTIALLY_FILLED";
        case OrderStatus::FILLED: return "FILLED";
        case OrderStatus::CANCELLED: return "CANCELLED";
        case OrderStatus::REJECTED: return "REJECTED";
        case OrderStatus::EXPIRED: return "EXPIRED";
        default: return "UNKNOWN";
    }
}

inline std::string to_string(RiskLevel level) {
    switch (level) {
        case RiskLevel::LOW: return "LOW";
        case RiskLevel::MEDIUM: return "MEDIUM";
        case RiskLevel::HIGH: return "HIGH";
        case RiskLevel::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

// Time utilities
inline uint64_t timestamp_to_ms(const Timestamp& ts) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        ts.time_since_epoch()).count();
}

inline Timestamp ms_to_timestamp(uint64_t ms) {
    return Timestamp(std::chrono::milliseconds(ms));
}

// Price utilities
constexpr double PRICE_EPSILON = 1e-8;

inline bool price_equal(Price a, Price b) {
    return std::abs(a - b) < PRICE_EPSILON;
}

inline bool price_greater(Price a, Price b) {
    return (a - b) > PRICE_EPSILON;
}

inline bool price_less(Price a, Price b) {
    return (b - a) > PRICE_EPSILON;
}

} // namespace trading
} // namespace vfx