use anyhow::{Context, Result};
use image::codecs::jpeg::JpegEncoder;
use log::{error, info, warn};
use rumqttc::{AsyncClient, Event, MqttOptions, Packet, QoS};
use std::time::Duration;

/// Configuration for the thumbnail converter service
#[derive(Debug)]
struct Config {
    mqtt_host: String,
    mqtt_port: u16,
    source_topic: String,
    dest_topic: String,
    thumbnail_size: u32,
    jpeg_quality: u8,
}

impl Default for Config {
    fn default() -> Self {
        Config {
            mqtt_host: "192.168.16.100".to_string(),
            mqtt_port: 1883,
            source_topic: "hass.agent/media_player/DESTEPTUL/thumbnail".to_string(),
            dest_topic: "hass.agent/media_player/DESTEPTUL/thumbnail_small".to_string(),
            thumbnail_size: 170, // 170px to match ESP32 screen height (320x170)
            jpeg_quality: 85,    // Increased quality slightly for larger image
        }
    }
}

/// Convert PNG image to JPEG with resizing
fn convert_png_to_jpeg(png_data: &[u8], size: u32, quality: u8) -> Result<Vec<u8>> {
    // Load the image from PNG bytes
    let img = image::load_from_memory(png_data)
        .context("Failed to load PNG image")?;

    info!("Original image size: {}x{}", img.width(), img.height());

    // Resize to thumbnail size while maintaining aspect ratio
    let thumbnail = img.resize(size, size, image::imageops::FilterType::Lanczos3);
    info!("Resized to: {}x{}", thumbnail.width(), thumbnail.height());

    // Convert to RGB8 for JPEG encoding (JPEG doesn't support alpha channel)
    let rgb_image = thumbnail.to_rgb8();

    // Convert to JPEG
    let mut jpeg_bytes = Vec::new();

    // Encode as JPEG with specified quality
    let mut encoder = JpegEncoder::new_with_quality(&mut jpeg_bytes, quality);
    encoder
        .encode(
            &rgb_image,
            rgb_image.width(),
            rgb_image.height(),
            image::ExtendedColorType::Rgb8,
        )
        .context("Failed to encode JPEG")?;

    info!(
        "Converted PNG ({} bytes) to JPEG ({} bytes) - {:.1}% reduction",
        png_data.len(),
        jpeg_bytes.len(),
        (1.0 - jpeg_bytes.len() as f64 / png_data.len() as f64) * 100.0
    );

    Ok(jpeg_bytes)
}

#[tokio::main]
async fn main() -> Result<()> {
    // Initialize logger
    env_logger::Builder::from_default_env()
        .filter_level(log::LevelFilter::Info)
        .init();

    info!("Starting ESP32 Thumbnail Converter Service");

    let config = Config::default();
    info!("Configuration: {:?}", config);

    // Setup MQTT client
    let mut mqttoptions = MqttOptions::new(
        "thumbnail_converter",
        &config.mqtt_host,
        config.mqtt_port,
    );
    mqttoptions.set_keep_alive(Duration::from_secs(30));

    // Increase max packet size to handle large thumbnails (up to 512KB)
    mqttoptions.set_max_packet_size(512 * 1024, 512 * 1024);

    let (client, mut eventloop) = AsyncClient::new(mqttoptions, 10);

    // Subscribe to source topic
    info!("Subscribing to: {}", config.source_topic);
    client
        .subscribe(&config.source_topic, QoS::AtLeastOnce)
        .await?;

    info!("Waiting for thumbnails...");

    // Process incoming messages
    loop {
        match eventloop.poll().await {
            Ok(Event::Incoming(Packet::Publish(publish))) => {
                info!(
                    "Received thumbnail on topic '{}': {} bytes",
                    publish.topic,
                    publish.payload.len()
                );

                // Check if this is a PNG
                if publish.payload.len() < 4 {
                    warn!("Payload too small to be a valid image");
                    continue;
                }

                // Check PNG signature (89 50 4E 47)
                if publish.payload[0] == 0x89
                    && publish.payload[1] == 0x50
                    && publish.payload[2] == 0x4E
                    && publish.payload[3] == 0x47
                {
                    info!("Detected PNG format");

                    // Convert PNG to JPEG
                    match convert_png_to_jpeg(
                        &publish.payload,
                        config.thumbnail_size,
                        config.jpeg_quality,
                    ) {
                        Ok(jpeg_data) => {
                            // Publish converted JPEG to destination topic
                            info!(
                                "Publishing JPEG to '{}': {} bytes",
                                config.dest_topic,
                                jpeg_data.len()
                            );

                            if let Err(e) = client
                                .publish(
                                    &config.dest_topic,
                                    QoS::AtLeastOnce,
                                    false,
                                    jpeg_data,
                                )
                                .await
                            {
                                error!("Failed to publish JPEG: {}", e);
                            } else {
                                info!("Successfully published converted thumbnail");
                            }
                        }
                        Err(e) => {
                            error!("Failed to convert PNG to JPEG: {}", e);
                        }
                    }
                } else if publish.payload[0] == 0xFF && publish.payload[1] == 0xD8 {
                    info!("Detected JPEG format - resizing only");

                    // Already JPEG, just resize
                    match image::load_from_memory(&publish.payload) {
                        Ok(img) => {
                            let thumbnail = img.resize(
                                config.thumbnail_size,
                                config.thumbnail_size,
                                image::imageops::FilterType::Lanczos3,
                            );

                            // Convert to RGB8 for JPEG encoding
                            let rgb_image = thumbnail.to_rgb8();

                            let mut jpeg_bytes = Vec::new();

                            let mut encoder = JpegEncoder::new_with_quality(
                                &mut jpeg_bytes,
                                config.jpeg_quality,
                            );
                            if let Err(e) = encoder.encode(
                                &rgb_image,
                                rgb_image.width(),
                                rgb_image.height(),
                                image::ExtendedColorType::Rgb8,
                            ) {
                                error!("Failed to encode JPEG: {}", e);
                                continue;
                            }

                            if let Err(e) = client
                                .publish(
                                    &config.dest_topic,
                                    QoS::AtLeastOnce,
                                    false,
                                    jpeg_bytes,
                                )
                                .await
                            {
                                error!("Failed to publish JPEG: {}", e);
                            } else {
                                info!("Successfully published resized thumbnail");
                            }
                        }
                        Err(e) => {
                            error!("Failed to load JPEG: {}", e);
                        }
                    }
                } else {
                    warn!(
                        "Unknown image format: {:02X} {:02X} {:02X} {:02X}",
                        publish.payload[0],
                        publish.payload[1],
                        publish.payload[2],
                        publish.payload[3]
                    );
                }
            }
            Ok(Event::Incoming(Packet::ConnAck(_))) => {
                info!("Connected to MQTT broker");
            }
            Ok(Event::Incoming(Packet::SubAck(suback))) => {
                info!("Subscription confirmed: {:?}", suback);
            }
            Ok(_) => {}
            Err(e) => {
                error!("MQTT error: {}", e);
                tokio::time::sleep(Duration::from_secs(1)).await;
            }
        }
    }
}
