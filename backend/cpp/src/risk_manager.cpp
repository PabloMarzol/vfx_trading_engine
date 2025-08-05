#include "risk_manager.h"
#include "market_data.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace vfx {
namespace trading {

RiskManager::RiskManager(std::shared_ptr<MarketDataManager> market_data)
    : market_data_(market_data) {
    
    // Set conservative default limits
    default_limits_.max_position_value = 1000000.0;
    default_limits_.max_daily_loss = 50000.0;
    default_limits_.max_total_exposure = 5000000.0;
    default_limits_.max_leverage = 10.0;
    default_limits_.max_orders_per_minute = 100;
    default_limits_.max_open_positions = 50;
    default_limits_.margin_call_level = 100.0;
    default_limits_.stop_out_level = 50.0;
    
    std::cout << "🛡️ RiskManager initialized with default limits" << std::endl;
}

RiskManager::~RiskManager() {
    std::cout << "🛡️ RiskManager destroyed" << std::endl;
}

void RiskManager::set_client_limits(ClientId client_id, const RiskLimits& limits) {
    std::unique_lock<std::shared_mutex> lock(limits_mutex_);
    client_limits_[client_id] = limits;
    
    std::cout << "⚙️ Set risk limits for client " << client_id << std::endl;
}

RiskLimits RiskManager::get_client_limits(ClientId client_id) const {
    std::shared_lock<std::shared_mutex> lock(limits_mutex_);
    
    auto it = client_limits_.find(client_id);
    return (it != client_limits_.end()) ? it->second : default_limits_;
}

bool RiskManager::check_order_risk(const Order& order) const {
    if (!risk_checks_enabled_) {
        return true;
    }
    
    total_risk_checks_++;
    
    try {
        // Basic order validation
        if (!validate_order_limits(order)) {
            risk_violations_++;
            return false;
        }
        
        // Position size validation
        if (!validate_position_limits(order.client_id, order.symbol, order.quantity)) {
            risk_violations_++;
            return false;
        }
        
        // Exposure validation
        Price order_value = order.quantity * 
            ((order.price > 0) ? order.price : get_current_price(order.symbol));
        
        if (!validate_exposure_limits(order.client_id, order_value)) {
            risk_violations_++;
            return false;
        }
        
        // Leverage check
        if (!check_leverage_limit(order.client_id, order_value)) {
            risk_violations_++;
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "❌ Error in risk check: " << e.what() << std::endl;
        return false;
    }
}

bool RiskManager::can_open_position(ClientId client_id, const Symbol& /*symbol*/, 
                                   Side /*side*/, Quantity quantity, Price price) const {
    auto limits = get_client_limits(client_id);
    
    // Check position size limit
    Price position_value = quantity * price;
    if (position_value > limits.max_position_value) {
        return false;
    }
    
    // Check total exposure
    auto client_risk = get_client_risk(client_id);
    if (client_risk.total_exposure + position_value > limits.max_total_exposure) {
        return false;
    }
    
    // Check position count
    if (client_risk.open_positions_count >= limits.max_open_positions) {
        return false;
    }
    
    return true;
}

bool RiskManager::check_leverage_limit(ClientId client_id, Price additional_exposure) const {
    auto limits = get_client_limits(client_id);
    auto client_risk = get_client_risk(client_id);
    
    if (client_risk.total_equity <= 0) {
        return false; // No equity to leverage
    }
    
    double new_leverage = (client_risk.total_exposure + additional_exposure) / client_risk.total_equity;
    return new_leverage <= limits.max_leverage;
}

bool RiskManager::check_position_size_limit(ClientId client_id, const Symbol& symbol, 
                                          Quantity new_quantity) const {
    auto limits = get_client_limits(client_id);
    Price current_price = get_current_price(symbol);
    Price position_value = std::abs(new_quantity) * current_price;
    
    return position_value <= limits.max_position_value;
}

void RiskManager::update_position(const Execution& execution) {
    std::unique_lock<std::shared_mutex> lock(positions_mutex_);
    
    ClientId client_id = execution.client_id;
    const Symbol& symbol = execution.symbol;
    
    // Get or create position
    auto& client_positions = positions_[client_id];
    auto it = client_positions.find(symbol);
    
    if (it == client_positions.end()) {
        // Create new position
        Position position(client_id, symbol);
        client_positions[symbol] = position;
        it = client_positions.find(symbol);
    }
    
    Position& position = it->second;
    
    // Update position with execution
    if (execution.side == Side::BUY) {
        if (position.quantity >= 0) {
            // Increasing long position or opening long
            Price total_cost = (position.quantity * position.avg_price) + 
                              (execution.quantity * execution.price);
            position.quantity += execution.quantity;
            position.avg_price = total_cost / position.quantity;
        } else {
            // Reducing short position
            position.quantity += execution.quantity;
            if (position.quantity >= 0 && position.quantity <= 1e-8) {
                // Position closed, realize P&L
                Price realized_pnl = std::abs(execution.quantity) * 
                                    (position.avg_price - execution.price);
                position.realized_pnl += realized_pnl;
                
                if (std::abs(position.quantity) <= 1e-8) {
                    position.quantity = 0;
                    position.avg_price = 0;
                }
            }
        }
    } else { // SELL
        if (position.quantity <= 0) {
            // Increasing short position or opening short
            Price total_cost = (std::abs(position.quantity) * position.avg_price) + 
                              (execution.quantity * execution.price);
            position.quantity -= execution.quantity;
            position.avg_price = total_cost / std::abs(position.quantity);
        } else {
            // Reducing long position
            position.quantity -= execution.quantity;
            if (position.quantity <= 0 && position.quantity >= -1e-8) {
                // Position closed, realize P&L
                Price realized_pnl = execution.quantity * 
                                    (execution.price - position.avg_price);
                position.realized_pnl += realized_pnl;
                
                if (std::abs(position.quantity) <= 1e-8) {
                    position.quantity = 0;
                    position.avg_price = 0;
                }
            }
        }
    }
    
    position.last_updated = std::chrono::high_resolution_clock::now();
    
    // Update risk calculations
    lock.unlock();
    calculate_client_risk(client_id);
    
    std::cout << "📊 Updated position: " << symbol << " = " << position.quantity 
              << " @ $" << position.avg_price << std::endl;
}

void RiskManager::calculate_client_risk(ClientId client_id) {
    std::unique_lock<std::shared_mutex> risk_lock(risks_mutex_);
    std::shared_lock<std::shared_mutex> pos_lock(positions_mutex_);
    
    ensure_client_risk_exists(client_id);
    ClientRisk& client_risk = client_risks_[client_id];
    
    // Reset calculations
    client_risk.total_exposure = 0;
    client_risk.unrealized_pnl = 0;
    client_risk.open_positions_count = 0;
    client_risk.position_risks.clear();
    
    // Calculate from positions
    auto client_pos_it = positions_.find(client_id);
    if (client_pos_it != positions_.end()) {
        for (const auto& [symbol, position] : client_pos_it->second) {
            if (!position.is_flat()) {
                Price current_price = get_current_price(symbol);
                Price position_value = std::abs(position.quantity) * current_price;
                Price unrealized_pnl = calculate_unrealized_pnl(position, current_price);
                
                client_risk.total_exposure += position_value;
                client_risk.unrealized_pnl += unrealized_pnl;
                client_risk.open_positions_count++;
                
                // Create position risk
                PositionRisk pos_risk(client_id, symbol);
                pos_risk.position_size = position.quantity;
                pos_risk.avg_price = position.avg_price;
                pos_risk.current_price = current_price;
                pos_risk.unrealized_pnl = unrealized_pnl;
                pos_risk.market_value = position_value;
                pos_risk.risk_percentage = (position_value / client_risk.total_equity) * 100.0;
                pos_risk.risk_level = (pos_risk.risk_percentage > 20.0) ? RiskLevel::HIGH : 
                                     (pos_risk.risk_percentage > 10.0) ? RiskLevel::MEDIUM : RiskLevel::LOW;
                
                client_risk.position_risks.push_back(pos_risk);
            }
        }
    }
    
    // Calculate margin and leverage
    if (client_risk.total_equity > 0) {
        client_risk.leverage_ratio = client_risk.total_exposure / client_risk.total_equity;
        client_risk.margin_level = (client_risk.total_equity / client_risk.used_margin) * 100.0;
    }
    
    // Update risk level
    update_risk_level(client_id);
    
    // Check for margin calls and stop outs
    check_margin_levels(client_id);
    
    client_risk.last_calculated = std::chrono::high_resolution_clock::now();
}

ClientRisk RiskManager::get_client_risk(ClientId client_id) const {
    std::shared_lock<std::shared_mutex> lock(risks_mutex_);
    
    auto it = client_risks_.find(client_id);
    return (it != client_risks_.end()) ? it->second : ClientRisk(client_id);
}

void RiskManager::add_position(ClientId client_id, const Position& position) {
    std::unique_lock<std::shared_mutex> lock(positions_mutex_);
    positions_[client_id][position.symbol] = position;
    
    lock.unlock();
    calculate_client_risk(client_id);
}

Position RiskManager::get_position(ClientId client_id, const Symbol& symbol) const {
    std::shared_lock<std::shared_mutex> lock(positions_mutex_);
    
    auto client_it = positions_.find(client_id);
    if (client_it == positions_.end()) {
        return Position(client_id, symbol);
    }
    
    auto pos_it = client_it->second.find(symbol);
    return (pos_it != client_it->second.end()) ? pos_it->second : Position(client_id, symbol);
}

std::vector<Position> RiskManager::get_client_positions(ClientId client_id) const {
    std::shared_lock<std::shared_mutex> lock(positions_mutex_);
    
    std::vector<Position> positions;
    auto it = positions_.find(client_id);
    if (it != positions_.end()) {
        for (const auto& [symbol, position] : it->second) {
            if (!position.is_flat()) {
                positions.push_back(position);
            }
        }
    }
    
    return positions;
}

void RiskManager::force_close_position(ClientId client_id, const Symbol& symbol) {
    std::unique_lock<std::shared_mutex> lock(positions_mutex_);
    
    auto client_it = positions_.find(client_id);
    if (client_it != positions_.end()) {
        auto pos_it = client_it->second.find(symbol);
        if (pos_it != client_it->second.end()) {
            pos_it->second.quantity = 0;
            pos_it->second.avg_price = 0;
            
            create_risk_event(RiskEventType::STOP_OUT, client_id, 
                             "Position forcibly closed: " + symbol, RiskLevel::CRITICAL, symbol);
            
            std::cout << "🚨 Force closed position: " << symbol << " for client " << client_id << std::endl;
        }
    }
    
    lock.unlock();
    calculate_client_risk(client_id);
}

void RiskManager::force_close_all_positions(ClientId client_id) {
    std::unique_lock<std::shared_mutex> lock(positions_mutex_);
    
    auto client_it = positions_.find(client_id);
    if (client_it != positions_.end()) {
        uint32_t closed_count = 0;
        for (auto& [symbol, position] : client_it->second) {
            if (!position.is_flat()) {
                position.quantity = 0;
                position.avg_price = 0;
                closed_count++;
            }
        }
        
        if (closed_count > 0) {
            create_risk_event(RiskEventType::STOP_OUT, client_id, 
                             "All positions forcibly closed (" + std::to_string(closed_count) + ")", 
                             RiskLevel::CRITICAL);
            
            std::cout << "🚨 Force closed all positions (" << closed_count << ") for client " << client_id << std::endl;
        }
    }
    
    lock.unlock();
    calculate_client_risk(client_id);
}

void RiskManager::trigger_margin_call(ClientId client_id) {
    {
        std::unique_lock<std::shared_mutex> lock(risks_mutex_);
        ensure_client_risk_exists(client_id);
        client_risks_[client_id].margin_call_triggered = true;
    }
    
    create_risk_event(RiskEventType::MARGIN_CALL, client_id, 
                     "Margin call triggered", RiskLevel::HIGH);
    
    auto client_risk = get_client_risk(client_id);
    notify_margin_call(client_id, client_risk);
    
    std::cout << "📞 Margin call triggered for client " << client_id << std::endl;
}

void RiskManager::trigger_stop_out(ClientId client_id) {
    {
        std::unique_lock<std::shared_mutex> lock(risks_mutex_);
        ensure_client_risk_exists(client_id);
        client_risks_[client_id].stop_out_triggered = true;
    }
    
    create_risk_event(RiskEventType::STOP_OUT, client_id, 
                     "Stop out triggered - forcing position closure", RiskLevel::CRITICAL);
    
    auto client_risk = get_client_risk(client_id);
    notify_stop_out(client_id, client_risk);
    
    // Force close all positions
    force_close_all_positions(client_id);
    
    std::cout << "🛑 Stop out triggered for client " << client_id << std::endl;
}

bool RiskManager::validate_order_limits(const Order& order) const {
    auto limits = get_client_limits(order.client_id);
    
    // Check basic limits
    if (order.quantity <= 0) return false;
    
    Price order_value = order.quantity * 
        ((order.price > 0) ? order.price : get_current_price(order.symbol));
    
    return order_value <= limits.max_position_value;
}

bool RiskManager::validate_position_limits(ClientId client_id, const Symbol& symbol, 
                                         Quantity additional_quantity) const {
    auto current_position = get_position(client_id, symbol);
    Quantity new_total = std::abs(current_position.quantity + 
                                 ((current_position.quantity >= 0) ? additional_quantity : -additional_quantity));
    
    return check_position_size_limit(client_id, symbol, new_total);
}

bool RiskManager::validate_exposure_limits(ClientId client_id, Price additional_exposure) const {
    auto limits = get_client_limits(client_id);
    auto client_risk = get_client_risk(client_id);
    
    return (client_risk.total_exposure + additional_exposure) <= limits.max_total_exposure;
}

Price RiskManager::calculate_unrealized_pnl(const Position& position, Price current_price) const {
    if (position.is_flat()) return 0.0;
    
    if (position.is_long()) {
        return position.quantity * (current_price - position.avg_price);
    } else {
        return std::abs(position.quantity) * (position.avg_price - current_price);
    }
}

void RiskManager::check_margin_levels(ClientId client_id) {
    auto limits = get_client_limits(client_id);
    auto client_risk = get_client_risk(client_id);
    
    if (client_risk.margin_level <= limits.stop_out_level && !client_risk.stop_out_triggered) {
        trigger_stop_out(client_id);
    } else if (client_risk.margin_level <= limits.margin_call_level && !client_risk.margin_call_triggered) {
        trigger_margin_call(client_id);
    }
}

void RiskManager::update_risk_level(ClientId client_id) {
    std::unique_lock<std::shared_mutex> lock(risks_mutex_);
    ensure_client_risk_exists(client_id);
    
    ClientRisk& client_risk = client_risks_[client_id];
    client_risk.overall_risk_level = calculate_risk_level(client_risk.margin_level, client_risk.leverage_ratio);
}

RiskLevel RiskManager::calculate_risk_level(double margin_level, double leverage) const {
    if (margin_level <= 50.0 || leverage >= 20.0) {
        return RiskLevel::CRITICAL;
    } else if (margin_level <= 100.0 || leverage >= 10.0) {
        return RiskLevel::HIGH;
    } else if (margin_level <= 200.0 || leverage >= 5.0) {
        return RiskLevel::MEDIUM;
    } else {
        return RiskLevel::LOW;
    }
}

Price RiskManager::get_current_price(const Symbol& symbol) const {
    if (!market_data_) {
        // Fallback prices for testing
        static std::unordered_map<Symbol, Price> test_prices = {
            {"BTC/USD", 68450.0}, {"ETH/USD", 3892.0}, {"AAPL", 175.0},
            {"TSLA", 245.0}, {"NVDA", 875.0}
        };
        
        auto it = test_prices.find(symbol);
        return (it != test_prices.end()) ? it->second : 100.0;
    }
    
    return market_data_->get_last_price(symbol);
}

void RiskManager::create_risk_event(RiskEventType type, ClientId client_id, 
                                   const std::string& description, RiskLevel severity,
                                   const Symbol& symbol) {
    RiskEvent event(type, client_id, description, severity);
    event.symbol = symbol;
    
    {
        std::unique_lock<std::shared_mutex> lock(events_mutex_);
        risk_events_.push_back(event);
        
        // Limit event history
        if (risk_events_.size() > 10000) {
            risk_events_.erase(risk_events_.begin(), risk_events_.begin() + 5000);
        }
    }
    
    notify_risk_event(event);
}

void RiskManager::notify_risk_event(const RiskEvent& event) {
    if (risk_event_callback_) {
        try {
            risk_event_callback_(event);
        } catch (const std::exception& e) {
            std::cout << "❌ Error in risk event callback: " << e.what() << std::endl;
        }
    }
}

void RiskManager::notify_margin_call(ClientId client_id, const ClientRisk& risk) {
    if (margin_call_callback_) {
        try {
            margin_call_callback_(client_id, risk);
        } catch (const std::exception& e) {
            std::cout << "❌ Error in margin call callback: " << e.what() << std::endl;
        }
    }
}

void RiskManager::notify_stop_out(ClientId client_id, const ClientRisk& risk) {
    if (stop_out_callback_) {
        try {
            stop_out_callback_(client_id, risk);
        } catch (const std::exception& e) {
            std::cout << "❌ Error in stop out callback: " << e.what() << std::endl;
        }
    }
}

std::vector<RiskEvent> RiskManager::get_recent_events(uint32_t count) const {
    std::shared_lock<std::shared_mutex> lock(events_mutex_);
    
    std::vector<RiskEvent> recent;
    uint32_t start_idx = (risk_events_.size() > count) ? risk_events_.size() - count : 0;
    
    for (uint32_t i = start_idx; i < risk_events_.size(); ++i) {
        recent.push_back(risk_events_[i]);
    }
    
    return recent;
}

RiskManager::RiskStatistics RiskManager::get_statistics() const {
    RiskStatistics stats;
    stats.total_risk_checks = total_risk_checks_;
    stats.risk_violations = risk_violations_;
    
    {
        std::shared_lock<std::shared_mutex> lock(risks_mutex_);
        stats.active_clients = client_risks_.size();
        stats.high_risk_clients = 0;
        
        double total_leverage = 0;
        double total_margin_level = 0;
        uint32_t clients_with_positions = 0;
        
        for (const auto& [client_id, risk] : client_risks_) {
            if (risk.overall_risk_level >= RiskLevel::HIGH) {
                stats.high_risk_clients++;
            }
            
            if (risk.open_positions_count > 0) {
                total_leverage += risk.leverage_ratio;
                total_margin_level += risk.margin_level;
                clients_with_positions++;
            }
        }
        
        stats.avg_leverage = clients_with_positions > 0 ? total_leverage / clients_with_positions : 0;
        stats.avg_margin_level = clients_with_positions > 0 ? total_margin_level / clients_with_positions : 0;
    }
    
    // Count today's events (simplified)
    stats.margin_calls_today = 0;
    stats.stop_outs_today = 0;
    
    return stats;
}

void RiskManager::ensure_client_risk_exists(ClientId client_id) {
    auto it = client_risks_.find(client_id);
    if (it == client_risks_.end()) {
        client_risks_[client_id] = ClientRisk(client_id);
        client_risks_[client_id].total_equity = 100000.0; // Default starting equity
    }
}

// RiskCalculator implementation
double RiskCalculator::calculate_var(const std::vector<Price>& returns, double confidence) {
    if (returns.empty()) return 0.0;
    
    std::vector<Price> sorted_returns = returns;
    std::sort(sorted_returns.begin(), sorted_returns.end());
    
    size_t index = static_cast<size_t>((1.0 - confidence) * sorted_returns.size());
    return sorted_returns[index];
}

double RiskCalculator::calculate_sharpe_ratio(const std::vector<Price>& returns, double risk_free_rate) {
    if (returns.empty()) return 0.0;
    
    double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    double volatility = calculate_volatility(returns);
    
    return volatility > 0 ? (mean_return - risk_free_rate) / volatility : 0.0;
}

double RiskCalculator::calculate_volatility(const std::vector<Price>& returns) {
    if (returns.size() < 2) return 0.0;
    
    double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    double variance = 0.0;
    
    for (Price ret : returns) {
        variance += (ret - mean) * (ret - mean);
    }
    
    variance /= (returns.size() - 1);
    return std::sqrt(variance);
}

} // namespace trading
} // namespace vfx