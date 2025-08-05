#include "trading_engine.h"
#include "websocket_server.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <memory>

int main() {
    std::cout << "===============================================" << std::endl;
    std::cout << "  VFX Trading Engine - C++ Core v1.0.0" << std::endl;
    std::cout << "===============================================" << std::endl;
    std::cout << std::endl;

    try {
        // Create trading engine
        auto engine = std::make_shared<vfx::trading::TradingEngine>();
        engine->Start();

        // Create WebSocket server for frontend communication
        auto websocket_server = std::make_unique<vfx::trading::WebSocketServer>("localhost", 8080);
        
        // Start WebSocket server
        if (websocket_server->start()) {
            std::cout << "🌐 WebSocket server started on ws://localhost:8080" << std::endl;
            std::cout << "🎯 Frontend can now connect!" << std::endl;
        } else {
            std::cout << "❌ Failed to start WebSocket server" << std::endl;
        }

        std::cout << std::endl;
        std::cout << "🚀 VFX Trading Engine is ready!" << std::endl;
        std::cout << "📊 WebSocket server listening for frontend connections" << std::endl;
        std::cout << "💹 Open frontend/index.html to start trading" << std::endl;
        std::cout << std::endl;
        std::cout << "Press Enter to run demo or Ctrl+C to stop..." << std::endl;
        
        std::cin.get();

        // Run trading demo
        engine->RunDemo();

        std::cout << std::endl << "🎉 VFX Trading Engine working perfectly!" << std::endl;
        std::cout << "💻 WebSocket server still running for frontend..." << std::endl;
        std::cout << std::endl;
        
        // Keep server running
        std::cout << "🔄 Server running... Press Ctrl+C to stop" << std::endl;
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}