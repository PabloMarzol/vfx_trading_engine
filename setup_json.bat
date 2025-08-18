@echo off
echo ===============================================
echo   Installing JSON Library for VFX Trading
echo ===============================================
echo.

REM Create external directory
echo Creating external libraries directory...
if not exist "backend\cpp\external" mkdir backend\cpp\external
if not exist "backend\cpp\external\json" mkdir backend\cpp\external\json

echo Downloading nlohmann JSON library...
cd backend\cpp\external\json

REM Download the single header file directly
powershell -Command "Invoke-WebRequest -Uri 'https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp' -OutFile 'json.hpp'"

if errorlevel 1 (
    echo Failed to download JSON library!
    echo.
    echo Alternative: Please manually download from:
    echo https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
    echo And place it in: backend\cpp\external\json\
    pause
    exit /b 1
)

echo ✓ JSON library downloaded successfully!

cd ..\..\..\..

echo.
echo ===============================================
echo   JSON Library Installation Complete!
echo ===============================================
echo.
echo The JSON library has been installed to:
echo   backend\cpp\external\json\json.hpp
echo.
pause