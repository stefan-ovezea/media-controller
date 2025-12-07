#!/bin/bash
# Thumbnail Converter Service - Linux/Mac Runner

set -e

echo "Starting ESP32 Thumbnail Converter Service..."
echo

# Check if cargo is available
if ! command -v cargo &> /dev/null; then
    echo "ERROR: Cargo not found!"
    echo "Please install Rust from https://rustup.rs/"
    exit 1
fi

# Build in release mode for better performance
echo "Building in release mode..."
cargo build --release

echo
echo "Starting service..."
echo "Press Ctrl+C to stop"
echo

# Run with info-level logging
RUST_LOG=info ./target/release/thumbnail_converter
