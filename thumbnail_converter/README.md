# ESP32 Thumbnail Converter Service

A lightweight Rust service that converts and resizes media thumbnails for ESP32-C6 devices with limited RAM.

## Problem

Home Assistant sends large PNG thumbnails (often 300x300 or larger) that exceed the ESP32-C6's available heap memory (~320KB total, with only 50-80KB free at runtime). The pngle PNG decoder requires ~44KB just for internal state, making it impossible to decode large images.

## Solution

This service acts as an MQTT middleware that:
1. Subscribes to Home Assistant's original thumbnail topic
2. Converts PNG to JPEG (better compression for photos)
3. Resizes images to 64x64 pixels (configurable)
4. Publishes the optimized thumbnail to a new topic for the ESP32

## Features

- **PNG to JPEG conversion** - Smaller file sizes with minimal quality loss
- **Smart resizing** - Maintains aspect ratio using high-quality Lanczos3 filter
- **Automatic format detection** - Handles both PNG and JPEG inputs
- **Low overhead** - Efficient Rust implementation
- **Configurable** - Adjust size and quality via config file

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
size = 64      # Output size in pixels (64x64 recommended for ESP32-C6)
quality = 80   # JPEG quality 0-100
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

Create `Dockerfile`:
```dockerfile
FROM rust:1.75 as builder
WORKDIR /app
COPY . .
RUN cargo build --release

FROM debian:bookworm-slim
COPY --from=builder /app/target/release/thumbnail_converter /usr/local/bin/
CMD ["thumbnail_converter"]
```

Build and run:
```bash
docker build -t thumbnail-converter .
docker run -d --name thumbnail-converter --restart always thumbnail-converter
```

## ESP32 Configuration

Update your ESP32 code to subscribe to the new topic:

In `main/app_config.h`:
```c
#define MQTT_TOPIC_THUMB "hass.agent/media_player/DESTEPTUL/thumbnail_small"
```

The ESP32 will now receive optimized 64x64 JPEG thumbnails instead of large PNGs!

## Performance

Example conversion:
- **Input**: 150x83 PNG (17.9 KB)
- **Output**: 64x64 JPEG (2-4 KB)
- **Reduction**: ~80% smaller
- **ESP32 memory needed**: ~8KB (64x64 RGB565) vs ~25KB (150x83)

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
