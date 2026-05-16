#pragma once
#include <Arduino.h>

// =============================================================================
//  ESP32 Internet Radio - pin map (Wemos MiniKit / ESP32-D0WDQ6)
// =============================================================================
// Avoid: GPIO 6-11 (SPI flash), 20/24/28-31 (don't exist), 34-39 (input-only,
// no internal pull-ups, no output). Strapping pins to use with care: 0, 2, 5,
// 12, 15.

// ---------------- ST7789V3 240x280 TFT (SPI - HSPI) --------------------------
// HSPI is used because VSPI's default CLK (18) is now the backlight pin and
// the chosen SCLK (5) is more conveniently driven by HSPI. The TFT_eSPI lib
// honours whatever pins we set here regardless of the underlying SPI bus.
#define TFT_SCLK_PIN   22   // SCL (moved off strapping pin 5 to avoid boot hang)
#define TFT_MOSI_PIN   23   // SDA
#define TFT_RST_PIN    33   // RES
#define TFT_DC_PIN     19   // DC
#define TFT_CS_PIN     26   // CS
#define TFT_BL_PIN     18   // BLK (backlight, PWM)

#define TFT_WIDTH     280   // logical width  (after rotation)
#define TFT_HEIGHT    240   // logical height (after rotation)
#define TFT_ROTATION    3   // landscape, rotated 90 deg counter-clockwise (USB at top)

// ---------------- PCM5102 I2S DAC --------------------------------------------
//   VIN -> 3V3 pin on the MiniKit (board's 3.3V LDO; clean rail).
//   GND -> any GND pin
//   SCK -> GND (use internal PLL; do NOT leave floating)
//   BCK -> I2S_BCLK
//   DIN -> I2S_DOUT
//   LCK -> I2S_LRC
//   FMT/XMT/FLT/DEMP -> GND (default I2S, 16-bit, soft-mute off, no de-emph)
#define I2S_BCLK_PIN   21
#define I2S_LRC_PIN    16
#define I2S_DOUT_PIN   17

// ---------------- Navigation buttons (active LOW) ----------------------------
//   UP    -> volume up
//   DOWN  -> volume down
//   LEFT  -> previous station
//   RIGHT -> next station
//   A     -> play / pause (mute toggle)
//   B     -> reserved (menu / long-press equivalent)
// Pins 14, 27, 13, 4 have internal pull-ups (INPUT_PULLUP works).
// Pins 34 and 35 are INPUT-ONLY with NO internal pull-up - wire an external
// 10 kOhm pull-up resistor to 3V3 on each, button shorts to GND.
#define BTN_UP_PIN     14
#define BTN_DOWN_PIN   27
#define BTN_LEFT_PIN   13
#define BTN_RIGHT_PIN   4   // moved off 33 (now TFT_RES)
#define BTN_A_PIN      34   // input-only: requires EXTERNAL 10k pull-up to 3V3
#define BTN_B_PIN      35   // input-only: requires EXTERNAL 10k pull-up to 3V3

// ---------------- Wi-Fi AP (captive portal) ----------------------------------
#define WIFI_AP_SSID   "Radio"
#define WIFI_AP_PASS   "radio1234"      // >= 8 chars
#define WIFI_AP_CHAN    6

// ---------------- Audio defaults ---------------------------------------------
#define DEFAULT_VOLUME       12         // 0..21 (audio library scale)
#define MAX_VOLUME           21
#define DEFAULT_BACKLIGHT   80          // 0..100 %

// ---------------- Build switches ---------------------------------------------
// Define DISPLAY_DISABLED to skip all TFT init / drawing. Useful when the
// panel is not yet wired so the rest of the firmware (Wi-Fi, audio, web UI,
// buttons) can be brought up without crashing on SPI traffic to nowhere.
//#define DISPLAY_DISABLED

// ---------------- Theme colors (RGB565) --------------------------------------
#define COL_BG          0x10A2          // #0e1726
#define COL_CARD        0x1985          // #161f33
#define COL_ACCENT      0x1CFD          // #1f6feb
#define COL_FG          0xEF7D          // #e6edf3
#define COL_MUTED       0x9D75          // #9fb3d1
#define COL_OK          0x7FCF          // #7ee787
#define COL_ERR         0xF44C          // #f08a8a
