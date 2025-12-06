#ifndef APP_CONFIG_H
#define APP_CONFIG_H

// Display Orientation
#define ORIENTATION_NORMAL  0  // Portrait: 170x320
#define ORIENTATION_ROTATE  1  // Landscape: 320x170
#define DISPLAY_ORIENTATION ORIENTATION_ROTATE

// Display Resolution
#if (DISPLAY_ORIENTATION == ORIENTATION_NORMAL)
    #define LCD_H_RES 170
    #define LCD_V_RES 320
#elif (DISPLAY_ORIENTATION == ORIENTATION_ROTATE)
    #define LCD_H_RES 320
    #define LCD_V_RES 170
#endif

// SPI Pin Configuration
#define PIN_NUM_MOSI    4
#define PIN_NUM_CLK     5
#define PIN_NUM_CS      7
#define PIN_NUM_RST     14
#define PIN_NUM_DC      6

// SD Card Pins (if enabled)
#define PIN_NUM_MISO    19
#define PIN_NUM_SDCS    20

// SPI Configuration
#define LCD_HOST        SPI2_HOST
#define LCD_PIXEL_CLOCK_HZ (20 * 1000 * 1000)

// Display Buffer Configuration
#define LCD_DMA_LINES   (LCD_V_RES / 2)

// LVGL Configuration
#define LVGL_TICK_PERIOD_MS     2
#define LVGL_TASK_MAX_DELAY_MS  500
#define LVGL_TASK_MIN_DELAY_MS  1
#define LVGL_TASK_STACK_SIZE    (4 * 1024)
#define LVGL_TASK_PRIORITY      2

// Feature Flags
#define ENABLE_DISPLAY  1
#define ENABLE_TOUCH    0
#define ENABLE_SDCARD   0

// WiFi Configuration (dummy credentials for now)
#define WIFI_SSID       "wirelesss"
#define WIFI_PASSWORD   "apacaldahaideshimdighel"
#define WIFI_MAX_RETRY  5

// Application Configuration
#define APP_TAG         "MediaController"

#endif // APP_CONFIG_H
