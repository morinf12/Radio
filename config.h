#pragma once
#include <Arduino.h>

// =============================================================================
//  ESP32-S2 Internet Radio - pin map
// =============================================================================
// Avoid: GPIO 19/20 (USB D-/D+), 26-32 (SPI flash/PSRAM), 22-25 (don't exist).
// Strapping pins to use with care: 0, 45, 46.

// ---------------- ST7789V3 240x280 TFT (SPI) ---------------------------------
// Same pinout as the "horloge" project so the same wired prototype can be used.
#define TFT_SCLK_PIN   12   // SCL
#define TFT_MOSI_PIN   11   // SDA
#define TFT_RST_PIN     9   // RES
#define TFT_DC_PIN      7   // DC
#define TFT_CS_PIN      5   // CS
#define TFT_BL_PIN      3   // BLK (backlight, PWM)

#define TFT_WIDTH     280   // logical width  (after rotation)
#define TFT_HEIGHT    240   // logical height (after rotation)
#define TFT_ROTATION    3   // landscape, rotated 90 deg counter-clockwise (USB at top)

// ---------------- PCM5102 I2S DAC --------------------------------------------
//   VIN -> 3.3V or 5V    GND -> GND
//   SCK -> GND (use internal PLL)
//   BCK -> I2S_BCLK
//   DIN -> I2S_DOUT
//   LCK -> I2S_LRC
//   FMT/XMT/FLT -> GND (default I2S, 16-bit, soft-mute disabled)
#define I2S_BCLK_PIN   16
#define I2S_LRC_PIN    17
#define I2S_DOUT_PIN   18

// ---------------- Navigation buttons (active LOW, internal pull-up) ----------
// Same pinout as the "horloge" project (Up/Down/Left/Right/A/B).
//   UP    -> volume up
//   DOWN  -> volume down
//   LEFT  -> previous station
//   RIGHT -> next station
//   A     -> play / pause (mute toggle)
//   B     -> reserved (menu / long-press equivalent)
#define BTN_UP_PIN      1
#define BTN_DOWN_PIN    2
#define BTN_LEFT_PIN    4
#define BTN_RIGHT_PIN   6
#define BTN_A_PIN       8
#define BTN_B_PIN      10

// ---------------- Wi-Fi AP (captive portal) ----------------------------------
#define WIFI_AP_SSID   "Radio"
#define WIFI_AP_PASS   "radio1234"      // >= 8 chars
#define WIFI_AP_CHAN    6

// ---------------- Audio defaults ---------------------------------------------
#define DEFAULT_VOLUME       12         // 0..21 (audio library scale)
#define MAX_VOLUME           21
#define DEFAULT_BACKLIGHT   80          // 0..100 %

// ---------------- Theme colors (RGB565) --------------------------------------
#define COL_BG          0x10A2          // #0e1726
#define COL_CARD        0x1985          // #161f33
#define COL_ACCENT      0x1CFD          // #1f6feb
#define COL_FG          0xEF7D          // #e6edf3
#define COL_MUTED       0x9D75          // #9fb3d1
#define COL_OK          0x7FCF          // #7ee787
#define COL_ERR         0xF44C          // #f08a8a
