# Home Assistant Configuration for ESP32-C6 Media Controller

## Memory Constraints

The ESP32-C6 has limited RAM (~320KB total, with only 50-80KB free at runtime).
PNG decoding with pngle requires:
- **~44KB** for decoder internal state (fixed overhead)
- **Image buffer**: width × height × 2 bytes (RGB565)

### Recommended Thumbnail Sizes

| Size   | RGB565 Buffer | Total RAM Needed | Status |
|--------|---------------|------------------|---------|
| 48×48  | 4.6 KB        | ~49 KB           | ✅ Should work |
| 64×64  | 8 KB          | ~52 KB           | ✅ Recommended |
| 80×80  | 12.8 KB       | ~57 KB           | ⚠️ Might work |
| 100×100| 20 KB         | ~64 KB           | ❌ Likely fails |
| 300×300| 180 KB        | ~224 KB          | ❌ Will fail |

**Recommendation: Use 64×64 pixel thumbnails**

## How to Configure Home Assistant

### Option 1: Modify your automation/script that publishes thumbnails

In your Home Assistant automation or AppDaemon script, resize the thumbnail before sending:

```yaml
# Example automation
automation:
  - alias: "Send Media Player Thumbnail to ESP32"
    trigger:
      - platform: state
        entity_id: media_player.your_player
        attribute: entity_picture
    action:
      - service: mqtt.publish
        data:
          topic: "hass.agent/media_player/DESTEPTUL/thumbnail"
          payload: >-
            {{ state_attr('media_player.your_player', 'entity_picture')
               | resize_image(64, 64) }}  # Resize to 64x64
```

### Option 2: Use ImageMagick or Pillow to resize thumbnails

If you're using AppDaemon or a Python script:

```python
from PIL import Image
import io

def resize_thumbnail(image_data, size=(64, 64)):
    """Resize thumbnail to fit ESP32-C6 memory constraints"""
    img = Image.open(io.BytesIO(image_data))

    # Resize with high quality
    img.thumbnail(size, Image.Resampling.LANCZOS)

    # Save as PNG with compression
    output = io.BytesIO()
    img.save(output, format='PNG', optimize=True)
    return output.getvalue()

# In your MQTT publish code:
thumbnail_data = resize_thumbnail(original_thumbnail, (64, 64))
client.publish("hass.agent/media_player/DESTEPTUL/thumbnail", thumbnail_data)
```

### Option 3: Server-side conversion (Advanced)

For best performance, you could convert PNG to raw RGB565 on the server:

```python
def png_to_rgb565(image_data):
    """Convert PNG to raw RGB565 format for ESP32"""
    img = Image.open(io.BytesIO(image_data))
    img = img.convert('RGB')
    img = img.resize((64, 64), Image.Resampling.LANCZOS)

    rgb565_data = bytearray()
    for y in range(img.height):
        for x in range(img.width):
            r, g, b = img.getpixel((x, y))
            # Convert to RGB565
            rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            rgb565_data.append(rgb565 >> 8)
            rgb565_data.append(rgb565 & 0xFF)

    return bytes(rgb565_data)
```

Then modify `ui_media_update_thumbnail()` in your ESP32 code to handle raw RGB565 data directly (skip PNG decoding).

## Testing

After reconfiguring Home Assistant:

1. Flash your ESP32-C6 with the updated code
2. Monitor the serial output for memory usage:
   ```
   I (12345) ui_media: Creating pngle decoder (free: 78234 bytes, min-free-ever: 65432 bytes)...
   I (12456) ui_media: pngle decoder created (used 44123 bytes, free: 34111 bytes)
   I (12567) ui_media: PNG decoded successfully: 64x64, consumed 3456 bytes
   ```
3. If you see "Failed to create pngle decoder", the thumbnail is still too large

## Alternative: Switch to JPEG

If PNG still doesn't work, consider switching to JPEG thumbnails which are:
- More memory-efficient for photos
- Smaller file sizes
- Supported by `esp_lv_decoder` (which you already have installed)

Let me know if you need help implementing JPEG support!
