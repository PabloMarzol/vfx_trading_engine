#include "trading_engine.h"

namespace vfx {
namespace trading {

TradingEngine::TradingEngine() {
    std::cout << "🚀 VFX Trading Engine v1.0.0 - Initialized" << std::endl;
}

TradingEngine::~TradingEngine() {
    Stop();
    std::cout << "🔌 VFX Trading Engine shutdown" << std::endl;
}

OrderId TradingEngine::SubmitOrder(const std::string& symbol, Side side, OrderType type, Quantity quantity, Price price) {
    OrderId order_id = next_order_id_++;
ECHO is off.
    auto order = std::make_unique<Order>(order_id, symbol, side, type, quantity, price);
ECHO is off.
    std::string side_str = (side == Side::BUY) ? "BUY" : "SELL";
    std::string type_str = (type == OrderType::MARKET) ? "MARKET" : "LIMIT";
ECHO is off.
    std::cout << "📝 Order " << order_id << ": " << side_str << " " << quantity << " " << symbol;
    if(type == OrderType::LIMIT) {
        std::cout << " @ $" << price;
    }
    std::cout << " ^(" << type_str << "^)" << std::endl;
ECHO is off.
    {
        std::unique_lock<std::shared_mutex> lock(orders_mutex_);
        orders_[order_id] = std::move(order);
    }
ECHO is off.
    ExecuteOrder(order_id);
    return order_id;
}

bool TradingEngine::CancelOrder(OrderId order_id) {
    std::unique_lock<std::shared_mutex> lock(orders_mutex_);
ECHO is off.
    auto it = orders_.find(order_id);
    if (it != orders_.end() && it->second->status == OrderStatus::PENDING) {
        it->second->status = OrderStatus::CANCELLED;
        std::cout << "❌ Order cancelled: " << order_id << std::endl;
        return true;
    }
    return false;
}

void TradingEngine::Start() {
    running_ = true;
    std::cout << "✅ Trading engine started" << std::endl;
}

void TradingEngine::Stop() {
    running_ = false;
    std::cout << "🛑 Trading engine stopped" << std::endl;
}

bool TradingEngine::IsRunning() const {
    return running_;
}

void TradingEngine::RunDemo() {
    std::cout << std::endl << "🎯 Running VFX Trading Demo..." << std::endl;
ECHO is off.
    SubmitOrder("BTC/USD", Side::BUY, OrderType::MARKET, 0.1);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
ECHO is off.
    SubmitOrder("ETH/USD", Side::BUY, OrderType::LIMIT, 2.5, 3900.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
ECHO is off.
    SubmitOrder("AAPL", Side::SELL, OrderType::MARKET, 50);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
ECHO is off.
    SubmitOrder("TSLA", Side::BUY, OrderType::LIMIT, 10, 250.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
ECHO is off.
    SubmitOrder("NVDA", Side::SELL, OrderType::MARKET, 5);
ECHO is off.
    std::cout << "🎉 Demo completed successfully!" << std::endl;
}

void TradingEngine::ExecuteOrder(OrderId order_id) {
    std::shared_lock<std::shared_mutex> lock(orders_mutex_);
ECHO is off.
    auto it = orders_.find(order_id);
    if (it == orders_.end()) return;
ECHO is off.
    Order& order = *it->second;
    Price exec_price = (order.type == OrderType::MARKET) ? GetMarketPrice(order.symbol) : order.price;
ECHO is off.
    // Add some realistic price variation
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.999, 1.001);
    exec_price *= dis(gen);
ECHO is off.
    order.status = OrderStatus::FILLED;
    order.price = exec_price;
ECHO is off.
    std::string side_str = (order.side == Side::BUY) ? "BUY" : "SELL";
    std::cout << "✅ EXECUTED: Order " << order_id << " - " << side_str << " " << order.quantity 
              << " " << order.symbol << " @ $" << exec_price << std::endl;
}

Price TradingEngine::GetMarketPrice(const std::string& symbol) const {
    if(symbol == "BTC/USD") return 68450.0;
    if(symbol == "ETH/USD") return 3892.0;
    if(symbol == "AAPL") return 175.0;
    if(symbol == "TSLA") return 245.0;
    if(symbol == "NVDA") return 875.0;
    return 100.0;
}

}}
