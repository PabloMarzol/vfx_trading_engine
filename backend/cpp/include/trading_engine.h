#pragma once

#include "types.h"
#include <memory>
#include <vector>
#include <string>
#include <atomic>
#include <thread>
#include <iostream>

namespace vfx {
namespace trading {

// Forward declarations
class OrderManager;
class MarketDataManager;
class RiskManager;
class WebSocketServer;

class TradingEngine {
private:
    // Core components
    std::shared_ptr<OrderManager> order_manager_;
    std::shared_ptr<MarketDataManager> market_data_;
    std::shared_ptr<RiskManager> risk_manager_;
    std::shared_ptr<WebSocketServer> websocket_server_;
    
    // Engine state
    std::atomic<bool> running_{false};

public:
    TradingEngine();
    ~TradingEngine();

    // Core trading operations
    OrderId SubmitOrder(const std::string& symbol, Side side, OrderType type, Quantity quantity, Price price = 0.0);
    bool CancelOrder(OrderId order_id);
    
    // Engine lifecycle
    void Start();
    void Stop();
    bool IsRunning() const;
    
    // Demo and testing
    void RunDemo();
    
    // Component access
    std::shared_ptr<OrderManager> GetOrderManager() const;
    std::shared_ptr<MarketDataManager> GetMarketDataManager() const;
    std::shared_ptr<RiskManager> GetRiskManager() const;
    
    // Status and statistics
    std::vector<Symbol> GetActiveSymbols() const;
    uint64_t GetTotalOrders() const;
    uint64_t GetTotalExecutions() const;
};

} // namespace trading
} // namespace vfx