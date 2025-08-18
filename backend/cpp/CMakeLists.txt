cmake_minimum_required(VERSION 3.16)
project(VFX_Trading_Engine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
endif()

include_directories(include)

set(SOURCES
    src/main.cpp
    src/trading_engine.cpp
)

add_executable(vfx_trading_engine ${SOURCES})

find_package(Threads REQUIRED)
target_link_libraries(vfx_trading_engine PRIVATE Threads::Threads)

if(WIN32)
    target_compile_definitions(vfx_trading_engine PRIVATE _WIN32_WINNT=0x0601)
endif()
