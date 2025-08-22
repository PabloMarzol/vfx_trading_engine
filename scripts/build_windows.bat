
@echo off
echo Building VFX Trading Engine for Windows...
if not exist build mkdir build
cd build
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release
echo.
echo Build complete! Executable: build/Release/vfx_trading_engine.exe
pause
