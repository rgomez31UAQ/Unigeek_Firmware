//
// LilyGO T-Display 16MB (ESP32)
//

#pragma once
#include <stdint.h>

// ─── SPI Bus ──────────────────────────────────────────────
#define SPI_SS_PIN    33
#define SPI_MOSI_PIN  26
#define SPI_MISO_PIN  27
#define SPI_SCK_PIN   25

static const uint8_t SS   = SPI_SS_PIN;
static const uint8_t MOSI = SPI_MOSI_PIN;
static const uint8_t MISO = SPI_MISO_PIN;
static const uint8_t SCK  = SPI_SCK_PIN;

// ─── I2C (Grove) ─────────────────────────────────────────
#define GROVE_SDA  21
#define GROVE_SCL  22

static const uint8_t SDA = GROVE_SDA;
static const uint8_t SCL = GROVE_SCL;

// ─── LCD ──────────────────────────────────────────────────
#define LCD_BL      4
#define LCD_BL_CH   7

// ─── ADC Power Enable ────────────────────────────────────
#define ADC_EN  14

// ─── Battery (ADC) ───────────────────────────────────────
#define BAT_ADC_PIN  34

// ─── Buttons ──────────────────────────────────────────────
#define BTN_UP  0    // top button
#define BTN_B   35   // bottom button

// ─── IR Transmitter ──────────────────────────────────────
#define IR_TX_PIN  2

// ─── TFT_eSPI config ─────────────────────────────────────
#define DISABLE_ALL_LIBRARY_WARNINGS 1
#define USER_SETUP_LOADED 1

#define ST7789_DRIVER
#define TFT_WIDTH   135
#define TFT_HEIGHT  240
#define CGRAM_OFFSET 1
#define TFT_MOSI    19
#define TFT_SCLK    18
#define TFT_CS      5
#define TFT_DC      16
#define TFT_RST     23
#define TFT_BL      -1
#define TFT_BACKLIGHT_ON HIGH
#define TFT_DEFAULT_ORIENTATION 3
#define TOUCH_CS    -1
#define SMOOTH_FONT
#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  6000000
