#include "display.h"
#include "config.h"
#include "audio_player.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <esp_heap_caps.h>

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif

// On classic ESP32, FSPI is SPI1 - which is the on-chip flash bus. Binding
// the TFT to it deadlocks the CPU on the first SPI transaction. Use HSPI
// (SPI2) instead, which is the standard user SPI on this part. On S2/S3
// FSPI happens to be SPI2 as well, so HSPI is a safe choice on both.
static SPIClass s_spi(HSPI);
static Adafruit_ST7789 s_tft(&s_spi, TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN);

// ---------------- Off-screen framebuffer (PSRAM) -----------------------------
// All drawing happens in a 16-bit canvas that lives in PSRAM, then the full
// frame is pushed to the panel in a single SPI transaction. This removes the
// flicker / tearing you'd otherwise see while individual rectangles and text
// glyphs are drawn one by one directly on the TFT.
//
// 280 * 240 * 2 = 134 400 bytes -> easily fits in the 2 MB PSRAM of the
// Wemos S2 Mini.
class PSCanvas16 : public GFXcanvas16 {
public:
  PSCanvas16(uint16_t w, uint16_t h)
    : GFXcanvas16(w, h, /*allocate_buffer=*/false) {
    size_t bytes = (size_t)w * (size_t)h * 2;
    // Prefer PSRAM; only fall back to internal RAM if there's plenty of it
    // free (this board is a no-PSRAM ESP32 with ~290 KB heap at boot, and a
    // 134 KB canvas would starve WiFi+audio). Refuse the allocation otherwise
    // and let display_begin() switch to direct-to-panel rendering.
    buffer = (uint16_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    if (!buffer) {
      size_t freeRam = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
      if (freeRam > bytes + 80 * 1024) {
        buffer = (uint16_t*)malloc(bytes);
      }
    }
    if (buffer) memset(buffer, 0, bytes);
    buffer_owned = false;   // long-lived, never destroyed
  }
  bool ok() const { return buffer != nullptr; }
};

static PSCanvas16* s_canvas = nullptr;
static Adafruit_GFX* s_gfx = nullptr;     // points at canvas if allocated, else &s_tft
#define g (*s_gfx)               // drawing target

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
static bool    s_titleOverflows = false;  // true when title is wider than screen (needs scrolling)

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
#ifdef DISPLAY_DISABLED
  return;
#elif defined(TFT_BL_PIN) && TFT_BL_PIN >= 0
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

// Push the PSRAM canvas to the panel as a single bitmap blit. Adafruit_SPITFT
// turns this into one setAddrWindow() + a streamed writePixels(), so there is
// no visible "drawing in progress" flicker on the screen. When no canvas is
// available we drew directly to the panel and this is a no-op.
static void flush() {
  if (!s_canvas || !s_canvas->getBuffer()) return;
  s_tft.startWrite();
  s_tft.setAddrWindow(0, 0, TFT_WIDTH, TFT_HEIGHT);
  s_tft.writePixels(s_canvas->getBuffer(), (uint32_t)TFT_WIDTH * TFT_HEIGHT);
  s_tft.endWrite();
}

void display_begin() {
#ifdef DISPLAY_DISABLED
  // TFT not wired - skip all SPI / panel init. The rest of the firmware
  // (Wi-Fi, audio, web UI, buttons) still uses the display_setXxx() setters
  // to update internal state, but no pixels are pushed anywhere.
  Serial.println(F("[Display] DISABLED (DISPLAY_DISABLED set in config.h)"));
  return;
#endif
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

  // Off-screen framebuffer in PSRAM. The canvas itself has no rotation set;
  // it is laid out in the same orientation the panel uses after setRotation(),
  // so its (0,0) maps directly to the TFT's (0,0).
  if (!s_canvas) s_canvas = new PSCanvas16(TFT_WIDTH, TFT_HEIGHT);
  if (s_canvas && s_canvas->ok()) {
    s_gfx = s_canvas;
    Serial.println(F("[Display] using off-screen canvas"));
  } else {
    // No PSRAM and not enough internal RAM for a full-screen canvas. Draw
    // directly to the panel - a touch of flicker, but it boots.
    s_gfx = &s_tft;
    Serial.println(F("[Display] no canvas - drawing direct to panel"));
  }
  g.fillScreen(COL_BG);

  applyBacklight();
  display_forceRedraw();
}

// ---------------- Splash screen ---------------------------------------------
void display_showBoot(const char* hostname) {
  if (!s_canvas) return;   // DISPLAY_DISABLED or PSRAM alloc failed
  g.fillScreen(COL_BG);
  g.setTextWrap(false);

  // Title
  g.setTextColor(COL_ACCENT);
  g.setTextSize(4);
  int16_t bx, by; uint16_t bw, bh;
  g.getTextBounds("Radio", 0, 0, &bx, &by, &bw, &bh);
  g.setCursor((TFT_WIDTH - (int16_t)bw) / 2, SAFE_Y + 30);
  g.print("Radio");

  // Hostname (centered)
  if (hostname && *hostname) {
    g.setTextColor(COL_FG);
    g.setTextSize(2);
    g.getTextBounds(hostname, 0, 0, &bx, &by, &bw, &bh);
    g.setCursor((TFT_WIDTH - (int16_t)bw) / 2, SAFE_Y + 90);
    g.print(hostname);
  }

  // Firmware version (small, bottom)
  g.setTextColor(COL_MUTED);
  g.setTextSize(1);
  const char* ver = FW_VERSION;
  g.getTextBounds(ver, 0, 0, &bx, &by, &bw, &bh);
  g.setCursor((TFT_WIDTH - (int16_t)bw) / 2, SAFE_Y + SAFE_H - 12);
  g.print(ver);

  flush();
  // Mark the main UI dirty so it repaints fully when we leave the splash.
  s_dirty = true;
}

void display_showIP(const char* ip) {
  if (!ip || !*ip) return;
  if (!s_canvas) return;   // DISPLAY_DISABLED or PSRAM alloc failed
  g.fillRect(SAFE_X, 140, SAFE_W, 30, COL_BG);
  g.setTextColor(COL_OK);
  g.setTextSize(2);
  int16_t bx, by; uint16_t bw, bh;
  g.getTextBounds(ip, 0, 0, &bx, &by, &bw, &bh);
  g.setCursor((TFT_WIDTH - (int16_t)bw) / 2, 145);
  g.print(ip);
  flush();
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
  g.fillRoundRect(SAFE_X, SAFE_Y, SAFE_W, 32, 6, COL_ACCENT);
  g.setTextColor(0xFFFF);
  g.setTextSize(2);
  g.setCursor(SAFE_X + 23, SAFE_Y + 8);
  g.print("Radio");
  // Wi-Fi indicator (right side)
  g.setTextSize(1);
  g.setCursor(SAFE_X + SAFE_W - 70, SAFE_Y + 4);
  g.print(s_wifiOk ? "WiFi OK" : "AP mode");
  g.setCursor(SAFE_X + SAFE_W - 70, SAFE_Y + 16);
  g.print(s_wifi.substring(0, 10));
}

static void drawStationCard() {
  int x = SAFE_X;
  int y = SAFE_Y + 36;
  int w = SAFE_W;
  const int h = 76;
  g.fillRoundRect(x, y, w, h, 8, COL_CARD);
  g.drawRoundRect(x, y, w, h, 8, 0x2186);

  g.setTextColor(COL_MUTED);
  g.setTextSize(1);
  g.setCursor(x + 8, y + 6);
  char buf[24];
  snprintf(buf, sizeof(buf), "STATION  %d / %d", s_idx + 1, s_total);
  g.print(buf);

  // Station name (large, centered)
  g.setTextColor(COL_FG);
  g.setTextSize(2);
  int16_t bx, by; uint16_t bw, bh;
  g.getTextBounds(s_station, 0, 0, &bx, &by, &bw, &bh);
  int16_t cx = x + (w - (int16_t)bw) / 2;
  if (cx < x + 8) cx = x + 8;
  g.setCursor(cx, y + 22);
  g.print(s_station);

  // Status line
  g.setTextColor(s_status.startsWith("Erreur") ? COL_ERR : COL_OK);
  g.setTextSize(1);
  g.getTextBounds(s_status, 0, 0, &bx, &by, &bw, &bh);
  cx = x + (w - (int16_t)bw) / 2; if (cx < x + 8) cx = x + 8;
  g.setCursor(cx, y + h - 12);
  g.print(s_status);
}

static void drawTitleArea() {
  g.fillRect(SAFE_X, TITLE_AREA_Y, SAFE_W, TITLE_AREA_H, COL_BG);
  g.setTextSize(2);
  g.setTextColor(COL_MUTED);

  String t = s_title.length() ? s_title : String("(en attente du titre)");

  int16_t bx, by; uint16_t bw, bh;
  g.getTextBounds(t, 0, 0, &bx, &by, &bw, &bh);
  int x;
  if ((int)bw <= SAFE_W) {
    s_titleOverflows = false;
    x = SAFE_X + (SAFE_W - (int16_t)bw) / 2;
    g.setCursor(x, TITLE_AREA_Y + 8);
    g.print(t);
  } else {
    s_titleOverflows = true;
    int total = (int)bw + 30;
    int off = s_titleScroll % total;
    g.setCursor(SAFE_X - off, TITLE_AREA_Y + 8);
    g.print(t);
    g.setCursor(SAFE_X - off + total, TITLE_AREA_Y + 8);
    g.print(t);
  }
}

static void drawVolume() {
  int y = 172;
  g.fillRect(SAFE_X, y, SAFE_W, (SAFE_Y + SAFE_H) - y, COL_BG);
  g.setTextColor(COL_MUTED);
  g.setTextSize(1);
  g.setCursor(SAFE_X, y);
  g.print(s_muted ? "VOLUME (MUET)" : "VOLUME");

  int barX = SAFE_X, barY = y + 12, barW = SAFE_W, barH = 14;
  g.drawRoundRect(barX, barY, barW, barH, 4, COL_MUTED);
  int fw = (int)((long)(barW - 4) * s_vol / s_volMax);
  uint16_t fill = s_muted ? COL_ERR : COL_OK;
  g.fillRoundRect(barX + 2, barY + 2, fw, barH - 4, 3, fill);

  char buf[8];
  snprintf(buf, sizeof(buf), "%u/%u", s_vol, s_volMax);
  g.setTextColor(COL_FG);
  g.setCursor(barX, barY + barH + 3);
  g.print(buf);
}

void display_loop() {
#ifdef DISPLAY_DISABLED
  return;
#endif
  // While audio is actively streaming, freeze the TFT entirely. A full-frame
  // blit takes ~27 ms over SPI at 40 MHz on the single-core S2 and starves
  // the I2S DMA feeder, causing audible drop-outs. Any pending change is
  // remembered in s_dirty and will be flushed the next time playback stops.
  if (audio_isPlaying()) return;

  uint32_t now = millis();
  bool needFlush = false;
  if (s_titleOverflows && s_title.length() && now - s_lastScroll >= 100) {
    s_lastScroll = now;
    s_titleScroll += 4;
    drawTitleArea();
    needFlush = true;
  }
  if (s_dirty) {
    s_dirty = false;
    g.fillScreen(COL_BG);
    drawHeader();
    drawStationCard();
    drawTitleArea();
    drawVolume();
    needFlush = true;
  }
  if (needFlush) flush();
}
