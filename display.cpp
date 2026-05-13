#include "display.h"
#include "config.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif

// ESP32-S2 has FSPI as the user SPI bus. Bind the TFT to it explicitly so we
// can pick GPIO 11/12 for MOSI/SCLK (same wiring as the horloge project).
static SPIClass s_spi(FSPI);
static Adafruit_ST7789 s_tft(&s_spi, TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN);

static String  s_station = "";
static int     s_idx = 0, s_total = 0;
static String  s_title = "";
static String  s_status = "";
static String  s_wifi = "";
static bool    s_wifiOk = false;
static uint8_t s_vol = 0, s_volMax = MAX_VOLUME;
static bool    s_muted = false;

static uint8_t s_backlight = DEFAULT_BACKLIGHT;
static bool    s_dirty = true;
static int     s_titleScroll = 0;
static uint32_t s_lastScroll = 0;

#define TITLE_AREA_Y       136
#define TITLE_AREA_H        28
#define TITLE_FONT_W        12     // textSize 2 -> 6*2 px wide

// Use the full panel area (corners are not visibly clipped on this unit).
#define MARGIN              0
#define SAFE_X              MARGIN
#define SAFE_Y              MARGIN
#define SAFE_W              (TFT_WIDTH  - 2 * MARGIN)
#define SAFE_H              (TFT_HEIGHT - 2 * MARGIN)

static void applyBacklight() {
#if defined(TFT_BL_PIN) && TFT_BL_PIN >= 0
  // Simple PWM via ledc (channel 0).
  static bool s_init = false;
  if (!s_init) {
    ledcSetup(0, 5000, 8);
    ledcAttachPin(TFT_BL_PIN, 0);
    s_init = true;
  }
  uint32_t duty = map(s_backlight, 0, 100, 0, 255);
  ledcWrite(0, duty);
#endif
}

void display_begin() {
  // Bind FSPI to the actual TFT wiring before initialising the panel.
  s_spi.begin(TFT_SCLK_PIN, -1, TFT_MOSI_PIN, TFT_CS_PIN);

  // Full hardware reset of the ST7789 controller. The MCU may have rebooted
  // (watchdog / software restart) while the panel kept its state, leaving GRAM
  // and registers inconsistent. Pulsing RST clears that.
  pinMode(TFT_RST_PIN, OUTPUT);
  digitalWrite(TFT_RST_PIN, HIGH); delay(5);
  digitalWrite(TFT_RST_PIN, LOW);  delay(20);
  digitalWrite(TFT_RST_PIN, HIGH); delay(150);

  // Panel native resolution is 240 x 280 (portrait); the logical orientation
  // is selected below via setRotation().
  s_tft.init(240, 280, SPI_MODE0);
  s_tft.setSPISpeed(40000000);
  s_tft.setRotation(TFT_ROTATION);
  s_tft.fillScreen(COL_BG);

  applyBacklight();
  display_forceRedraw();
}

// ---------------- Splash screen ---------------------------------------------
void display_showBoot(const char* hostname) {
  s_tft.fillScreen(COL_BG);
  s_tft.setTextWrap(false);

  // Title
  s_tft.setTextColor(COL_ACCENT);
  s_tft.setTextSize(4);
  int16_t bx, by; uint16_t bw, bh;
  s_tft.getTextBounds("Radio", 0, 0, &bx, &by, &bw, &bh);
  s_tft.setCursor((TFT_WIDTH - (int16_t)bw) / 2, SAFE_Y + 30);
  s_tft.print("Radio");

  // Hostname (centered)
  if (hostname && *hostname) {
    s_tft.setTextColor(COL_FG);
    s_tft.setTextSize(2);
    s_tft.getTextBounds(hostname, 0, 0, &bx, &by, &bw, &bh);
    s_tft.setCursor((TFT_WIDTH - (int16_t)bw) / 2, SAFE_Y + 90);
    s_tft.print(hostname);
  }

  // Firmware version (small, bottom)
  s_tft.setTextColor(COL_MUTED);
  s_tft.setTextSize(1);
  const char* ver = FW_VERSION;
  s_tft.getTextBounds(ver, 0, 0, &bx, &by, &bw, &bh);
  s_tft.setCursor((TFT_WIDTH - (int16_t)bw) / 2, SAFE_Y + SAFE_H - 12);
  s_tft.print(ver);

  // Mark the main UI dirty so it repaints fully when we leave the splash.
  s_dirty = true;
}

void display_showIP(const char* ip) {
  if (!ip || !*ip) return;
  s_tft.fillRect(SAFE_X, 140, SAFE_W, 30, COL_BG);
  s_tft.setTextColor(COL_OK);
  s_tft.setTextSize(2);
  int16_t bx, by; uint16_t bw, bh;
  s_tft.getTextBounds(ip, 0, 0, &bx, &by, &bw, &bh);
  s_tft.setCursor((TFT_WIDTH - (int16_t)bw) / 2, 145);
  s_tft.print(ip);
  s_dirty = true;
}

void display_setStation(const String& name, int index, int total) {
  if (name != s_station || index != s_idx || total != s_total) {
    s_station = name; s_idx = index; s_total = total; s_dirty = true;
  }
}
void display_setStreamTitle(const String& title) {
  if (title != s_title) { s_title = title; s_titleScroll = 0; s_dirty = true; }
}
void display_setVolume(uint8_t v, uint8_t maxV, bool muted) {
  if (v != s_vol || maxV != s_volMax || muted != s_muted) {
    s_vol = v; s_volMax = maxV; s_muted = muted; s_dirty = true;
  }
}
void display_setStatus(const String& s) {
  if (s != s_status) { s_status = s; s_dirty = true; }
}
void display_setWifi(bool connected, const String& ssidOrIp) {
  if (connected != s_wifiOk || ssidOrIp != s_wifi) {
    s_wifiOk = connected; s_wifi = ssidOrIp; s_dirty = true;
  }
}

void display_setBacklight(uint8_t pct) {
  if (pct > 100) pct = 100;
  s_backlight = pct;
  applyBacklight();
}
uint8_t display_getBacklight() { return s_backlight; }
void    display_forceRedraw()  { s_dirty = true; }

static void drawHeader() {
  // Stay inside the rounded-corner safe area.
  s_tft.fillRoundRect(SAFE_X, SAFE_Y, SAFE_W, 32, 6, COL_ACCENT);
  s_tft.setTextColor(0xFFFF);
  s_tft.setTextSize(2);
  s_tft.setCursor(SAFE_X + 23, SAFE_Y + 8);
  s_tft.print("Radio");
  // Wi-Fi indicator (right side)
  s_tft.setTextSize(1);
  s_tft.setCursor(SAFE_X + SAFE_W - 70, SAFE_Y + 4);
  s_tft.print(s_wifiOk ? "WiFi OK" : "AP mode");
  s_tft.setCursor(SAFE_X + SAFE_W - 70, SAFE_Y + 16);
  s_tft.print(s_wifi.substring(0, 10));
}

static void drawStationCard() {
  int x = SAFE_X;
  int y = SAFE_Y + 36;
  int w = SAFE_W;
  const int h = 76;
  s_tft.fillRoundRect(x, y, w, h, 8, COL_CARD);
  s_tft.drawRoundRect(x, y, w, h, 8, 0x2186);

  s_tft.setTextColor(COL_MUTED);
  s_tft.setTextSize(1);
  s_tft.setCursor(x + 8, y + 6);
  char buf[24];
  snprintf(buf, sizeof(buf), "STATION  %d / %d", s_idx + 1, s_total);
  s_tft.print(buf);

  // Station name (large, centered)
  s_tft.setTextColor(COL_FG);
  s_tft.setTextSize(2);
  int16_t bx, by; uint16_t bw, bh;
  s_tft.getTextBounds(s_station, 0, 0, &bx, &by, &bw, &bh);
  int16_t cx = x + (w - (int16_t)bw) / 2;
  if (cx < x + 8) cx = x + 8;
  s_tft.setCursor(cx, y + 22);
  s_tft.print(s_station);

  // Status line
  s_tft.setTextColor(s_status.startsWith("Erreur") ? COL_ERR : COL_OK);
  s_tft.setTextSize(1);
  s_tft.getTextBounds(s_status, 0, 0, &bx, &by, &bw, &bh);
  cx = x + (w - (int16_t)bw) / 2; if (cx < x + 8) cx = x + 8;
  s_tft.setCursor(cx, y + h - 12);
  s_tft.print(s_status);
}

static void drawTitleArea() {
  s_tft.fillRect(SAFE_X, TITLE_AREA_Y, SAFE_W, TITLE_AREA_H, COL_BG);
  s_tft.setTextSize(2);
  s_tft.setTextColor(COL_MUTED);

  String t = s_title.length() ? s_title : String("(en attente du titre)");

  int16_t bx, by; uint16_t bw, bh;
  s_tft.getTextBounds(t, 0, 0, &bx, &by, &bw, &bh);
  int x;
  if ((int)bw <= SAFE_W) {
    x = SAFE_X + (SAFE_W - (int16_t)bw) / 2;
    s_tft.setCursor(x, TITLE_AREA_Y + 8);
    s_tft.print(t);
  } else {
    int total = (int)bw + 30;
    int off = s_titleScroll % total;
    s_tft.setCursor(SAFE_X - off, TITLE_AREA_Y + 8);
    s_tft.print(t);
    s_tft.setCursor(SAFE_X - off + total, TITLE_AREA_Y + 8);
    s_tft.print(t);
  }
}

static void drawVolume() {
  int y = 172;
  s_tft.fillRect(SAFE_X, y, SAFE_W, (SAFE_Y + SAFE_H) - y, COL_BG);
  s_tft.setTextColor(COL_MUTED);
  s_tft.setTextSize(1);
  s_tft.setCursor(SAFE_X, y);
  s_tft.print(s_muted ? "VOLUME (MUET)" : "VOLUME");

  int barX = SAFE_X, barY = y + 12, barW = SAFE_W, barH = 14;
  s_tft.drawRoundRect(barX, barY, barW, barH, 4, COL_MUTED);
  int fw = (int)((long)(barW - 4) * s_vol / s_volMax);
  uint16_t fill = s_muted ? COL_ERR : COL_OK;
  s_tft.fillRoundRect(barX + 2, barY + 2, fw, barH - 4, 3, fill);

  char buf[8];
  snprintf(buf, sizeof(buf), "%u/%u", s_vol, s_volMax);
  s_tft.setTextColor(COL_FG);
  s_tft.setCursor(barX, barY + barH + 3);
  s_tft.print(buf);
}

void display_loop() {
  uint32_t now = millis();
  // Scrolling title (50 ms)
  if (s_title.length() && now - s_lastScroll >= 50) {
    s_lastScroll = now;
    s_titleScroll += 2;
    drawTitleArea();
  }
  if (!s_dirty) return;
  s_dirty = false;
  s_tft.fillScreen(COL_BG);
  drawHeader();
  drawStationCard();
  drawTitleArea();
  drawVolume();
}
