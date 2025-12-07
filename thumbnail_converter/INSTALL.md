# Installation Guide

## Step 1: Install Rust

### Windows
1. Download and run the installer from: https://rustup.rs/
2. Follow the installation prompts
3. **Restart your terminal/command prompt** after installation
4. Verify installation:
   ```bash
   cargo --version
   ```

### Linux/Mac
```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source ~/.cargo/env
cargo --version
```

## Step 2: Build the Service

```bash
cd thumbnail_converter
cargo build --release
```

This will download dependencies and compile the service. First build takes 2-5 minutes.

## Step 3: Configure

Edit `config.toml` or modify the defaults in `src/main.rs`:

```toml
[mqtt]
host = "192.168.16.100"  # Your MQTT broker IP
port = 1883

[topics]
source = "hass.agent/media_player/DESTEPTUL/thumbnail"
destination = "hass.agent/media_player/DESTEPTUL/thumbnail_small"

[image]
size = 64       # Output size (64x64 recommended)
quality = 80    # JPEG quality 0-100
```

## Step 4: Run

### Windows
Double-click `run.bat` or:
```bash
.\run.bat
```

### Linux/Mac
```bash
chmod +x run.sh
./run.sh
```

Or run directly:
```bash
cargo run --release
```

## Step 5: Verify

You should see output like:
```
[INFO] Starting ESP32 Thumbnail Converter Service
[INFO] Configuration: Config { mqtt_host: "192.168.16.100", ... }
[INFO] Subscribing to: hass.agent/media_player/DESTEPTUL/thumbnail
[INFO] Connected to MQTT broker
[INFO] Waiting for thumbnails...
```

When a thumbnail arrives:
```
[INFO] Received thumbnail on topic '...': 17911 bytes
[INFO] Detected PNG format
[INFO] Original image size: 150x83
[INFO] Resized to: 64x64
[INFO] Converted PNG (17911 bytes) to JPEG (2847 bytes) - 84.1% reduction
[INFO] Publishing JPEG to '...thumbnail_small': 2847 bytes
[INFO] Successfully published converted thumbnail
```

## Troubleshooting

### "Cargo not found"
- Make sure you installed Rust from https://rustup.rs/
- **Restart your terminal** after installation
- On Windows, you might need to restart VS Code entirely

### "Failed to connect to MQTT broker"
- Check that your MQTT broker is running
- Verify the IP address in config matches your broker
- Test with mosquitto:
  ```bash
  mosquitto_sub -h 192.168.16.100 -t "#" -v
  ```

### Build errors
- Update Rust: `rustup update`
- Clean and rebuild: `cargo clean && cargo build --release`

### No thumbnails being converted
- Check Home Assistant is publishing to the source topic
- Monitor MQTT with:
  ```bash
  mosquitto_sub -h 192.168.16.100 -t "hass.agent/media_player/+/thumbnail" -v
  ```

## Running in Production

See [README.md](README.md) for systemd service or Docker deployment instructions.
