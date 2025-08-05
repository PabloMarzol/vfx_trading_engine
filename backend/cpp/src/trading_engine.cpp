#include "trading_engine.h"
#include "order_manager.h"
#include "market_data.h"
#include "risk_manager.h"
#include "websocket_server.h"

namespace vfx {
namespace trading {

TradingEngine::TradingEngine() {
    std::cout << "🚀 VFX Trading Engine v1.0.0 - Initializing..." << std::endl;
    
    // Initialize components
    market_data_ = std::make_shared<MarketDataManager>();
    risk_manager_ = std::make_shared<RiskManager>(market_data_);
    order_manager_ = std::make_shared<OrderManager>(market_data_, risk_manager_);
    
    // Initialize WebSocket server
    websocket_server_ = std::make_shared<WebSocketServer>("localhost", 8080);
    websocket_server_->set_order_manager(order_manager_);
    websocket_server_->set_market_data(market_data_);
    websocket_server_->set_risk_manager(risk_manager_);
    
    // Setup default symbols for market data
    std::vector<Symbol> default_symbols = {
        "BTC/USD", "ETH/USD", "AAPL", "TSLA", "NVDA", "SPY", "QQQ"
    };
    
    for (const auto& symbol : default_symbols) {
        market_data_->subscribe(symbol);
    }
    
    std::cout << "✅ VFX Trading Engine components initialized" << std::endl;
}

TradingEngine::~TradingEngine() {
    Stop();
    std::cout << "🔌 VFX Trading Engine shutdown complete" << std::endl;
}

OrderId TradingEngine::SubmitOrder(const std::string& symbol, Side side, OrderType type, Quantity quantity, Price price) {
    if (!running_) {
        std::cout << "❌ Engine not running - cannot submit order" << std::endl;
        return 0;
    }
    
    if (!order_manager_) {
        std::cout << "❌ Order manager not available" << std::endl;
        return 0;
    }
    
    // Use default client ID for demo
    ClientId client_id = 1;
    
    return order_manager_->submit_order(client_id, symbol, side, type, quantity, price);
}

bool TradingEngine::CancelOrder(OrderId order_id) {
    if (!order_manager_) {
        return false;
    }
    
    return order_manager_->cancel_order(order_id);
}

void TradingEngine::Start() {
    if (running_) {
        std::cout << "⚠️ Engine already running" << std::endl;
        return;
    }
    
    running_ = true;
    
    // Start all components
    if (market_data_) {
        market_data_->start();
    }
    
    if (order_manager_) {
        order_manager_->start();
    }
    
    // Start WebSocket server
    if (websocket_server_) {
        websocket_server_->start();
    }
    
    std::cout << "✅ VFX Trading Engine started successfully" << std::endl;
    std::cout << "🌐 WebSocket server running on ws://localhost:8080" << std::endl;
    std::cout << "🎯 Ready for frontend connections!" << std::endl;
}

void TradingEngine::Stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    // Stop all components
    if (websocket_server_) {
        websocket_server_->stop();
    }
    
    if (order_manager_) {
        order_manager_->stop();
    }
    
    if (market_data_) {
        market_data_->stop();
    }
    
    std::cout << "🛑 VFX Trading Engine stopped" << std::endl;
}

bool TradingEngine::IsRunning() const {
    return running_;
}

void TradingEngine::RunDemo() {
    std::cout << std::endl << "🎯 Running VFX Trading Engine Demo..." << std::endl;
    std::cout << "=======================================" << std::endl;
    
    // Demo 1: Market orders
    std::cout << "\n📊 Testing Market Orders:" << std::endl;
    OrderId order1 = SubmitOrder("BTC/USD", Side::BUY, OrderType::MARKET, 0.1);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    OrderId order2 = SubmitOrder("ETH/USD", Side::BUY, OrderType::MARKET, 2.5);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Demo 2: Limit orders
    std::cout << "\n📈 Testing Limit Orders:" << std::endl;
    OrderId order3 = SubmitOrder("AAPL", Side::BUY, OrderType::LIMIT, 100, 170.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    OrderId order4 = SubmitOrder("TSLA", Side::SELL, OrderType::LIMIT, 50, 250.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Demo 3: Mixed orders
    std::cout << "\n💹 Testing Mixed Orders:" << std::endl;
    OrderId order5 = SubmitOrder("NVDA", Side::BUY, OrderType::MARKET, 25);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    OrderId order6 = SubmitOrder("SPY", Side::SELL, OrderType::LIMIT, 200, 445.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Demo 4: Order cancellation
    std::cout << "\n❌ Testing Order Cancellation:" << std::endl;
    if (order3 > 0) {
        bool cancelled = CancelOrder(order3);
        std::cout << "Cancel order " << order3 << ": " << (cancelled ? "✅ Success" : "❌ Failed") << std::endl;
    }
    
    // Show which orders were processed
    std::cout << "\n📋 Orders Processed:" << std::endl;
    std::cout << "  Order 1 (BTC Market): " << (order1 > 0 ? "✅ Submitted" : "❌ Failed") << std::endl;
    std::cout << "  Order 2 (ETH Market): " << (order2 > 0 ? "✅ Submitted" : "❌ Failed") << std::endl;
    std::cout << "  Order 3 (AAPL Limit): " << (order3 > 0 ? "✅ Submitted" : "❌ Failed") << std::endl;
    std::cout << "  Order 4 (TSLA Limit): " << (order4 > 0 ? "✅ Submitted" : "❌ Failed") << std::endl;
    std::cout << "  Order 5 (NVDA Market): " << (order5 > 0 ? "✅ Submitted" : "❌ Failed") << std::endl;
    std::cout << "  Order 6 (SPY Limit): " << (order6 > 0 ? "✅ Submitted" : "❌ Failed") << std::endl;
    
    // Demo 5: Show statistics
    std::cout << "\n📊 Engine Statistics:" << std::endl;
    if (order_manager_) {
        auto stats = order_manager_->get_statistics();
        std::cout << "  Total Orders: " << stats.total_orders << std::endl;
        std::cout << "  Active Orders: " << stats.active_orders << std::endl;
        std::cout << "  Total Executions: " << stats.total_executions << std::endl;
        std::cout << "  Active Symbols: " << stats.active_symbols << std::endl;
    }
    
    if (market_data_) {
        auto md_stats = market_data_->get_statistics();
        std::cout << "  Market Data Symbols: " << md_stats.total_symbols << std::endl;
        std::cout << "  Total Ticks: " << md_stats.total_ticks_received << std::endl;
    }
    
    if (risk_manager_) {
        auto risk_stats = risk_manager_->get_statistics();
        std::cout << "  Risk Checks: " << risk_stats.total_risk_checks << std::endl;
        std::cout << "  Risk Violations: " << risk_stats.risk_violations << std::endl;
        std::cout << "  Active Clients: " << risk_stats.active_clients << std::endl;
    }
    
    std::cout << "\n🎉 VFX Trading Engine demo completed successfully!" << std::endl;
    std::cout << "=======================================" << std::endl;
}

// Additional methods for component access
std::shared_ptr<OrderManager> TradingEngine::GetOrderManager() const {
    return order_manager_;
}

std::shared_ptr<MarketDataManager> TradingEngine::GetMarketDataManager() const {
    return market_data_;
}

std::shared_ptr<RiskManager> TradingEngine::GetRiskManager() const {
    return risk_manager_;
}

std::vector<Symbol> TradingEngine::GetActiveSymbols() const {
    if (market_data_) {
        return market_data_->get_subscribed_symbols();
    }
    return {};
}

uint64_t TradingEngine::GetTotalOrders() const {
    if (order_manager_) {
        return order_manager_->get_statistics().total_orders;
    }
    return 0;
}

uint64_t TradingEngine::GetTotalExecutions() const {
    if (order_manager_) {
        return order_manager_->get_statistics().total_executions;
    }
    return 0;
}

} // namespace trading
} // namespace vfx