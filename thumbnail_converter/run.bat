@echo off
REM Thumbnail Converter Service - Windows Runner
REM Make sure Rust is installed: https://rustup.rs/

echo Starting ESP32 Thumbnail Converter Service...
echo.

REM Check if cargo is available
where cargo >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Cargo not found!
    echo Please install Rust from https://rustup.rs/
    echo After installation, restart your terminal and run this script again.
    pause
    exit /b 1
)

REM Build in release mode for better performance
echo Building in release mode...
cargo build --release
if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    pause
    exit /b 1
)

echo.
echo Starting service...
echo Press Ctrl+C to stop
echo.

REM Run with info-level logging
set RUST_LOG=info
.\target\release\thumbnail_converter.exe

pause
