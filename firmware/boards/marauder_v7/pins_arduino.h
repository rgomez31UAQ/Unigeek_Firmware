//
// WiFi Marauder v7 (ESP32, 4MB flash)
// ILI9341 240x320 display, 5 buttons active LOW with external pull-ups.
// GPIOs 34-39 are input-only on ESP32 — no internal pull-up, use INPUT mode.
//

#pragma once
#include <stdint.h>

// ─── SPI bus (display + SD share: SCK=18, MISO=19, MOSI=23) ──
#define SPI_MOSI_PIN  23
#define SPI_MISO_PIN  19
#define SPI_SCK_PIN   18

static const uint8_t SS   = 4;   // SD CS
static const uint8_t MOSI = SPI_MOSI_PIN;
static const uint8_t MISO = SPI_MISO_PIN;
static const uint8_t SCK  = SPI_SCK_PIN;

// ─── I2C (Grove) ──────────────────────────────────────────
#define GROVE_SDA  13
#define GROVE_SCL  15

static const uint8_t SDA = GROVE_SDA;
static const uint8_t SCL = GROVE_SCL;

// ─── LCD ──────────────────────────────────────────────────
#define LCD_CS   17
#define LCD_DC   26
#define LCD_BL   32   // backlight: HIGH = on, PWM capable

// ─── SD Card ──────────────────────────────────────────────
#define SD_CS  4

// ─── Navigation buttons (active LOW, external pull-ups) ───
// GPIO 34/35/36/39: input-only, no internal pull-up — use INPUT mode
#define BTN_SEL    34   // OK / Select  (RTC_GPIO4, usable for ext0 wakeup)
#define BTN_UP     36   // Up
#define BTN_DOWN   35   // Down
#define BTN_RIGHT  39   // Right
#define BTN_LEFT   13   // Left / Back

// ─── App feature flags ────────────────────────────────────
#define APP_MENU_POWER_OFF   // deep sleep via BTN_SEL wakeup

// ─── TFT_eSPI config ──────────────────────────────────────
#define DISABLE_ALL_LIBRARY_WARNINGS 1
#define USER_SETUP_LOADED 1

#define ILI9341_DRIVER
#define TFT_WIDTH   240
#define TFT_HEIGHT  320
#define TFT_MOSI    SPI_MOSI_PIN
#define TFT_SCLK    SPI_SCK_PIN
#define TFT_MISO    SPI_MISO_PIN
#define TFT_CS      LCD_CS
#define TFT_DC      LCD_DC
#define TFT_RST     -1
#define TFT_BL      LCD_BL
#define TFT_BACKLIGHT_ON  1
#define TFT_DEFAULT_ORIENTATION  0
#define USE_HSPI_PORT
#define TOUCH_CS    -1
#define SMOOTH_FONT 1
#define SPI_FREQUENCY       27000000
#define SPI_READ_FREQUENCY  16000000