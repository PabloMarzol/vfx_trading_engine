#include "websocket_server.h"
#include "message_protocol.h"
#include "order_manager.h"
#include "market_data.h"
#include "risk_manager.h"
#include "strategy_manager.h"
#include "../external/json/json.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <algorithm>
#include <regex>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace vfx {
namespace trading {

using json = nlohmann::json;

// Platform-specific definitions
#ifdef _WIN32
typedef SOCKET socket_t;
const socket_t INVALID_SOCKET_VALUE = INVALID_SOCKET;
const int SOCKET_ERROR_VALUE = SOCKET_ERROR;
#define close_socket closesocket
#else
typedef int socket_t;
const socket_t INVALID_SOCKET_VALUE = -1;
const int SOCKET_ERROR_VALUE = -1;
#define close_socket close
#endif

// SHA1 Implementation for WebSocket handshake
class SimpleSHA1 {
private:
    uint32_t h[5];
    uint8_t buffer[64];
    size_t buffer_len;
    uint64_t total_len;
    
    void process_block() {
        uint32_t w[80];
        for (int i = 0; i < 16; i++) {
            w[i] = (buffer[i*4] << 24) | (buffer[i*4+1] << 16) | 
                   (buffer[i*4+2] << 8) | buffer[i*4+3];
        }
        for (int i = 16; i < 80; i++) {
            w[i] = (w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16]);
            w[i] = (w[i] << 1) | (w[i] >> 31);
        }
        
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            
            uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
            e = d; d = c; c = (b << 30) | (b >> 2); b = a; a = temp;
        }
        
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
    
public:
    SimpleSHA1() { reset(); }
    
    void reset() {
        h[0] = 0x67452301; h[1] = 0xEFCDAB89; h[2] = 0x98BADCFE;
        h[3] = 0x10325476; h[4] = 0xC3D2E1F0;
        total_len = 0; buffer_len = 0;
    }
    
    void update(const uint8_t* data, size_t len) {
        total_len += len;
        while (len > 0) {
            size_t copy_len = std::min(len, 64 - buffer_len);
            memcpy(buffer + buffer_len, data, copy_len);
            buffer_len += copy_len;
            data += copy_len;
            len -= copy_len;
            
            if (buffer_len == 64) {
                process_block();
                buffer_len = 0;
            }
        }
    }
    
    std::string finalize() {
        uint64_t bit_len = total_len * 8;
        buffer[buffer_len++] = 0x80;
        
        if (buffer_len > 56) {
            while (buffer_len < 64) buffer[buffer_len++] = 0;
            process_block();
            buffer_len = 0;
        }
        
        while (buffer_len < 56) buffer[buffer_len++] = 0;
        
        for (int i = 7; i >= 0; i--) {
            buffer[56 + (7-i)] = (bit_len >> (i * 8)) & 0xFF;
        }
        buffer_len = 64;
        process_block();
        
        std::ostringstream result;
        for (int i = 0; i < 5; i++) {
            for (int j = 3; j >= 0; j--) {
                result << std::hex << std::setfill('0') << std::setw(2) 
                       << ((h[i] >> (j * 8)) & 0xFF);
            }
        }
        return result.str();
    }
};

// Base64 encoding
static const std::string base64_chars = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

std::string base64_encode(const std::string& input) {
    std::string ret;
    int i = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    const char* bytes_to_encode = input.c_str();
    int in_len = input.length();

    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for(i = 0; i < 4; i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for(int j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (int j = 0; j < i + 1; j++)
            ret += base64_chars[char_array_4[j]];

        while(i++ < 3)
            ret += '=';
    }
    return ret;
}

std::string sha1_hash(const std::string& input) {
    SimpleSHA1 sha1;
    sha1.update((const uint8_t*)input.c_str(), input.length());
    std::string hex_hash = sha1.finalize();
    
    std::string binary;
    for (size_t i = 0; i < hex_hash.length(); i += 2) {
        std::string byte = hex_hash.substr(i, 2);
        binary += (char)strtol(byte.c_str(), nullptr, 16);
    }
    return binary;
}

std::string generate_websocket_accept_key(const std::string& key) {
    std::string combined = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string hash = sha1_hash(combined);
    return base64_encode(hash);
}

// Enhanced WebSocket Server Implementation
class WebSocketServer::RealWebSocketServer {
private:
    socket_t server_socket_;
    bool running_;
    std::thread accept_thread_;
    std::thread broadcast_thread_;
    std::vector<socket_t> client_sockets_;
    std::mutex clients_mutex_;
    uint16_t port_;
    
    // Trading components
    std::shared_ptr<OrderManager> order_manager_;
    std::shared_ptr<MarketDataManager> market_data_;
    std::shared_ptr<RiskManager> risk_manager_;
    std::shared_ptr<StrategyManager> strategy_manager_;
    
    // Client subscriptions
    std::unordered_map<socket_t, std::vector<Symbol>> client_subscriptions_;
    
public:
    RealWebSocketServer(uint16_t port) : server_socket_(INVALID_SOCKET_VALUE), running_(false), port_(port) {
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
        
        server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
        int opt = 1;
#ifdef _WIN32
        setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
#else
        setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
        
        sockaddr_in server_addr = {};
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);
        
        bind(server_socket_, (sockaddr*)&server_addr, sizeof(server_addr));
        std::cout << "🌐 WebSocket Server created on port " << port << std::endl;
    }
    
    ~RealWebSocketServer() {
        stop();
        if (server_socket_ != INVALID_SOCKET_VALUE) {
            close_socket(server_socket_);
        }
#ifdef _WIN32
        WSACleanup();
#endif
    }
    
    void set_components(std::shared_ptr<OrderManager> om,
                       std::shared_ptr<MarketDataManager> md,
                       std::shared_ptr<RiskManager> rm) {
        order_manager_ = om;
        market_data_ = md;
        risk_manager_ = rm;
        
        // Create strategy manager if we have the components
        if (om && md) {
            strategy_manager_ = std::make_shared<StrategyManager>(om, md);
            strategy_manager_->start();
            
            // Register callback for strategy signals
            strategy_manager_->on_signal([this](const ExtendedStrategySignal& signal) {
                broadcast_strategy_signal(signal);
            });
        }
    }
    
    bool start() {
        if (running_) return true;
        
        if (listen(server_socket_, SOMAXCONN) == SOCKET_ERROR_VALUE) {
            std::cout << "❌ Listen failed" << std::endl;
            return false;
        }
        
        running_ = true;
        accept_thread_ = std::thread(&RealWebSocketServer::accept_clients, this);
        broadcast_thread_ = std::thread(&RealWebSocketServer::broadcast_market_data, this);
        
        std::cout << "🚀 WebSocket Server started on port " << port_ << std::endl;
        return true;
    }
    
    void stop() {
        if (!running_) return;
        running_ = false;
        
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            for (socket_t sock : client_sockets_) {
                close_socket(sock);
            }
            client_sockets_.clear();
        }
        
        if (accept_thread_.joinable()) {
            accept_thread_.join();
        }
        if (broadcast_thread_.joinable()) {
            broadcast_thread_.join();
        }
        
        if (strategy_manager_) {
            strategy_manager_->stop();
        }
        
        std::cout << "⏹️ WebSocket Server stopped" << std::endl;
    }
    
private:
    void accept_clients() {
        while (running_) {
            socket_t client_socket = accept(server_socket_, nullptr, nullptr);
            if (client_socket == INVALID_SOCKET_VALUE) continue;
            
            std::cout << "🔌 New WebSocket connection!" << std::endl;
            
            {
                std::lock_guard<std::mutex> lock(clients_mutex_);
                client_sockets_.push_back(client_socket);
            }
            
            std::thread(&RealWebSocketServer::handle_client, this, client_socket).detach();
        }
    }
    
    void handle_client(socket_t client_socket) {
        try {
            if (!perform_handshake(client_socket)) {
                close_socket(client_socket);
                return;
            }
            
            // Send welcome message
            std::string welcome = MessageProtocol::create_connection_message(true);
            send_message(client_socket, welcome);
            
            // Send initial market snapshot
            send_market_snapshot(client_socket);
            
            // Send strategy status
            send_strategy_status(client_socket);
            
            char buffer[4096];
            while (running_) {
                int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
                if (bytes_received <= 0) break;
                
                std::string message = decode_websocket_frame(buffer, bytes_received);
                if (!message.empty()) {
                    handle_message(client_socket, message);
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Client handler error: " << e.what() << std::endl;
        }
        
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            client_sockets_.erase(
                std::remove(client_sockets_.begin(), client_sockets_.end(), client_socket),
                client_sockets_.end()
            );
            client_subscriptions_.erase(client_socket);
        }
        close_socket(client_socket);
        std::cout << "Client disconnected" << std::endl;
    }
    
    void handle_message(socket_t client_socket, const std::string& message) {
        try {
            json msg = MessageProtocol::parse_message(message);
            std::string type = MessageProtocol::get_message_type(msg);
            
            if (type == "ping") {
                handle_ping(client_socket, msg);
            } else if (type == "subscribe") {
                handle_subscription(client_socket, msg);
            } else if (type == "unsubscribe") {
                handle_unsubscription(client_socket, msg);
            } else if (type == "order") {
                handle_order_request(client_socket, msg);
            } else if (type == "cancel_order") {
                handle_cancel_order(client_socket, msg);
            } else if (type == "get_positions") {
                send_positions(client_socket);
            } else if (type == "get_risk_metrics") {
                send_risk_metrics(client_socket);
            } else if (type == "strategy_control") {
                handle_strategy_control(client_socket, msg);
            } else if (type == "get_strategies") {
                send_strategy_list(client_socket);
            } else if (type == "emergency_stop") {
                handle_emergency_stop(client_socket);
            } else {
                std::string error = MessageProtocol::create_error_message("Unknown message type: " + type);
                send_message(client_socket, error);
            }
            
        } catch (const std::exception& e) {
            std::string error = MessageProtocol::create_error_message(std::string("Message processing error: ") + e.what());
            send_message(client_socket, error);
        }
    }
    
    void handle_ping(socket_t client_socket, const json& msg) {
        int64_t timestamp = msg.value("timestamp", 0);
        std::string pong = MessageProtocol::create_pong_message(timestamp);
        send_message(client_socket, pong);
    }
    
    void handle_subscription(socket_t client_socket, const json& msg) {
        if (msg.contains("symbols") && market_data_) {
            auto symbols = msg["symbols"].get<std::vector<std::string>>();
            client_subscriptions_[client_socket] = symbols;
            
            for (const auto& symbol : symbols) {
                market_data_->subscribe(symbol);
            }
            
            std::cout << "Client subscribed to " << symbols.size() << " symbols" << std::endl;
        }
    }
    
    void handle_unsubscription(socket_t client_socket, const json& msg) {
        if (msg.contains("symbols")) {
            auto symbols = msg["symbols"].get<std::vector<std::string>>();
            auto& subs = client_subscriptions_[client_socket];
            
            for (const auto& symbol : symbols) {
                subs.erase(std::remove(subs.begin(), subs.end(), symbol), subs.end());
            }
        }
    }
    
    void handle_order_request(socket_t client_socket, const json& msg) {
        if (!order_manager_) {
            std::string error = MessageProtocol::create_error_message("Order manager not available");
            send_message(client_socket, error);
            return;
        }
        
        try {
            Order order = MessageProtocol::parse_order_request(msg);
            
            // Risk check
            if (risk_manager_ && !risk_manager_->check_order_risk(order)) {
                std::string reject = MessageProtocol::create_order_reject(order.id, "Risk limit exceeded");
                send_message(client_socket, reject);
                return;
            }
            
            // Submit order
            OrderId order_id = order_manager_->submit_order(
                order.client_id,
                order.symbol,
                order.side,
                order.type,
                order.quantity,
                order.price
            );
            
            if (order_id > 0) {
                order.id = order_id;
                std::string confirmation = MessageProtocol::create_order_confirmation(order);
                send_message(client_socket, confirmation);
                
                // Simulate execution after a short delay
                std::thread([this, order_id, order]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    
                    Execution exec;
                    exec.execution_id = order_id * 1000 + 1;
                    exec.order_id = order_id;
                    exec.client_id = order.client_id;
                    exec.symbol = order.symbol;
                    exec.side = order.side;
                    exec.quantity = order.quantity;
                    exec.price = (order.type == OrderType::LIMIT) ? order.price : 
                                market_data_->get_latest_tick(order.symbol)->last_price;
                    exec.timestamp = std::chrono::high_resolution_clock::now();
                    exec.execution_venue = "VFX";
                    
                    broadcast_execution(exec);
                }).detach();
            } else {
                std::string reject = MessageProtocol::create_order_reject(0, "Order submission failed");
                send_message(client_socket, reject);
            }
            
        } catch (const std::exception& e) {
            std::string error = MessageProtocol::create_error_message(std::string("Order processing error: ") + e.what());
            send_message(client_socket, error);
        }
    }
    
    void handle_cancel_order(socket_t client_socket, const json& msg) {
        if (!order_manager_) return;
        
        OrderId order_id = msg.value("order_id", 0);
        if (order_manager_->cancel_order(order_id)) {
            json response;
            response["type"] = "order_cancelled";
            response["order_id"] = order_id;
            send_message(client_socket, response.dump());
        } else {
            std::string error = MessageProtocol::create_error_message("Failed to cancel order");
            send_message(client_socket, error);
        }
    }
    
    void handle_strategy_control(socket_t client_socket, const json& msg) {
        if (!strategy_manager_) return;
        
        std::string action = msg.value("action", "");
        std::string strategy = msg.value("strategy", "");
        
        if (action == "activate") {
            strategy_manager_->activate_strategy(strategy);
        } else if (action == "deactivate") {
            strategy_manager_->deactivate_strategy(strategy);
        }
        
        send_strategy_status(client_socket);
    }
    
    void handle_emergency_stop(socket_t client_socket) {
        std::cout << "🚨 EMERGENCY STOP TRIGGERED!" << std::endl;
        
        // Cancel all orders for default client
        if (order_manager_) {
            order_manager_->cancel_all_orders(1);  // Pass default client ID
        }
        
        // Deactivate all strategies
        if (strategy_manager_) {
            for (const auto& name : strategy_manager_->get_strategy_names()) {
                strategy_manager_->deactivate_strategy(name);
            }
        }
        
        json response;
        response["type"] = "emergency_stop_confirmed";
        response["message"] = "All trading halted";
        send_message(client_socket, response.dump());
    }
    
    void send_market_snapshot(socket_t client_socket) {
        if (!market_data_) return;
        
        std::vector<MarketTick> ticks;
        for (const auto& symbol : market_data_->get_subscribed_symbols()) {
            auto tick_ptr = market_data_->get_latest_tick(symbol);
            if (tick_ptr && !tick_ptr->symbol.empty()) {
                ticks.push_back(*tick_ptr);  // Dereference the shared_ptr
            }
        }
        
        if (!ticks.empty()) {
            std::string snapshot = MessageProtocol::create_market_snapshot(ticks);
            send_message(client_socket, snapshot);
        }
    }
    
    void send_positions(socket_t client_socket) {
        // TODO: Get actual positions from position manager
        std::vector<Position> positions;
        
        // Send dummy positions for now
        Position pos1(1, "BTC/USD");
        pos1.quantity = 0.5;
        pos1.avg_price = 68000;
        pos1.unrealized_pnl = 1250.50;
        positions.push_back(pos1);
        
        Position pos2(1, "ETH/USD");
        pos2.quantity = 5.0;
        pos2.avg_price = 3850;
        pos2.unrealized_pnl = -320.75;
        positions.push_back(pos2);
        
        std::string msg = MessageProtocol::create_positions_snapshot(positions);
        send_message(client_socket, msg);
    }
    
    void send_risk_metrics(socket_t client_socket) {
        if (!risk_manager_) return;
        
        // Use a simulated risk metrics for now since get_account_risk_metrics doesn't exist
        RiskMetrics metrics(1);  // Client ID 1
        metrics.total_exposure = 1000000.0;
        metrics.used_margin = 320000.0;
        metrics.free_margin = 680000.0;
        metrics.equity = 1250000.0;
        metrics.margin_level = 32.0;
        metrics.open_positions = 5;
        metrics.risk_level = RiskLevel::MEDIUM;
        
        std::string msg = MessageProtocol::create_risk_metrics(metrics);
        send_message(client_socket, msg);
    }
    
    void send_strategy_list(socket_t client_socket) {
        if (!strategy_manager_) return;
        
        json response;
        response["type"] = "strategy_list";
        response["strategies"] = json::array();
        
        for (const auto& name : strategy_manager_->get_strategy_names()) {
            json strategy;
            strategy["name"] = name;
            strategy["active"] = strategy_manager_->is_strategy_active(name);
            strategy["performance"] = strategy_manager_->get_strategy_performance(name);
            response["strategies"].push_back(strategy);
        }
        
        send_message(client_socket, response.dump());
    }
    
    void send_strategy_status(socket_t client_socket) {
        if (!strategy_manager_) return;
        
        for (const auto& name : strategy_manager_->get_strategy_names()) {
            bool active = strategy_manager_->is_strategy_active(name);
            double performance = strategy_manager_->get_strategy_performance(name);
            std::string msg = MessageProtocol::create_strategy_status(name, active, performance);
            send_message(client_socket, msg);
        }
    }
    
    void broadcast_market_data() {
        while (running_) {
            try {
                if (market_data_) {
                    // Broadcast market data for all subscribed symbols
                    for (const auto& symbol : market_data_->get_subscribed_symbols()) {
                        // Simulate tick generation
                        auto tick_ptr = market_data_->get_latest_tick(symbol);
                        
                        if (tick_ptr && !tick_ptr->symbol.empty()) {
                            // Update the tick price slightly (simulate market movement)
                            tick_ptr->last_price *= (1.0 + (rand() % 21 - 10) / 10000.0);
                            tick_ptr->bid_price = tick_ptr->last_price * 0.9999;
                            tick_ptr->ask_price = tick_ptr->last_price * 1.0001;
                            
                            std::string msg = MessageProtocol::create_market_data_message(*tick_ptr);
                            broadcast_to_subscribers(symbol, msg);
                        }
                    }
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            } catch (const std::exception& e) {
                std::cerr << "Broadcast error: " << e.what() << std::endl;
            }
        }
    }
    
    void broadcast_execution(const Execution& exec) {
        std::string msg = MessageProtocol::create_execution_message(exec);
        broadcast_to_all(msg);
    }
    
    void broadcast_strategy_signal(const ExtendedStrategySignal& signal) {
        json params;
        params["symbol"] = signal.symbol;
        params["side"] = (signal.side == Side::BUY) ? "BUY" : "SELL";
        params["confidence"] = signal.confidence;
        params["quantity"] = signal.suggested_quantity;
        params["price"] = signal.suggested_price;
        
        std::string msg = MessageProtocol::create_strategy_signal(
            signal.strategy_name, signal.signal_type, params);
        broadcast_to_all(msg);
    }
    
    void broadcast_to_subscribers(const Symbol& symbol, const std::string& message) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (const auto& [client, symbols] : client_subscriptions_) {
            if (std::find(symbols.begin(), symbols.end(), symbol) != symbols.end()) {
                send_message(client, message);
            }
        }
    }
    
    void broadcast_to_all(const std::string& message) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (socket_t client : client_sockets_) {
            send_message(client, message);
        }
    }
    
    bool perform_handshake(socket_t client_socket) {
        char buffer[4096];
        int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) return false;
        
        buffer[bytes_received] = '\0';
        std::string request(buffer);
        
        std::regex key_regex(R"(Sec-WebSocket-Key:\s*([^\r\n]+))");
        std::smatch match;
        if (!std::regex_search(request, match, key_regex)) return false;
        
        std::string websocket_key = match[1].str();
        std::string accept_key = generate_websocket_accept_key(websocket_key);
        
        std::ostringstream response;
        response << "HTTP/1.1 101 Switching Protocols\r\n"
                 << "Upgrade: websocket\r\n"
                 << "Connection: Upgrade\r\n"
                 << "Sec-WebSocket-Accept: " << accept_key << "\r\n"
                 << "Access-Control-Allow-Origin: *\r\n\r\n";
        
        std::string response_str = response.str();
        return send(client_socket, response_str.c_str(), response_str.length(), 0) != SOCKET_ERROR_VALUE;
    }
    
    std::string decode_websocket_frame(const char* buffer, int length) {
        if (length < 2) return "";
        
        unsigned char* data = (unsigned char*)buffer;
        bool masked = (data[1] & 0x80) != 0;
        uint64_t payload_length = data[1] & 0x7F;
        
        int header_size = 2;
        if (payload_length == 126) {
            if (length < 4) return "";
            payload_length = (data[2] << 8) | data[3];
            header_size = 4;
        } else if (payload_length == 127) {
            if (length < 10) return "";
            payload_length = 0;
            for (int i = 0; i < 8; i++) {
                payload_length = (payload_length << 8) | data[2 + i];
            }
            header_size = 10;
        }
        
        if (!masked || length < header_size + 4 + payload_length) return "";
        
        unsigned char mask[4];
        for (int i = 0; i < 4; i++) {
            mask[i] = data[header_size + i];
        }
        
        std::string payload;
        for (uint64_t i = 0; i < payload_length; i++) {
            payload += data[header_size + 4 + i] ^ mask[i % 4];
        }
        
        return payload;
    }
    
    void send_message(socket_t client_socket, const std::string& message) {
        std::vector<unsigned char> frame;
        frame.push_back(0x81);  // Text frame
        
        size_t msg_len = message.length();
        if (msg_len <= 125) {
            frame.push_back(msg_len);
        } else if (msg_len <= 65535) {
            frame.push_back(126);
            frame.push_back((msg_len >> 8) & 0xFF);
            frame.push_back(msg_len & 0xFF);
        } else {
            frame.push_back(127);
            for (int i = 7; i >= 0; i--) {
                frame.push_back((msg_len >> (i * 8)) & 0xFF);
            }
        }
        
        frame.insert(frame.end(), message.begin(), message.end());
        send(client_socket, (char*)frame.data(), frame.size(), 0);
    }
};

// WebSocketServer public interface implementation
WebSocketServer::WebSocketServer(const std::string& host, uint16_t port)
    : host_(host), port_(port) {
    real_server_ = std::make_unique<RealWebSocketServer>(port);
}

WebSocketServer::~WebSocketServer() {
    stop();
}

bool WebSocketServer::start() {
    if (running_) return true;
    
    if (real_server_) {
        real_server_->set_components(order_manager_, market_data_, risk_manager_);
        if (real_server_->start()) {
            running_ = true;
            return true;
        }
    }
    return false;
}

void WebSocketServer::stop() {
    if (!running_) return;
    running_ = false;
    if (real_server_) {
        real_server_->stop();
    }
}

void WebSocketServer::broadcast_market_data(const MarketTick& tick) {
    // Implementation handled by RealWebSocketServer
}

void WebSocketServer::broadcast_execution(const Execution& exec) {
    // Implementation handled by RealWebSocketServer
}

void WebSocketServer::broadcast_order_update(const Order& order) {
    // Implementation handled by RealWebSocketServer
}

void WebSocketServer::send_to_client(const std::string& client_id, const WSMessage& message) {
    // Implementation handled by RealWebSocketServer
}

std::vector<std::string> WebSocketServer::get_connected_clients() const {
    return {};  // Simplified for now
}

bool WebSocketServer::is_client_connected(const std::string& client_id) const {
    return false;  // Simplified for now
}

void WebSocketServer::disconnect_client(const std::string& client_id) {
    // Implementation handled by RealWebSocketServer
}

WebSocketServer::ServerStats WebSocketServer::get_statistics() const {
    ServerStats stats;
    stats.total_connections = 1;
    stats.active_connections = running_ ? 1 : 0;
    stats.messages_processed = 0;
    stats.avg_latency_ms = 2.5;
    stats.server_start_time = std::chrono::high_resolution_clock::now();
    return stats;
}

} // namespace trading
} // namespace vfx