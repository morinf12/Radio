// =============================================================================
//  ESP32-S2 Internet Radio - main entry point
//  Hardware: ESP32-S2 + ST7789V3 (240x280) + PCM5102 I2S DAC
//            + KY-040 rotary encoder + 2 push buttons (prev/next)
// =============================================================================
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

#include "config.h"
#include "display.h"
#include "audio_player.h"
#include "controls.h"
#include "stations.h"
#include "webui.h"

static void playCurrent() {
  int idx = stations_currentIndex();
  if (idx < 0 || idx >= stations_count()) return;
  const Station& st = stations_get(idx);
  display_setStation(st.name, idx, stations_count());
  display_setStatus("Connexion...");
  display_setStreamTitle("");
  audio_play(st.url);
  display_setStatus(audio_isPlaying() ? "En cours" : "Erreur de connexion");
}

static void changeStation(int delta) {
  int n = stations_count();
  if (n == 0) return;
  int idx = (stations_currentIndex() + delta + n) % n;
  stations_setCurrentIndex(idx);
  playCurrent();
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println(F("\n[Radio] boot"));

  // Restore display backlight
  uint8_t bl = DEFAULT_BACKLIGHT;
  {
    Preferences p;
    if (p.begin("radio", true)) {
      bl = p.getUChar("bl", DEFAULT_BACKLIGHT);
      p.end();
    }
  }
  display_begin();
  display_setBacklight(bl);
  display_setStatus("Demarrage...");
  display_setStation("Radio", 0, 0);

  controls_begin();
  stations_begin();
  audio_begin();
  display_setVolume(audio_getVolume(), MAX_VOLUME, false);

  // Wi-Fi + web server (also handles captive-portal AP fallback)
  webui_begin();
  display_setWifi(!webui_isApMode(), webui_currentSsidOrIp());

  if (!webui_isApMode() && stations_count() > 0) {
    playCurrent();
  } else {
    display_setStatus(webui_isApMode()
      ? String("Connectez-vous au Wi-Fi ") + WIFI_AP_SSID
      : String("Aucune station"));
    int idx = stations_currentIndex();
    if (idx >= 0 && idx < stations_count()) {
      display_setStation(stations_get(idx).name, idx, stations_count());
    }
  }
}

void loop() {
  // Audio MUST be pumped continuously to keep the I2S buffer full.
  audio_loop();

  // Controls
  CtrlEvent ev = controls_poll();
  switch (ev) {
    case CTRL_ROT_CW:    audio_volumeUp();
                         display_setVolume(audio_getVolume(), MAX_VOLUME, audio_isMuted());
                         break;
    case CTRL_ROT_CCW:   audio_volumeDown();
                         display_setVolume(audio_getVolume(), MAX_VOLUME, audio_isMuted());
                         break;
    case CTRL_ENC_PRESS:
      if (audio_isPlaying()) { audio_setMute(!audio_isMuted());
                               display_setVolume(audio_getVolume(), MAX_VOLUME, audio_isMuted()); }
      else                    playCurrent();
      break;
    case CTRL_ENC_LONG:  // reserved for future menu
      break;
    case CTRL_PREV:      changeStation(-1); break;
    case CTRL_NEXT:      changeStation(+1); break;
    default: break;
  }

  // Streaming title from ICY metadata
  static String s_lastTitle;
  const String& t = audio_streamTitle();
  if (t != s_lastTitle) { s_lastTitle = t; display_setStreamTitle(t); }

  // Wi-Fi status update (cheap; once a second)
  static uint32_t s_lastWifi = 0;
  if (millis() - s_lastWifi > 1000) {
    s_lastWifi = millis();
    display_setWifi(!webui_isApMode(), webui_currentSsidOrIp());
  }

  display_loop();
  webui_loop();
}
