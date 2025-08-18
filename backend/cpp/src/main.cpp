#include "trading_engine.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    std::cout << "===============================================" << std::endl;
    std::cout << "  VFX Trading Engine - C++ Core v1.0.0" << std::endl;
    std::cout << "===============================================" << std::endl;
    std::cout << std::endl;

    try {
        vfx::trading::TradingEngine engine;
        engine.Start();

        std::cout << "Press Enter to run trading demo..." << std::endl;
        std::cin.get();

        engine.RunDemo();

        std::cout << std::endl << "🎉 VFX Trading Engine working perfectly!" << std::endl;
        engine.Stop();

    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << std::endl << "Press Enter to exit..." << std::endl;
    std::cin.get();
    return 0;
}
