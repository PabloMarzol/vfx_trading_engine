#pragma once

#include <types.h>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <functional>

namespace vfx {
namespace trading {

// Forward declarations
class OrderManager;
class MarketDataManager;
class RiskManager;
struct Order;
struct Execution;
struct MarketTick;

struct WSMessage {
    std::string type;
    std::string data;
    std::chrono::high_resolution_clock::time_point timestamp;
};

class WebSocketServer {
public:
    struct ServerStats {
        size_t total_connections = 0;
        size_t active_connections = 0;
        size_t messages_processed = 0;
        double avg_latency_ms = 0.0;
        std::chrono::high_resolution_clock::time_point server_start_time;
    };

private:
    std::string host_;
    uint16_t port_;
    std::atomic<bool> running_{false};
    
    // Trading components
    std::shared_ptr<OrderManager> order_manager_;
    std::shared_ptr<MarketDataManager> market_data_;
    std::shared_ptr<RiskManager> risk_manager_;
    
    // Server implementation
    class RealWebSocketServer;
    std::unique_ptr<RealWebSocketServer> real_server_;

public:
    WebSocketServer(const std::string& host = "localhost", uint16_t port = 8080);
    ~WebSocketServer();

    // Core server operations
    bool start();
    void stop();
    bool is_running() const { return running_; }

    // Component setters
    void set_order_manager(std::shared_ptr<OrderManager> om) { order_manager_ = om; }
    void set_market_data(std::shared_ptr<MarketDataManager> md) { market_data_ = md; }
    void set_risk_manager(std::shared_ptr<RiskManager> rm) { risk_manager_ = rm; }

    // Broadcasting methods
    void broadcast_market_data(const MarketTick& tick);
    void broadcast_execution(const Execution& execution);
    void broadcast_order_update(const Order& order);

    // Client management
    void send_to_client(const std::string& client_id, const WSMessage& message);
    std::vector<std::string> get_connected_clients() const;
    bool is_client_connected(const std::string& client_id) const;
    void disconnect_client(const std::string& client_id);

    // Statistics
    ServerStats get_statistics() const;
    
    // Getters
    std::string get_host() const { return host_; }
    uint16_t get_port() const { return port_; }
};

} // namespace trading
} // namespace vfx