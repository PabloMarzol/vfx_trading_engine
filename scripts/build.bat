@echo off
cls
echo.
echo ===============================================
echo   VFX Trading Engine - CMake Build
echo ===============================================
echo.

REM Clean previous builds
echo [1/6] Cleaning previous builds...
if exist "build" (
    echo Removing old build directory...
    rmdir /s /q build
)

REM Create directory structure
echo [2/6] Setting up directories...
mkdir backend\cpp\src 2>nul
mkdir backend\cpp\include 2>nul
mkdir build

REM Create the corrected header file
echo [3/6] Creating trading_engine.h...
(
echo #pragma once
echo.
echo #include ^<memory^>
echo #include ^<vector^>
echo #include ^<unordered_map^>
echo #include ^<string^>
echo #include ^<atomic^>
echo #include ^<chrono^>
echo #include ^<thread^>
echo #include ^<mutex^>
echo #include ^<shared_mutex^>
echo #include ^<condition_variable^>
echo #include ^<queue^>
echo #include ^<functional^>
echo #include ^<cstdint^>
echo #include ^<algorithm^>
echo #include ^<iostream^>
echo #include ^<random^>
echo.
echo namespace vfx {
echo namespace trading {
echo.
echo using Price = double;
echo using Quantity = double;
echo using OrderId = uint64_t;
echo using Timestamp = std::chrono::high_resolution_clock::time_point;
echo.
echo enum class Side : uint8_t { BUY = 0, SELL = 1 };
echo enum class OrderType : uint8_t { MARKET = 0, LIMIT = 1 };
echo enum class OrderStatus : uint8_t { PENDING = 0, FILLED = 1, CANCELLED = 2 };
echo.
echo struct Order {
echo     OrderId id;
echo     std::string symbol;
echo     Side side;
echo     OrderType type;
echo     Quantity quantity;
echo     Price price;
echo     OrderStatus status;
echo     Timestamp created_at;
echo.
echo     Order^(OrderId id, const std::string^& symbol, Side side, OrderType type, Quantity qty, Price price = 0.0^)
echo         : id^(id^), symbol^(symbol^), side^(side^), type^(type^), quantity^(qty^), price^(price^),
echo           status^(OrderStatus::PENDING^), created_at^(std::chrono::high_resolution_clock::now^(^)^) {}
echo };
echo.
echo class TradingEngine {
echo private:
echo     std::unordered_map^<OrderId, std::unique_ptr^<Order^>^> orders_;
echo     mutable std::shared_mutex orders_mutex_;
echo     std::atomic^<OrderId^> next_order_id_{1000};
echo     std::atomic^<bool^> running_{false};
echo.
echo public:
echo     TradingEngine^(^);
echo     ~TradingEngine^(^);
echo.
echo     OrderId SubmitOrder^(const std::string^& symbol, Side side, OrderType type, Quantity quantity, Price price = 0.0^);
echo     bool CancelOrder^(OrderId order_id^);
echo     void Start^(^);
echo     void Stop^(^);
echo     bool IsRunning^(^) const;
echo     void RunDemo^(^);
echo.
echo private:
echo     void ExecuteOrder^(OrderId order_id^);
echo     Price GetMarketPrice^(const std::string^& symbol^) const;
echo };
echo.
echo }}
) > "backend\cpp\include\trading_engine.h"

echo ✓ Created trading_engine.h

REM Create the implementation file
echo [4/6] Creating trading_engine.cpp...
(
echo #include "trading_engine.h"
echo.
echo namespace vfx {
echo namespace trading {
echo.
echo TradingEngine::TradingEngine^(^) {
echo     std::cout ^<^< "🚀 VFX Trading Engine v1.0.0 - Initialized" ^<^< std::endl;
echo }
echo.
echo TradingEngine::~TradingEngine^(^) {
echo     Stop^(^);
echo     std::cout ^<^< "🔌 VFX Trading Engine shutdown" ^<^< std::endl;
echo }
echo.
echo OrderId TradingEngine::SubmitOrder^(const std::string^& symbol, Side side, OrderType type, Quantity quantity, Price price^) {
echo     OrderId order_id = next_order_id_++;
echo     
echo     auto order = std::make_unique^<Order^>^(order_id, symbol, side, type, quantity, price^);
echo     
echo     std::string side_str = ^(side == Side::BUY^) ? "BUY" : "SELL";
echo     std::string type_str = ^(type == OrderType::MARKET^) ? "MARKET" : "LIMIT";
echo     
echo     std::cout ^<^< "📝 Order " ^<^< order_id ^<^< ": " ^<^< side_str ^<^< " " ^<^< quantity ^<^< " " ^<^< symbol;
echo     if^(type == OrderType::LIMIT^) {
echo         std::cout ^<^< " @ $" ^<^< price;
echo     }
echo     std::cout ^<^< " ^(" ^<^< type_str ^<^< "^)" ^<^< std::endl;
echo     
echo     {
echo         std::unique_lock^<std::shared_mutex^> lock^(orders_mutex_^);
echo         orders_[order_id] = std::move^(order^);
echo     }
echo     
echo     ExecuteOrder^(order_id^);
echo     return order_id;
echo }
echo.
echo bool TradingEngine::CancelOrder^(OrderId order_id^) {
echo     std::unique_lock^<std::shared_mutex^> lock^(orders_mutex_^);
echo     
echo     auto it = orders_.find^(order_id^);
echo     if ^(it != orders_.end^(^) ^&^& it-^>second-^>status == OrderStatus::PENDING^) {
echo         it-^>second-^>status = OrderStatus::CANCELLED;
echo         std::cout ^<^< "❌ Order cancelled: " ^<^< order_id ^<^< std::endl;
echo         return true;
echo     }
echo     return false;
echo }
echo.
echo void TradingEngine::Start^(^) {
echo     running_ = true;
echo     std::cout ^<^< "✅ Trading engine started" ^<^< std::endl;
echo }
echo.
echo void TradingEngine::Stop^(^) {
echo     running_ = false;
echo     std::cout ^<^< "🛑 Trading engine stopped" ^<^< std::endl;
echo }
echo.
echo bool TradingEngine::IsRunning^(^) const {
echo     return running_;
echo }
echo.
echo void TradingEngine::RunDemo^(^) {
echo     std::cout ^<^< std::endl ^<^< "🎯 Running VFX Trading Demo..." ^<^< std::endl;
echo     
echo     SubmitOrder^("BTC/USD", Side::BUY, OrderType::MARKET, 0.1^);
echo     std::this_thread::sleep_for^(std::chrono::milliseconds^(500^)^);
echo     
echo     SubmitOrder^("ETH/USD", Side::BUY, OrderType::LIMIT, 2.5, 3900.0^);
echo     std::this_thread::sleep_for^(std::chrono::milliseconds^(500^)^);
echo     
echo     SubmitOrder^("AAPL", Side::SELL, OrderType::MARKET, 50^);
echo     std::this_thread::sleep_for^(std::chrono::milliseconds^(500^)^);
echo     
echo     SubmitOrder^("TSLA", Side::BUY, OrderType::LIMIT, 10, 250.0^);
echo     std::this_thread::sleep_for^(std::chrono::milliseconds^(500^)^);
echo     
echo     SubmitOrder^("NVDA", Side::SELL, OrderType::MARKET, 5^);
echo     
echo     std::cout ^<^< "🎉 Demo completed successfully!" ^<^< std::endl;
echo }
echo.
echo void TradingEngine::ExecuteOrder^(OrderId order_id^) {
echo     std::shared_lock^<std::shared_mutex^> lock^(orders_mutex_^);
echo     
echo     auto it = orders_.find^(order_id^);
echo     if ^(it == orders_.end^(^)^) return;
echo     
echo     Order^& order = *it-^>second;
echo     Price exec_price = ^(order.type == OrderType::MARKET^) ? GetMarketPrice^(order.symbol^) : order.price;
echo     
echo     // Add some realistic price variation
echo     std::random_device rd;
echo     std::mt19937 gen^(rd^(^)^);
echo     std::uniform_real_distribution^<^> dis^(0.999, 1.001^);
echo     exec_price *= dis^(gen^);
echo     
echo     order.status = OrderStatus::FILLED;
echo     order.price = exec_price;
echo     
echo     std::string side_str = ^(order.side == Side::BUY^) ? "BUY" : "SELL";
echo     std::cout ^<^< "✅ EXECUTED: Order " ^<^< order_id ^<^< " - " ^<^< side_str ^<^< " " ^<^< order.quantity 
echo               ^<^< " " ^<^< order.symbol ^<^< " @ $" ^<^< exec_price ^<^< std::endl;
echo }
echo.
echo Price TradingEngine::GetMarketPrice^(const std::string^& symbol^) const {
echo     if^(symbol == "BTC/USD"^) return 68450.0;
echo     if^(symbol == "ETH/USD"^) return 3892.0;
echo     if^(symbol == "AAPL"^) return 175.0;
echo     if^(symbol == "TSLA"^) return 245.0;
echo     if^(symbol == "NVDA"^) return 875.0;
echo     return 100.0;
echo }
echo.
echo }}
) > "backend\cpp\src\trading_engine.cpp"

echo ✓ Created trading_engine.cpp

REM Create main.cpp
echo [5/6] Creating main.cpp...
(
echo #include "trading_engine.h"
echo #include ^<iostream^>
echo #include ^<thread^>
echo #include ^<chrono^>
echo.
echo int main^(^) {
echo     std::cout ^<^< "===============================================" ^<^< std::endl;
echo     std::cout ^<^< "  VFX Trading Engine - C++ Core v1.0.0" ^<^< std::endl;
echo     std::cout ^<^< "===============================================" ^<^< std::endl;
echo     std::cout ^<^< std::endl;
echo.
echo     try {
echo         vfx::trading::TradingEngine engine;
echo         engine.Start^(^);
echo.
echo         std::cout ^<^< "Press Enter to run trading demo..." ^<^< std::endl;
echo         std::cin.get^(^);
echo.
echo         engine.RunDemo^(^);
echo.
echo         std::cout ^<^< std::endl ^<^< "🎉 VFX Trading Engine working perfectly!" ^<^< std::endl;
echo         engine.Stop^(^);
echo.
echo     } catch ^(const std::exception^& e^) {
echo         std::cerr ^<^< "❌ Error: " ^<^< e.what^(^) ^<^< std::endl;
echo         return 1;
echo     }
echo.
echo     std::cout ^<^< std::endl ^<^< "Press Enter to exit..." ^<^< std::endl;
echo     std::cin.get^(^);
echo     return 0;
echo }
) > "backend\cpp\src\main.cpp"

echo ✓ Created main.cpp

REM Create CMakeLists.txt
echo Creating CMakeLists.txt...
(
echo cmake_minimum_required^(VERSION 3.16^)
echo project^(VFX_Trading_Engine VERSION 1.0.0 LANGUAGES CXX^)
echo.
echo set^(CMAKE_CXX_STANDARD 17^)
echo set^(CMAKE_CXX_STANDARD_REQUIRED ON^)
echo set^(CMAKE_CXX_EXTENSIONS OFF^)
echo.
echo if^(MINGW^)
echo     set^(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++"^)
echo endif^(^)
echo.
echo include_directories^(include^)
echo.
echo set^(SOURCES
echo     src/main.cpp
echo     src/trading_engine.cpp
echo ^)
echo.
echo add_executable^(vfx_trading_engine ${SOURCES}^)
echo.
echo find_package^(Threads REQUIRED^)
echo target_link_libraries^(vfx_trading_engine PRIVATE Threads::Threads^)
echo.
echo if^(WIN32^)
echo     target_compile_definitions^(vfx_trading_engine PRIVATE _WIN32_WINNT=0x0601^)
echo endif^(^)
) > "backend\cpp\CMakeLists.txt"

echo ✓ Created CMakeLists.txt

echo [6/6] Building with CMake...
cd build

echo Configuring with MinGW...
cmake ..\backend\cpp -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    echo ❌ CMake configuration failed!
    cd ..
    pause
    exit /b 1
)

echo ✓ Configuration successful

echo Building...
cmake --build . --config Release
if errorlevel 1 (
    echo ❌ Build failed!
    cd ..
    pause
    exit /b 1
)

cd ..

echo.
echo ===============================================
echo   🎉 BUILD SUCCESSFUL! 🎉
echo ===============================================
echo.
echo 📁 Executable: build\vfx_trading_engine.exe
echo 📊 Source files created in backend\cpp\
echo.
echo 🚀 VFX Trading Engine is ready!
echo.

set /p run_now="Would you like to run the trading engine now? (y/n): "
if /i "%run_now%"=="y" (
    echo.
    echo Starting VFX Trading Engine...
    echo.
    build\vfx_trading_engine.exe
)

echo.
echo ✅ VFX Trading Engine C++ core completed!
echo.
echo Next steps:
echo 1. ✅ C++ engine is working
echo 2. 🐍 Start Python strategy engine: python backend\python\main.py  
echo 3. 🌐 Open frontend: frontend\index.html
echo 4. 💹 Start trading!
echo.
pause