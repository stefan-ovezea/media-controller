#include "display_driver.h"
#include "app_config.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_sh8601.h"
#include "esp_log.h"
#include "i2c_bsp.h"

static const char *TAG = "display_driver";

// LCD initialization commands for SH8601
static const sh8601_lcd_init_cmd_t lcd_init_cmds[] = {
#if (DISPLAY_ORIENTATION == ORIENTATION_NORMAL)
    // Normal orientation (portrait)
    //{0x36, (uint8_t []){0x00}, 1, 0},
#elif (DISPLAY_ORIENTATION == ORIENTATION_ROTATE)
    // Rotated orientation (landscape)
    {0x36, (uint8_t []){0x70}, 1, 0},
#endif
    {0xb2, (uint8_t []){0x0c, 0x0c, 0x00, 0x33, 0x33}, 5, 0},
    {0xb7, (uint8_t []){0x35}, 1, 0},
    {0xbb, (uint8_t []){0x13}, 1, 0},
    {0xc0, (uint8_t []){0x2c}, 1, 0},
    {0xc2, (uint8_t []){0x01}, 1, 0},
    {0xc3, (uint8_t []){0x0b}, 1, 0},
    {0xc4, (uint8_t []){0x20}, 1, 0},
    {0xc6, (uint8_t []){0x0f}, 1, 0},
    {0xd0, (uint8_t []){0xa4, 0xa1}, 2, 0},
    {0xd6, (uint8_t []){0xa1}, 1, 0},
    {0xe0, (uint8_t []){0x00, 0x03, 0x07, 0x08, 0x07, 0x15, 0x2A, 0x44, 0x42, 0x0A, 0x17, 0x18, 0x25, 0x27}, 14, 0},
    {0xe1, (uint8_t []){0x00, 0x03, 0x08, 0x07, 0x07, 0x23, 0x2A, 0x43, 0x42, 0x09, 0x18, 0x17, 0x25, 0x27}, 14, 0},
    {0x21, (uint8_t []){0x21}, 0, 0},
    {0x11, (uint8_t []){0x11}, 0, 120},
    {0x29, (uint8_t []){0x29}, 0, 0},
};

esp_lcd_panel_handle_t display_init(void)
{
    ESP_LOGI(TAG, "Initializing display driver");
    
    // Configure SPI bus
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LCD_DMA_LINES * sizeof(uint16_t),
    };
    
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_LOGI(TAG, "SPI bus initialized");
    
    // Configure LCD panel IO
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_NUM_DC,
        .cs_gpio_num = PIN_NUM_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));
    ESP_LOGI(TAG, "LCD panel IO initialized");
    
    // Configure LCD panel
    sh8601_vendor_config_t vendor_config = {
        .init_cmds = lcd_init_cmds,
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
    };
    
    esp_lcd_panel_handle_t panel_handle = NULL;
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_config,
        .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
    };
    
    ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(io_handle, &panel_config, &panel_handle));
    ESP_LOGI(TAG, "LCD panel created");
    
    // Reset and initialize panel
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    
    // Initialize I2C (required for some display features)
    I2C_master_Init();
    
    ESP_LOGI(TAG, "Display initialized successfully (Resolution: %dx%d)", LCD_H_RES, LCD_V_RES);
    
    return panel_handle;
}
