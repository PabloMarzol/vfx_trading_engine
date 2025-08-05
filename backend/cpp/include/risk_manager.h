#pragma once

#include "types.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <functional>
#include <string>
#include <cmath>

namespace vfx {
namespace trading {

// Forward declarations
class MarketDataManager;

// Risk limit types
struct RiskLimits {
    Price max_position_value = 1000000.0;      // Maximum position value per symbol
    Price max_daily_loss = 50000.0;            // Maximum daily loss per client
    Price max_total_exposure = 5000000.0;      // Maximum total exposure per client
    double max_leverage = 10.0;                // Maximum leverage ratio
    uint32_t max_orders_per_minute = 100;      // Rate limiting
    uint32_t max_open_positions = 50;          // Maximum open positions per client
    double margin_call_level = 100.0;          // Margin call at 100% margin level
    double stop_out_level = 50.0;              // Stop out at 50% margin level
    bool allow_hedging = true;                 // Allow hedged positions
    bool allow_short_selling = true;           // Allow short positions
    
    RiskLimits() = default;
};

// Position risk metrics
struct PositionRisk {
    ClientId client_id;
    Symbol symbol;
    Quantity position_size;
    Price avg_price;
    Price current_price;
    Price unrealized_pnl;
    Price market_value;
    double risk_percentage;
    RiskLevel risk_level;
    Timestamp last_calculated;
    
    PositionRisk() = default;
    PositionRisk(ClientId cid, const Symbol& sym) 
        : client_id(cid), symbol(sym), position_size(0), avg_price(0), 
          current_price(0), unrealized_pnl(0), market_value(0), 
          risk_percentage(0), risk_level(RiskLevel::LOW),
          last_calculated(std::chrono::high_resolution_clock::now()) {}
};

// Client risk profile
struct ClientRisk {
    ClientId client_id;
    Price total_equity;
    Price used_margin;
    Price free_margin;
    Price total_exposure;
    Price daily_pnl;
    Price unrealized_pnl;
    double margin_level;
    double leverage_ratio;
    RiskLevel overall_risk_level;
    uint32_t open_positions_count;
    uint32_t orders_last_minute;
    bool margin_call_triggered;
    bool stop_out_triggered;
    Timestamp last_calculated;
    std::vector<PositionRisk> position_risks;
    
    ClientRisk() = default;
    ClientRisk(ClientId cid) 
        : client_id(cid), total_equity(0), used_margin(0), free_margin(0),
          total_exposure(0), daily_pnl(0), unrealized_pnl(0), margin_level(0),
          leverage_ratio(0), overall_risk_level(RiskLevel::LOW),
          open_positions_count(0), orders_last_minute(0),
          margin_call_triggered(false), stop_out_triggered(false),
          last_calculated(std::chrono::high_resolution_clock::now()) {}
};

// Risk event types
enum class RiskEventType : uint8_t {
    LIMIT_EXCEEDED = 0,
    MARGIN_CALL = 1,
    STOP_OUT = 2,
    HIGH_LEVERAGE = 3,
    LARGE_POSITION = 4,
    RAPID_TRADING = 5,
    DAILY_LOSS_LIMIT = 6
};

struct RiskEvent {
    RiskEventType type;
    ClientId client_id;
    Symbol symbol;
    std::string description;
    RiskLevel severity;
    Timestamp occurred_at;
    bool resolved;
    
    RiskEvent(RiskEventType t, ClientId cid, const std::string& desc, RiskLevel sev)
        : type(t), client_id(cid), description(desc), severity(sev),
          occurred_at(std::chrono::high_resolution_clock::now()), resolved(false) {}
};

// Risk callbacks
using RiskEventCallback = std::function<void(const RiskEvent&)>;
using MarginCallCallback = std::function<void(ClientId, const ClientRisk&)>;
using StopOutCallback = std::function<void(ClientId, const ClientRisk&)>;

class RiskManager {
private:
    // Risk data
    std::unordered_map<ClientId, RiskLimits> client_limits_;
    std::unordered_map<ClientId, ClientRisk> client_risks_;
    std::unordered_map<ClientId, std::unordered_map<Symbol, Position>> positions_;
    std::vector<RiskEvent> risk_events_;
    
    // Thread safety
    mutable std::shared_mutex limits_mutex_;
    mutable std::shared_mutex risks_mutex_;
    mutable std::shared_mutex positions_mutex_;
    mutable std::shared_mutex events_mutex_;
    
    // Dependencies
    std::shared_ptr<MarketDataManager> market_data_;
    
    // Configuration
    RiskLimits default_limits_;
    bool risk_checks_enabled_{true};
    
    // Callbacks
    RiskEventCallback risk_event_callback_;
    MarginCallCallback margin_call_callback_;
    StopOutCallback stop_out_callback_;
    
    // Performance tracking
    mutable std::atomic<uint64_t> total_risk_checks_{0};
    mutable std::atomic<uint64_t> risk_violations_{0};
    
public:
    RiskManager(std::shared_ptr<MarketDataManager> market_data);
    ~RiskManager();
    
    // Configuration
    void set_default_limits(const RiskLimits& limits) { default_limits_ = limits; }
    const RiskLimits& get_default_limits() const { return default_limits_; }
    
    void set_client_limits(ClientId client_id, const RiskLimits& limits);
    RiskLimits get_client_limits(ClientId client_id) const;
    
    void enable_risk_checks(bool enabled) { risk_checks_enabled_ = enabled; }
    bool are_risk_checks_enabled() const { return risk_checks_enabled_; }
    
    // Pre-trade risk checks
    bool check_order_risk(const Order& order) const;
    bool can_open_position(ClientId client_id, const Symbol& symbol, 
                          Side side, Quantity quantity, Price price) const;
    bool check_leverage_limit(ClientId client_id, Price additional_exposure) const;
    bool check_position_size_limit(ClientId client_id, const Symbol& symbol, 
                                  Quantity new_quantity) const;
    
    // Post-trade updates
    void update_position(const Execution& execution);
    void update_client_equity(ClientId client_id, Price new_equity);
    void process_end_of_day(ClientId client_id);
    
    // Risk monitoring
    void calculate_all_risks();
    void calculate_client_risk(ClientId client_id);
    ClientRisk get_client_risk(ClientId client_id) const;
    std::vector<ClientRisk> get_high_risk_clients() const;
    
    // Position management
    void add_position(ClientId client_id, const Position& position);
    void remove_position(ClientId client_id, const Symbol& symbol);
    Position get_position(ClientId client_id, const Symbol& symbol) const;
    std::vector<Position> get_client_positions(ClientId client_id) const;
    
    // Risk events
    std::vector<RiskEvent> get_recent_events(uint32_t count = 100) const;
    std::vector<RiskEvent> get_client_events(ClientId client_id) const;
    void resolve_event(size_t event_index);
    
    // Emergency functions
    void force_close_position(ClientId client_id, const Symbol& symbol);
    void force_close_all_positions(ClientId client_id);
    void trigger_margin_call(ClientId client_id);
    void trigger_stop_out(ClientId client_id);
    
    // Callbacks
    void set_risk_event_callback(RiskEventCallback callback) { 
        risk_event_callback_ = callback; 
    }
    void set_margin_call_callback(MarginCallCallback callback) { 
        margin_call_callback_ = callback; 
    }
    void set_stop_out_callback(StopOutCallback callback) { 
        stop_out_callback_ = callback; 
    }
    
    // Statistics
    struct RiskStatistics {
        uint64_t total_risk_checks;
        uint64_t risk_violations;
        uint32_t active_clients;
        uint32_t high_risk_clients;
        uint32_t margin_calls_today;
        uint32_t stop_outs_today;
        double avg_leverage;
        double avg_margin_level;
    };
    
    RiskStatistics get_statistics() const;
    
private:
    // Internal calculations
    void calculate_position_risk(ClientId client_id, const Symbol& symbol);
    void calculate_margin_requirements(ClientId client_id);
    void check_margin_levels(ClientId client_id);
    void update_risk_level(ClientId client_id);
    
    // Risk validation helpers
    bool validate_order_limits(const Order& order) const;
    bool validate_position_limits(ClientId client_id, const Symbol& symbol, 
                                 Quantity additional_quantity) const;
    bool validate_exposure_limits(ClientId client_id, Price additional_exposure) const;
    
    // Position helpers
    void update_position_pnl(ClientId client_id, const Symbol& symbol);
    Price calculate_margin_requirement(const Position& position, Price current_price) const;
    Price calculate_unrealized_pnl(const Position& position, Price current_price) const;
    
    // Event handling
    void create_risk_event(RiskEventType type, ClientId client_id, 
                          const std::string& description, RiskLevel severity,
                          const Symbol& symbol = "");
    void notify_risk_event(const RiskEvent& event);
    void notify_margin_call(ClientId client_id, const ClientRisk& risk);
    void notify_stop_out(ClientId client_id, const ClientRisk& risk);
    
    // Utility functions
    RiskLevel calculate_risk_level(double margin_level, double leverage) const;
    Price get_current_price(const Symbol& symbol) const;
    double calculate_correlation_risk(ClientId client_id) const;
    
    // Threading utilities
    void ensure_client_risk_exists(ClientId client_id);
    RiskLimits get_effective_limits(ClientId client_id) const;
};

// Risk calculation utilities
class RiskCalculator {
public:
    static double calculate_var(const std::vector<Price>& returns, double confidence = 0.95);
    static double calculate_sharpe_ratio(const std::vector<Price>& returns, double risk_free_rate = 0.02);
    static double calculate_max_drawdown(const std::vector<Price>& equity_curve);
    static double calculate_correlation(const std::vector<Price>& returns1, const std::vector<Price>& returns2);
    static Price calculate_position_var(const Position& position, Price current_price, double volatility);
    
private:
    static std::vector<double> calculate_returns(const std::vector<Price>& prices);
    static double calculate_volatility(const std::vector<Price>& returns);
};

// Risk monitoring alerts
struct RiskAlert {
    enum class AlertType {
        APPROACHING_LIMIT,
        LIMIT_BREACHED,
        MARGIN_WARNING,
        HIGH_VOLATILITY,
        CONCENTRATION_RISK
    } type;
    
    ClientId client_id;
    Symbol symbol;
    std::string message;
    RiskLevel severity;
    double threshold_value;
    double current_value;
    Timestamp created_at;
    
    RiskAlert(AlertType t, ClientId cid, const std::string& msg, RiskLevel sev)
        : type(t), client_id(cid), message(msg), severity(sev),
          threshold_value(0), current_value(0),
          created_at(std::chrono::high_resolution_clock::now()) {}
};

} // namespace trading
} // namespace vfx