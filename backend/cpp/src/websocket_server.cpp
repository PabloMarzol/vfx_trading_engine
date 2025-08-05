#include "websocket_server.h"
#include "types.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

#include <iostream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <iomanip>
#include <random>

namespace vfx {
namespace trading {

// Simple SHA1 for WebSocket handshake
class SimpleSHA1 {
private:
    uint32_t h[5];
    uint8_t buffer[64];
    uint64_t total_len;
    size_t buffer_len;
    
    uint32_t rotleft(uint32_t value, unsigned int amount) {
        return (value << amount) | (value >> (32 - amount));
    }
    
    void process_block() {
        uint32_t w[80];
        
        for (int i = 0; i < 16; i++) {
            w[i] = (buffer[i * 4] << 24) | (buffer[i * 4 + 1] << 16) | 
                   (buffer[i * 4 + 2] << 8) | buffer[i * 4 + 3];
        }
        
        for (int i = 16; i < 80; i++) {
            w[i] = rotleft(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
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
            
            uint32_t temp = rotleft(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rotleft(b, 30); b = a; a = temp;
        }
        
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
    
public:
    SimpleSHA1() {
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

// Simplified WebSocket Server
class WebSocketServer::RealWebSocketServer {
private:
    SOCKET server_socket_;
    bool running_;
    std::thread accept_thread_;
    std::vector<SOCKET> client_sockets_;
    std::mutex clients_mutex_;
    uint16_t port_;
    
public:
    RealWebSocketServer(uint16_t port) : server_socket_(INVALID_SOCKET), running_(false), port_(port) {
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
        if (server_socket_ != INVALID_SOCKET) {
            closesocket(server_socket_);
        }
#ifdef _WIN32
        WSACleanup();
#endif
    }
    
    bool start() {
        if (running_) return true;
        
        if (listen(server_socket_, SOMAXCONN) == SOCKET_ERROR) {
            std::cout << "❌ Listen failed" << std::endl;
            return false;
        }
        
        running_ = true;
        accept_thread_ = std::thread(&RealWebSocketServer::accept_clients, this);
        
        std::cout << "🚀 WebSocket Server started on port " << port_ << std::endl;
        return true;
    }
    
    void stop() {
        if (!running_) return;
        running_ = false;
        
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            for (SOCKET sock : client_sockets_) {
                closesocket(sock);
            }
            client_sockets_.clear();
        }
        
        if (accept_thread_.joinable()) {
            accept_thread_.join();
        }
        std::cout << "⏹️ WebSocket Server stopped" << std::endl;
    }
    
private:
    void accept_clients() {
        while (running_) {
            SOCKET client_socket = accept(server_socket_, nullptr, nullptr);
            if (client_socket == INVALID_SOCKET) continue;
            
            std::cout << "🔌 New WebSocket connection!" << std::endl;
            
            {
                std::lock_guard<std::mutex> lock(clients_mutex_);
                client_sockets_.push_back(client_socket);
            }
            
            std::thread(&RealWebSocketServer::handle_client, this, client_socket).detach();
        }
    }
    
    void handle_client(SOCKET client_socket) {
        try {
            if (!perform_handshake(client_socket)) {
                closesocket(client_socket);
                return;
            }
            
            // Send welcome
            send_message(client_socket, "{\"type\":\"connected\",\"message\":\"VFX Engine Ready\"}");
            
            char buffer[4096];
            while (running_) {
                int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
                if (bytes_received <= 0) break;
                
                std::string message = decode_websocket_frame(buffer, bytes_received);
                if (!message.empty()) {
                    handle_message(client_socket, message);
                }
            }
        } catch (...) {
            // Silent error handling
        }
        
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            client_sockets_.erase(
                std::remove(client_sockets_.begin(), client_sockets_.end(), client_socket),
                client_sockets_.end()
            );
        }
        closesocket(client_socket);
    }
    
    bool perform_handshake(SOCKET client_socket) {
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
        return send(client_socket, response_str.c_str(), response_str.length(), 0) != SOCKET_ERROR;
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
            payload += (char)(data[header_size + 4 + i] ^ mask[i % 4]);
        }
        return payload;
    }
    
    void send_message(SOCKET client_socket, const std::string& message) {
        std::vector<unsigned char> frame;
        frame.push_back(0x81); // Text frame
        
        if (message.length() < 126) {
            frame.push_back((unsigned char)message.length());
        } else if (message.length() < 65536) {
            frame.push_back(126);
            frame.push_back((unsigned char)(message.length() >> 8));
            frame.push_back((unsigned char)(message.length() & 0xFF));
        }
        
        for (char c : message) {
            frame.push_back((unsigned char)c);
        }
        
        send(client_socket, (char*)frame.data(), frame.size(), 0);
    }
    
    void handle_message(SOCKET client_socket, const std::string& message) {
        if (message.find("\"type\":\"order\"") != std::string::npos) {
            handle_order(client_socket, message);
        } else if (message.find("\"type\":\"ping\"") != std::string::npos) {
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()).count();
            send_message(client_socket, "{\"type\":\"pong\",\"timestamp\":" + std::to_string(now) + "}");
        } else if (message.find("\"type\":\"subscribe\"") != std::string::npos) {
            send_message(client_socket, "{\"type\":\"subscribed\",\"symbols\":[\"BTC/USD\",\"ETH/USD\",\"AAPL\"]}");
        }
    }
    
    void handle_order(SOCKET client_socket, const std::string& message) {
        // Simple order processing
        static uint64_t order_id = 1000;
        order_id++;
        
        // Basic parsing
        std::string symbol = "BTC/USD";
        std::string side = "BUY";
        double quantity = 1.0;
        double price = 68450.0;
        
        if (message.find("SELL") != std::string::npos) side = "SELL";
        if (message.find("ETH") != std::string::npos) { symbol = "ETH/USD"; price = 3892.0; }
        if (message.find("AAPL") != std::string::npos) { symbol = "AAPL"; price = 175.0; }
        
        // Send confirmation
        std::ostringstream response;
        response << "{\"type\":\"order_confirmation\""
                 << ",\"order_id\":" << order_id
                 << ",\"symbol\":\"" << symbol << "\""
                 << ",\"side\":\"" << side << "\""
                 << ",\"status\":\"submitted\"}";
        
        send_message(client_socket, response.str());
        
        // Simulate execution
        std::thread([=]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            std::ostringstream exec;
            exec << "{\"type\":\"execution\""
                 << ",\"order_id\":" << order_id
                 << ",\"symbol\":\"" << symbol << "\""
                 << ",\"side\":\"" << side << "\""
                 << ",\"quantity\":" << quantity
                 << ",\"price\":" << price << "}";
            
            send_message(client_socket, exec.str());
        }).detach();
        
        std::cout << "✅ Order " << order_id << " processed: " << side << " " << symbol << std::endl;
    }
};

// WebSocketServer implementation
WebSocketServer::WebSocketServer(const std::string& host, uint16_t port)
    : host_(host), port_(port) {
    real_server_ = std::make_unique<RealWebSocketServer>(port);
}

WebSocketServer::~WebSocketServer() {
    stop();
}

bool WebSocketServer::start() {
    if (running_) return true;
    running_ = true;
    return real_server_ ? real_server_->start() : false;
}

void WebSocketServer::stop() {
    if (!running_) return;
    running_ = false;
    if (real_server_) {
        real_server_->stop();
    }
}

// Stub implementations
void WebSocketServer::broadcast_market_data(const MarketTick&) {}
void WebSocketServer::broadcast_execution(const Execution&) {}
void WebSocketServer::broadcast_order_update(const Order&) {}
void WebSocketServer::send_to_client(const std::string&, const WSMessage&) {}
std::vector<std::string> WebSocketServer::get_connected_clients() const { return {}; }
bool WebSocketServer::is_client_connected(const std::string&) const { return false; }
void WebSocketServer::disconnect_client(const std::string&) {}

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