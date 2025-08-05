cmake_minimum_required(VERSION 3.16)
project(VFX_Trading_Engine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific options
if(MSVC)
    add_compile_options(/W4)
    add_compile_definitions(_WIN32_WINNT=0x0601)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# MinGW specific linking
if(MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
endif()

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Optimization flags
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")

# Find required packages
find_package(Threads REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Source files for the complete VFX Trading Engine
set(SOURCES
    src/main.cpp
    src/trading_engine.cpp
    src/order_manager.cpp
    src/market_data.cpp
    src/risk_manager.cpp
    src/websocket_server.cpp
)

# Header files
set(HEADERS
    include/types.h
    include/trading_engine.h
    include/order_manager.h
    include/market_data.h
    include/risk_manager.h
)

# Create executable
add_executable(vfx_trading_engine ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(vfx_trading_engine 
    PRIVATE 
    Threads::Threads
)

# Windows specific libraries
if(WIN32)
    target_compile_definitions(vfx_trading_engine PRIVATE _WIN32_WINNT=0x0601)
    target_link_libraries(vfx_trading_engine PRIVATE ws2_32 wsock32)
endif()

# Installation
install(TARGETS vfx_trading_engine
    RUNTIME DESTINATION bin
)

# Print build information
message(STATUS "=== VFX Trading Engine Build Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "==============================================")