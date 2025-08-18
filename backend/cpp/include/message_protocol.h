#pragma once

#include "types.h"
#include "../external/json/json.hpp"
#include <string>
#include <vector>

namespace vfx {
namespace trading {

using json = nlohmann::json;

class MessageProtocol {
public:
    // Market Data Messages
    static std::string create_market_data_message(const MarketTick& tick);
    static std::string create_market_snapshot(const std::vector<MarketTick>& ticks);
    
    // Order Messages
    static std::string create_order_confirmation(const Order& order);
    static std::string create_execution_message(const Execution& exec);
    static std::string create_order_reject(OrderId order_id, const std::string& reason);
    static Order parse_order_request(const json& msg);
    
    // Position Messages
    static std::string create_position_update(const Position& position);
    static std::string create_positions_snapshot(const std::vector<Position>& positions);
    
    // Risk Messages
    static std::string create_risk_metrics(const RiskMetrics& metrics);
    
    // Strategy Messages
    static std::string create_strategy_signal(const std::string& strategy_name, 
                                             const std::string& signal_type,
                                             const json& params);
    static std::string create_strategy_status(const std::string& strategy_name,
                                             bool active,
                                             double performance);
    
    // System Messages
    static std::string create_connection_message(bool connected);
    static std::string create_error_message(const std::string& error);
    static std::string create_pong_message(int64_t timestamp);
    static std::string create_system_metrics(double cpu_usage, double memory_usage, 
                                            int active_connections, double latency_ms);
    
    // Parse incoming messages
    static json parse_message(const std::string& message);
    static std::string get_message_type(const json& msg);
};

} // namespace trading
} // namespace vfx