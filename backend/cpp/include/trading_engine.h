#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <cstdint>
#include <algorithm>
#include <iostream>
#include <random>

namespace vfx {
namespace trading {

using Price = double;
using Quantity = double;
using OrderId = uint64_t;
using Timestamp = std::chrono::high_resolution_clock::time_point;

enum class Side : uint8_t { BUY = 0, SELL = 1 };
enum class OrderType : uint8_t { MARKET = 0, LIMIT = 1 };
enum class OrderStatus : uint8_t { PENDING = 0, FILLED = 1, CANCELLED = 2 };

struct Order {
    OrderId id;
    std::string symbol;
    Side side;
    OrderType type;
    Quantity quantity;
    Price price;
    OrderStatus status;
    Timestamp created_at;

    Order(OrderId id, const std::string& symbol, Side side, OrderType type, Quantity qty, Price price = 0.0)
        : id(id), symbol(symbol), side(side), type(type), quantity(qty), price(price),
          status(OrderStatus::PENDING), created_at(std::chrono::high_resolution_clock::now()) {}
};

class TradingEngine {
private:
    std::unordered_map<OrderId, std::unique_ptr<Order>> orders_;
    mutable std::shared_mutex orders_mutex_;
    std::atomic<OrderId> next_order_id_{1000};
    std::atomic<bool> running_{false};

public:
    TradingEngine();
    ~TradingEngine();

    OrderId SubmitOrder(const std::string& symbol, Side side, OrderType type, Quantity quantity, Price price = 0.0);
    bool CancelOrder(OrderId order_id);
    void Start();
    void Stop();
    bool IsRunning() const;
    void RunDemo();

private:
    void ExecuteOrder(OrderId order_id);
    Price GetMarketPrice(const std::string& symbol) const;
};

}}
