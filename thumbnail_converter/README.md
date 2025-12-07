# ESP32 Thumbnail Converter Service

A lightweight Rust service that converts and resizes media thumbnails for ESP32-C6 devices with limited RAM.

## Problem

Home Assistant sends large PNG thumbnails (often 300x300 or larger) that exceed the ESP32-C6's available heap memory (~320KB total, with only 50-80KB free at runtime). The pngle PNG decoder requires ~44KB just for internal state, making it impossible to decode large images.

## Solution

This service acts as an MQTT middleware that:
1. Subscribes to Home Assistant's original thumbnail topic
2. Converts PNG to JPEG (better compression for photos)
3. Resizes images to 170x170 pixels (matches ESP32 screen height)
4. Applies horizontal fade gradient effect (fade to black on left side)
5. Publishes the optimized thumbnail to a new topic for the ESP32

## Features

- **PNG to JPEG conversion** - Smaller file sizes with minimal quality loss
- **Smart resizing** - Maintains aspect ratio using high-quality Lanczos3 filter
- **Horizontal fade effect** - Gradient fade to black on left side for stylish display
- **Automatic format detection** - Handles both PNG and JPEG inputs
- **Low overhead** - Efficient Rust implementation
- **Configurable** - Adjust size and quality via config file
- **Docker ready** - Easy deployment with included Dockerfile and docker-compose

## Installation

### Prerequisites

- Rust toolchain (install from https://rustup.rs/)
- MQTT broker (same one used by Home Assistant)

### Build

```bash
cd thumbnail_converter
cargo build --release
```

The compiled binary will be in `target/release/thumbnail_converter`

## Configuration

Edit `config.toml` (or use hardcoded defaults in `main.rs`):

```toml
[mqtt]
host = "192.168.16.100"
port = 1883

[topics]
source = "hass.agent/media_player/DESTEPTUL/thumbnail"
destination = "hass.agent/media_player/DESTEPTUL/thumbnail_small"

[image]
size = 170     # Output size in pixels (170x170 matches ESP32 screen height)
quality = 85   # JPEG quality 0-100
```

## Usage

Run the service:

```bash
# Development
cargo run

# Production (optimized build)
cargo build --release
./target/release/thumbnail_converter
```

### Running as a Service (Linux)

Create `/etc/systemd/system/thumbnail-converter.service`:

```ini
[Unit]
Description=ESP32 Thumbnail Converter
After=network.target

[Service]
Type=simple
User=your-user
WorkingDirectory=/path/to/thumbnail_converter
ExecStart=/path/to/thumbnail_converter/target/release/thumbnail_converter
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

Enable and start:
```bash
sudo systemctl enable thumbnail-converter
sudo systemctl start thumbnail-converter
```

### Running with Docker

The easiest way to run on a server! Docker files are included.

**Using Docker Compose (Recommended):**
```bash
cd thumbnail_converter
docker-compose up -d
```

**Using Docker directly:**
```bash
# Build
docker build -t thumbnail-converter .

# Run with host networking (required for MQTT)
docker run -d \
  --name thumbnail-converter \
  --network host \
  --restart unless-stopped \
  thumbnail-converter
```

**View logs:**
```bash
docker-compose logs -f          # With docker-compose
docker logs -f thumbnail-converter  # Without docker-compose
```

**Stop the service:**
```bash
docker-compose down             # With docker-compose
docker stop thumbnail-converter  # Without docker-compose
```

## ESP32 Configuration

Update your ESP32 code to subscribe to the new topic:

In `main/app_config.h`:
```c
#define MQTT_TOPIC_THUMB "hass.agent/media_player/DESTEPTUL/thumbnail_small"
```

The ESP32 will now receive optimized 170x170 JPEG thumbnails with fade effect instead of large PNGs!

## Performance

Example conversion:
- **Input**: 300x300 PNG (~110 KB)
- **Output**: 170x170 JPEG with fade effect (~10-12 KB)
- **Reduction**: ~90% smaller
- **ESP32 memory needed**: ~58KB (170x170 RGB565) - fits comfortably in available heap
- **Processing time**: < 50ms per image on modern hardware

## Troubleshooting

### Service won't start
- Check MQTT broker is running and accessible
- Verify host/port in config
- Check logs: `RUST_LOG=info cargo run`

### Thumbnails not appearing on ESP32
- Verify ESP32 is subscribed to the correct topic (`thumbnail_small`)
- Check MQTT broker logs
- Monitor service output: `RUST_LOG=debug cargo run`

### Image quality too low
- Increase `quality` setting in config (80-95 recommended)
- Increase `size` to 80 (max recommended for ESP32-C6)

## Logging

Set log level via environment variable:
```bash
RUST_LOG=debug cargo run  # Verbose logging
RUST_LOG=info cargo run   # Standard logging (default)
RUST_LOG=warn cargo run   # Warnings only
```

## License

MIT

## Credits

Built for ESP32-C6 media controller project
