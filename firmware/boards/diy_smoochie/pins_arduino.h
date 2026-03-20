//
// M5Stack Cardputer (ESP32-S3)
//

#pragma once
#include <stdint.h>

// ─── SD SPI Bus ───────────────────────────────────────────
#define SPI_SS_PIN    43
#define SPI_MOSI_PIN  12
#define SPI_MISO_PIN  11
#define SPI_SCK_PIN   13

static const uint8_t SS   = SPI_SS_PIN;
static const uint8_t MOSI = SPI_MOSI_PIN;
static const uint8_t MISO = SPI_MISO_PIN;
static const uint8_t SCK  = SPI_SCK_PIN;

// ─── I2C (Grove) ──────────────────────────────────────────
#define GROVE_SDA  47
#define GROVE_SCL  48

static const uint8_t SDA = 47;
static const uint8_t SCL = 48;

// ─── LCD (own SPI bus: MOSI=35, SCK=36) ──────────────────
#define LCD_CS   7
#define LCD_DC   15
#define LCD_RST  16
#define LCD_BL   6

// ─── SD Card ──────────────────────────────────────────────
#define SD_CS  12

// ─── Keyboard (74HC138 GPIO matrix) ───────────────────────

// ─── IR Emitter ───────────────────────────────────────────
#define IR_TX  44

// ─── RGB LED (SK6812) ─────────────────────────────────────
#define RGB_LED  45

// ─── Speaker (I2S) ────────────────────────────────────────
#define SPK_BCLK      41
#define SPK_WCLK      43
#define SPK_DOUT      42
#define SPK_I2S_PORT  I2S_NUM_1

// ─── Boot / shoulder button ───────────────────────────────
#define BTN_BOOT  0

// ─── TFT_eSPI config ──────────────────────────────────────
#define DISABLE_ALL_LIBRARY_WARNINGS 1
#define USER_SETUP_LOADED 1

#define ST7789_2_DRIVER
#define TFT_RGB_ORDER  TFT_BGR
#define TFT_WIDTH   170
#define TFT_HEIGHT  320
#define TFT_MOSI    17
#define TFT_SCLK    18
#define TFT_MISO    8
#define TFT_CS      LCD_CS
#define TFT_DC      LCD_DC
#define TFT_RST     LCD_RST
#define TFT_BL      LCD_BL
#define TFT_BACKLIGHT_ON  1
#define TFT_INVERSION_ON
#define TFT_DEFAULT_ORIENTATION  1
#define USE_HSPI_PORT
#define TOUCH_CS    -1
#define SMOOTH_FONT 1
#define SPI_FREQUENCY       27000000
#define SPI_READ_FREQUENCY  16000000
