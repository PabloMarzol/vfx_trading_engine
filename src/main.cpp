
#include "trading_engine.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    std::cout << "VFX Trading Engine v1.0.0 Starting..." << std::endl;
    
    try {
        vfx::trading::TradingEngine engine;
        
        std::cout << "Engine initialized successfully" << std::endl;
        std::cout << "Trading engine running. Press Ctrl+C to stop." << std::endl;
        
        // Keep running
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
