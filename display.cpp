#include "display.h"
#include "config.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

static Adafruit_ST7789 s_tft(TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN);

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

#define TITLE_AREA_Y       170
#define TITLE_AREA_H        40
#define TITLE_FONT_W        12     // textSize 2 -> 6*2 px wide

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
  pinMode(TFT_BL_PIN, OUTPUT);
  digitalWrite(TFT_BL_PIN, LOW);

  // ST7789V3 240x280: use the matching Adafruit init.
  s_tft.init(TFT_WIDTH, TFT_HEIGHT, SPI_MODE0);
  s_tft.setSPISpeed(40000000);
  s_tft.setRotation(TFT_ROTATION);
  s_tft.fillScreen(COL_BG);

  applyBacklight();
  display_forceRedraw();
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
  s_tft.fillRect(0, 0, TFT_WIDTH, 36, COL_ACCENT);
  s_tft.setTextColor(0xFFFF);
  s_tft.setTextSize(2);
  s_tft.setCursor(8, 10);
  s_tft.print("Radio");
  // Wi-Fi indicator (right side)
  s_tft.setTextSize(1);
  s_tft.setCursor(TFT_WIDTH - 60, 6);
  s_tft.print(s_wifiOk ? "WiFi OK" : "AP mode");
  s_tft.setCursor(TFT_WIDTH - 60, 18);
  s_tft.print(s_wifi.substring(0, 10));
}

static void drawStationCard() {
  int y = 46;
  s_tft.fillRoundRect(8, y, TFT_WIDTH - 16, 100, 8, COL_CARD);
  s_tft.drawRoundRect(8, y, TFT_WIDTH - 16, 100, 8, 0x2186);

  s_tft.setTextColor(COL_MUTED);
  s_tft.setTextSize(1);
  s_tft.setCursor(16, y + 8);
  char buf[24];
  snprintf(buf, sizeof(buf), "STATION  %d / %d", s_idx + 1, s_total);
  s_tft.print(buf);

  // Station name (large, centered)
  s_tft.setTextColor(COL_FG);
  s_tft.setTextSize(2);
  int16_t bx, by; uint16_t bw, bh;
  s_tft.getTextBounds(s_station, 0, 0, &bx, &by, &bw, &bh);
  int16_t cx = (TFT_WIDTH - (int16_t)bw) / 2;
  if (cx < 16) cx = 16;
  s_tft.setCursor(cx, y + 30);
  s_tft.print(s_station);

  // Status line
  s_tft.setTextColor(s_status.startsWith("Erreur") ? COL_ERR : COL_OK);
  s_tft.setTextSize(1);
  s_tft.getTextBounds(s_status, 0, 0, &bx, &by, &bw, &bh);
  cx = (TFT_WIDTH - (int16_t)bw) / 2; if (cx < 16) cx = 16;
  s_tft.setCursor(cx, y + 78);
  s_tft.print(s_status);
}

static void drawTitleArea() {
  s_tft.fillRect(0, TITLE_AREA_Y, TFT_WIDTH, TITLE_AREA_H, COL_BG);
  s_tft.setTextSize(2);
  s_tft.setTextColor(COL_MUTED);

  String t = s_title.length() ? s_title : String("(en attente du titre)");

  int16_t bx, by; uint16_t bw, bh;
  s_tft.getTextBounds(t, 0, 0, &bx, &by, &bw, &bh);
  int x;
  if ((int)bw <= TFT_WIDTH - 16) {
    x = (TFT_WIDTH - (int16_t)bw) / 2;
    s_tft.setCursor(x, TITLE_AREA_Y + 10);
    s_tft.print(t);
  } else {
    int total = (int)bw + 30;
    int off = s_titleScroll % total;
    s_tft.setCursor(8 - off, TITLE_AREA_Y + 10);
    s_tft.print(t);
    s_tft.setCursor(8 - off + total, TITLE_AREA_Y + 10);
    s_tft.print(t);
  }
}

static void drawVolume() {
  int y = 222;
  s_tft.fillRect(0, y, TFT_WIDTH, 50, COL_BG);
  s_tft.setTextColor(COL_MUTED);
  s_tft.setTextSize(1);
  s_tft.setCursor(8, y);
  s_tft.print(s_muted ? "VOLUME (MUET)" : "VOLUME");

  int barX = 8, barY = y + 14, barW = TFT_WIDTH - 16, barH = 16;
  s_tft.drawRoundRect(barX, barY, barW, barH, 4, COL_MUTED);
  int fw = (int)((long)(barW - 4) * s_vol / s_volMax);
  uint16_t fill = s_muted ? COL_ERR : COL_OK;
  s_tft.fillRoundRect(barX + 2, barY + 2, fw, barH - 4, 3, fill);

  char buf[8];
  snprintf(buf, sizeof(buf), "%u/%u", s_vol, s_volMax);
  s_tft.setTextColor(COL_FG);
  s_tft.setCursor(barX, barY + barH + 4);
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
