# VFX Trading Engine - PowerShell Build Script (FIXED)
# Run this in PowerShell: .\build_vfx_fixed.ps1

Clear-Host
Write-Host ""
Write-Host "===============================================" -ForegroundColor Green
Write-Host "  VFX Trading Engine - PowerShell Build" -ForegroundColor Green  
Write-Host "===============================================" -ForegroundColor Green
Write-Host ""

# Check if we're in the right directory
if (-not (Test-Path "backend\cpp")) {
    Write-Host "ERROR: Please run this script from the vfx-trading root directory" -ForegroundColor Red
    Write-Host "Expected: vfx-trading\backend\cpp\" -ForegroundColor Yellow
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Host "[1/6] Checking system requirements..." -ForegroundColor Cyan

# Check for Visual Studio or Build Tools
$vsFound = $false
$vsPaths = @(
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\*\MSBuild\Current\Bin\MSBuild.exe",
    "${env:ProgramFiles}\Microsoft Visual Studio\2019\*\MSBuild\Current\Bin\MSBuild.exe",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\*\MSBuild\Current\Bin\MSBuild.exe"
)

foreach ($path in $vsPaths) {
    $found = Get-ChildItem -Path $path -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) {
        $vsFound = $true
        break
    }
}

if (-not $vsFound) {
    Write-Host "ERROR: Visual Studio or Build Tools not found!" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please install Visual Studio 2019/2022 (Community Edition is free)" -ForegroundColor Yellow
    Write-Host "Download from: https://visualstudio.microsoft.com/" -ForegroundColor Blue
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Host "✓ Found Visual Studio Build Tools" -ForegroundColor Green

# Check for CMake
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) {
    Write-Host "ERROR: CMake not found!" -ForegroundColor Red
    Write-Host "Please install CMake from: https://cmake.org/" -ForegroundColor Blue
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Host "✓ Found CMake" -ForegroundColor Green

Write-Host "[2/6] Setting up directory structure..." -ForegroundColor Cyan

# Create build directory
if (-not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
    Write-Host "✓ Created build directory" -ForegroundColor Green
}

# Create missing directories if needed
$dirs = @("backend\cpp\src", "backend\cpp\include")
foreach ($dir in $dirs) {
    if (-not (Test-Path $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
        Write-Host "✓ Created directory: $dir" -ForegroundColor Green
    }
}

Write-Host "[3/6] Checking source files..." -ForegroundColor Cyan

# Check for essential files and create minimal versions if missing
$headerPath = "backend\cpp\include\trading_engine.h"
if (-not (Test-Path $headerPath)) {
    Write-Host "Creating minimal trading_engine.h..." -ForegroundColor Yellow
    
    $headerContent = @'
#pragma once
#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <chrono>

namespace vfx {
namespace trading {
    using OrderId = uint64_t;
    using Price = double;
    using Quantity = double;
    
    enum class Side { BUY, SELL };
    enum class OrderType { MARKET, LIMIT };
    
    class TradingEngine {
    private:
        std::atomic<OrderId> next_order_id_;
        
    public:
        TradingEngine() : next_order_id_(1000) {
            std::cout << "VFX Trading Engine initialized" << std::endl;
        }
        
        OrderId SubmitOrder(const std::string& symbol, Side side, 
                           OrderType type, Quantity quantity, Price price = 0.0) {
            OrderId id = next_order_id_++;
            std::cout << "Order " << id << ": " << symbol << std::endl;
            return id;
        }
        
        void Start() {
            std::cout << "Trading engine started" << std::endl;
        }
        
        void Stop() {
            std::cout << "Trading engine stopped" << std::endl;
        }
    };
}}
'@
    
    $headerContent | Out-File -FilePath $headerPath -Encoding UTF8
}

$mainPath = "backend\cpp\src\main.cpp"
if (-not (Test-Path $mainPath)) {
    Write-Host "Creating minimal main.cpp..." -ForegroundColor Yellow
    
    $mainContent = @'
#include "trading_engine.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    std::cout << "🚀 VFX Trading Engine v1.0.0" << std::endl;
    
    try {
        vfx::trading::TradingEngine engine;
        engine.Start();
        
        // Test order submission
        auto orderId = engine.SubmitOrder("BTC/USD", vfx::trading::Side::BUY, 
                                         vfx::trading::OrderType::MARKET, 1.0, 50000.0);
        std::cout << "✅ Test order submitted: " << orderId << std::endl;
        
        std::cout << "Press Ctrl+C to stop..." << std::endl;
        
        // Keep running for 10 seconds then exit
        for(int i = 0; i < 10; i++) {
            std::cout << "💹 Trading engine running... " << (10-i) << "s remaining" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        engine.Stop();
        std::cout << "✅ VFX Trading Engine completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
'@
    
    $mainContent | Out-File -FilePath $mainPath -Encoding UTF8
}

$cmakePath = "backend\cpp\CMakeLists.txt"
if (-not (Test-Path $cmakePath)) {
    Write-Host "Creating minimal CMakeLists.txt..." -ForegroundColor Yellow
    
    $cmakeContent = @'
cmake_minimum_required(VERSION 3.16)
project(VFX_Trading_Engine VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(include)

# Create executable
add_executable(vfx_trading_engine src/main.cpp)

# Windows specific settings
if(WIN32)
    target_compile_definitions(vfx_trading_engine PRIVATE _WIN32_WINNT=0x0601)
endif()
'@
    
    $cmakeContent | Out-File -FilePath $cmakePath -Encoding UTF8
}

Write-Host "✓ Source files ready" -ForegroundColor Green

Write-Host "[4/6] Configuring with CMake..." -ForegroundColor Cyan

Set-Location "build"

# Run CMake configuration
try {
    $configResult = cmake "..\backend\cpp" -G "Visual Studio 16 2019" -A x64 2>&1
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Trying with Visual Studio 17 2022..." -ForegroundColor Yellow
        $configResult = cmake "..\backend\cpp" -G "Visual Studio 17 2022" -A x64 2>&1
        
        if ($LASTEXITCODE -ne 0) {
            Write-Host "ERROR: CMake configuration failed!" -ForegroundColor Red
            Write-Host $configResult -ForegroundColor Red
            Set-Location ".."
            Read-Host "Press Enter to exit"
            exit 1
        }
    }
    
    Write-Host "✓ CMake configuration successful" -ForegroundColor Green
}
catch {
    Write-Host "ERROR: CMake configuration failed!" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    Set-Location ".."
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Host "[5/6] Building project..." -ForegroundColor Cyan

# Build the project
try {
    $buildResult = cmake --build . --config Release 2>&1
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: Build failed!" -ForegroundColor Red
        Write-Host $buildResult -ForegroundColor Red
        Set-Location ".."
        Read-Host "Press Enter to exit"
        exit 1
    }
    
    Write-Host "✓ Build successful" -ForegroundColor Green
}
catch {
    Write-Host "ERROR: Build failed!" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    Set-Location ".."
    Read-Host "Press Enter to exit"
    exit 1
}

Set-Location ".."

Write-Host "[6/6] Build completed!" -ForegroundColor Cyan
Write-Host ""
Write-Host "===============================================" -ForegroundColor Green
Write-Host "  🎉 BUILD SUCCESSFUL! 🎉" -ForegroundColor Green
Write-Host "===============================================" -ForegroundColor Green
Write-Host ""
Write-Host "📁 Executable location:" -ForegroundColor White
Write-Host "   build\Release\vfx_trading_engine.exe" -ForegroundColor Yellow
Write-Host ""
Write-Host "🚀 To run the trading engine:" -ForegroundColor White
Write-Host "   cd build\Release" -ForegroundColor Yellow
Write-Host "   .\vfx_trading_engine.exe" -ForegroundColor Yellow
Write-Host ""

# Option to run immediately
$runNow = Read-Host "Would you like to run the trading engine now? (y/n)"
if ($runNow -eq "y" -or $runNow -eq "Y") {
    Write-Host ""
    Write-Host "Starting VFX Trading Engine..." -ForegroundColor Green
    Set-Location "build\Release"
    .\vfx_trading_engine.exe
    Set-Location "..\.."
}

Write-Host ""
Write-Host "Build script completed successfully!" -ForegroundColor Green
Read-Host "Press Enter to exit"