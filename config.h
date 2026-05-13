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

#define TFT_WIDTH     240
#define TFT_HEIGHT    280
#define TFT_ROTATION    0   // portrait (240 wide x 280 tall)

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

// ---------------- Rotary encoder (KY-040) + push -----------------------------
//   GND -> GND   + -> 3.3V
//   CLK -> ENC_A   DT -> ENC_B   SW -> ENC_SW (active LOW, internal pull-up)
#define ENC_A_PIN      33
#define ENC_B_PIN      34
#define ENC_SW_PIN     35

// ---------------- Front buttons (active LOW, internal pull-up) ---------------
#define BTN_PREV_PIN   37   // station -1
#define BTN_NEXT_PIN   38   // station +1

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
