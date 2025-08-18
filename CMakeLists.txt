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

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Optimization flags
if(MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")

# Find required packages
find_package(Threads REQUIRED)

# Platform-specific libraries
if(WIN32)
    set(PLATFORM_LIBS ws2_32 wsock32)
elseif(UNIX AND NOT APPLE)
    set(PLATFORM_LIBS pthread rt)
elseif(APPLE)
    set(PLATFORM_LIBS pthread)
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/json  # JSON library
)

# Source files
set(SOURCES
    src/main.cpp
    src/trading_engine.cpp
    src/websocket_server.cpp
    src/order_manager.cpp
    src/market_data.cpp
    src/risk_manager.cpp
    src/message_protocol.cpp  
    src/strategy_manager.cpp   
)

# Create executable
add_executable(vfx_trading_engine ${SOURCES})

# Link libraries
target_link_libraries(vfx_trading_engine 
    PRIVATE 
    Threads::Threads
    ${PLATFORM_LIBS}
)

# Compiler definitions
target_compile_definitions(vfx_trading_engine PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

# Print build information
message(STATUS "=== VFX Trading Engine Build Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "WebSocket support: Enabled")
message(STATUS "JSON support: Enabled (nlohmann/json)")
message(STATUS "===============================================")

if(WIN32)
    message(STATUS "Windows libraries: ${PLATFORM_LIBS}")
endif()