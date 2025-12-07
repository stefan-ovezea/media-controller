# Quick Start Guide

## TL;DR

```bash
# 1. Install Rust (if not already installed)
# Windows: https://rustup.rs/
# Linux/Mac: curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh

# 2. Build and run
cd thumbnail_converter
cargo run --release
```

That's it! The service will:
- ✅ Subscribe to your Home Assistant thumbnail topic
- ✅ Convert PNG → JPEG
- ✅ Resize to 64×64 pixels
- ✅ Publish to `thumbnail_small` topic for your ESP32

## What You'll See

```
[INFO] Starting ESP32 Thumbnail Converter Service
[INFO] Connected to MQTT broker
[INFO] Waiting for thumbnails...

# When you play media in Home Assistant:
[INFO] Received thumbnail: 17911 bytes
[INFO] Original image size: 150x83
[INFO] Resized to: 64x64
[INFO] Converted PNG (17911 bytes) to JPEG (2847 bytes) - 84.1% reduction
[INFO] Successfully published converted thumbnail
```

## Configuration

The service works out-of-the-box with these defaults:
- MQTT Broker: `192.168.16.100:1883`
- Source Topic: `hass.agent/media_player/DESTEPTUL/thumbnail`
- Dest Topic: `hass.agent/media_player/DESTEPTUL/thumbnail_small`
- Output Size: `64×64 pixels`
- JPEG Quality: `80`

To change these, edit the `Config::default()` function in `src/main.rs` or create a `config.toml` file.

## Need Help?

See [INSTALL.md](INSTALL.md) for detailed installation instructions.
