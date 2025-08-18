#include "message_protocol.h"
#include <chrono>
#include <iomanip>
#include <sstream>

namespace vfx {
namespace trading {

std::string MessageProtocol::create_market_data_message(const MarketTick& tick) {
    json msg;
    msg["type"] = "market_data";
    msg["data"]["symbol"] = tick.symbol;
    msg["data"]["price"] = tick.last_price;
    msg["data"]["bid"] = tick.bid_price;
    msg["data"]["ask"] = tick.ask_price;
    msg["data"]["volume"] = tick.last_size;
    msg["data"]["change"] = ((tick.last_price - tick.bid_price) / tick.bid_price) * 100;
    msg["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        tick.timestamp.time_since_epoch()).count();
    
    return msg.dump();
}

std::string MessageProtocol::create_market_snapshot(const std::vector<MarketTick>& ticks) {
    json msg;
    msg["type"] = "market_snapshot";
    msg["data"] = json::array();
    
    for (const auto& tick : ticks) {
        json tick_data;
        tick_data["symbol"] = tick.symbol;
        tick_data["price"] = tick.last_price;
        tick_data["bid"] = tick.bid_price;
        tick_data["ask"] = tick.ask_price;
        tick_data["volume"] = tick.last_size;
        msg["data"].push_back(tick_data);
    }
    
    msg["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    
    return msg.dump();
}

std::string MessageProtocol::create_order_confirmation(const Order& order) {
    json msg;
    msg["type"] = "order_confirmation";
    msg["order_id"] = order.id;
    msg["symbol"] = order.symbol;
    msg["side"] = (order.side == Side::BUY) ? "BUY" : "SELL";
    msg["quantity"] = order.quantity;
    msg["price"] = order.price;
    msg["status"] = "submitted";
    msg["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        order.created_at.time_since_epoch()).count();
    
    return msg.dump();
}

std::string MessageProtocol::create_execution_message(const Execution& exec) {
    json msg;
    msg["type"] = "execution";
    msg["data"]["execution_id"] = exec.execution_id;
    msg["data"]["order_id"] = exec.order_id;
    msg["data"]["symbol"] = exec.symbol;
    msg["data"]["side"] = (exec.side == Side::BUY) ? "BUY" : "SELL";
    msg["data"]["quantity"] = exec.quantity;
    msg["data"]["price"] = exec.price;
    msg["data"]["venue"] = exec.execution_venue;
    msg["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        exec.timestamp.time_since_epoch()).count();
    
    return msg.dump();
}

std::string MessageProtocol::create_order_reject(OrderId order_id, const std::string& reason) {
    json msg;
    msg["type"] = "order_reject";
    msg["order_id"] = order_id;
    msg["reason"] = reason;
    msg["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    
    return msg.dump();
}

Order MessageProtocol::parse_order_request(const json& msg) {
    Order order;
    
    order.symbol = msg["symbol"].get<std::string>();
    order.side = (msg["side"].get<std::string>() == "BUY") ? Side::BUY : Side::SELL;
    order.quantity = msg["quantity"].get<double>();
    
    std::string order_type = msg.value("type", "MARKET");
    order.type = (order_type == "LIMIT") ? OrderType::LIMIT : OrderType::MARKET;
    
    if (order.type == OrderType::LIMIT) {
        order.price = msg["price"].get<double>();
    }
    
    order.client_id = msg.value("client_id", 1);
    order.time_in_force = TimeInForce::GTC;
    order.status = OrderStatus::PENDING;
    order.created_at = std::chrono::high_resolution_clock::now();
    order.updated_at = order.created_at;
    
    return order;
}

std::string MessageProtocol::create_position_update(const Position& position) {
    json msg;
    msg["type"] = "position_update";
    msg["data"]["symbol"] = position.symbol;
    msg["data"]["quantity"] = position.quantity;
    msg["data"]["avg_price"] = position.avg_price;
    msg["data"]["unrealized_pnl"] = position.unrealized_pnl;
    msg["data"]["realized_pnl"] = position.realized_pnl;
    msg["data"]["side"] = position.is_long() ? "LONG" : (position.is_short() ? "SHORT" : "FLAT");
    msg["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        position.last_updated.time_since_epoch()).count();
    
    return msg.dump();
}

std::string MessageProtocol::create_positions_snapshot(const std::vector<Position>& positions) {
    json msg;
    msg["type"] = "positions_snapshot";
    msg["data"] = json::array();
    
    for (const auto& pos : positions) {
        json pos_data;
        pos_data["symbol"] = pos.symbol;
        pos_data["quantity"] = pos.quantity;
        pos_data["avg_price"] = pos.avg_price;
        pos_data["unrealized_pnl"] = pos.unrealized_pnl;
        pos_data["realized_pnl"] = pos.realized_pnl;
        msg["data"].push_back(pos_data);
    }
    
    msg["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    
    return msg.dump();
}

std::string MessageProtocol::create_risk_metrics(const RiskMetrics& metrics) {
    json msg;
    msg["type"] = "risk_metrics";
    msg["data"]["total_exposure"] = metrics.total_exposure;
    msg["data"]["used_margin"] = metrics.used_margin;
    msg["data"]["free_margin"] = metrics.free_margin;
    msg["data"]["equity"] = metrics.equity;
    msg["data"]["margin_level"] = metrics.margin_level;
    msg["data"]["open_positions"] = metrics.open_positions;
    msg["data"]["risk_level"] = static_cast<int>(metrics.risk_level);
    msg["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        metrics.last_calculated.time_since_epoch()).count();
    
    return msg.dump();
}

std::string MessageProtocol::create_strategy_signal(const std::string& strategy_name,
                                                   const std::string& signal_type,
                                                   const json& params) {
    json msg;
    msg["type"] = "strategy_signal";
    msg["data"]["strategy"] = strategy_name;
    msg["data"]["signal"] = signal_type;
    msg["data"]["params"] = params;
    msg["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    
    return msg.dump();
}

std::string MessageProtocol::create_strategy_status(const std::string& strategy_name,
                                                   bool active,
                                                   double performance) {
    json msg;
    msg["type"] = "strategy_status";
    msg["data"]["name"] = strategy_name;
    msg["data"]["active"] = active;
    msg["data"]["performance"] = performance;
    msg["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    
    return msg.dump();
}

std::string MessageProtocol::create_connection_message(bool connected) {
    json msg;
    msg["type"] = connected ? "connected" : "disconnected";
    msg["message"] = connected ? "VFX Trading Engine Connected" : "Disconnected from engine";
    msg["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    
    return msg.dump();
}

std::string MessageProtocol::create_error_message(const std::string& error) {
    json msg;
    msg["type"] = "error";
    msg["message"] = error;
    msg["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    
    return msg.dump();
}

std::string MessageProtocol::create_pong_message(int64_t timestamp) {
    json msg;
    msg["type"] = "pong";
    msg["timestamp"] = timestamp;
    msg["server_time"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    
    return msg.dump();
}

std::string MessageProtocol::create_system_metrics(double cpu_usage, double memory_usage,
                                                  int active_connections, double latency_ms) {
    json msg;
    msg["type"] = "system_metrics";
    msg["data"]["cpu_usage"] = cpu_usage;
    msg["data"]["memory_usage"] = memory_usage;
    msg["data"]["active_connections"] = active_connections;
    msg["data"]["latency_ms"] = latency_ms;
    msg["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    
    return msg.dump();
}

json MessageProtocol::parse_message(const std::string& message) {
    try {
        return json::parse(message);
    } catch (const std::exception& e) {
        return json{{"type", "error"}, {"message", "Invalid JSON"}};
    }
}

std::string MessageProtocol::get_message_type(const json& msg) {
    if (msg.contains("type")) {
        return msg["type"].get<std::string>();
    }
    return "unknown";
}

} // namespace trading
} // namespace vfx