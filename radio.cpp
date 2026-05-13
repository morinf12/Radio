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
  display_setStreamTitle("");
  // connecttohost() is async: a 'true' here only means the HTTP request was
  // dispatched. Actual playback starts a few audio_loop() iterations later,
  // so we mark the UI as "Connexion..." and let loop() promote to
  // "En cours" once isRunning() becomes true (or back to an error after a
  // timeout if it never does).
  bool ok = audio_play(st.url);
  display_setStatus(ok ? "Connexion..." : "Erreur de connexion");
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

  // Splash screen (hostname is what the web UI publishes via mDNS / DHCP).
  String hostname;
  {
    Preferences p;
    if (p.begin("wifi", true)) {
      hostname = p.getString("hostname", "radio");
      p.end();
    }
    if (hostname.length() == 0) hostname = "radio";
  }
  display_showBoot(hostname.c_str());

  controls_begin();
  stations_begin();
  audio_begin();
  display_setVolume(audio_getVolume(), MAX_VOLUME, false);

  // Wi-Fi + web server (also handles captive-portal AP fallback)
  webui_begin();
  display_setWifi(!webui_isApMode(), webui_currentSsidOrIp());

  // Show IP on the splash screen for a couple of seconds before switching to
  // the playback UI.
  display_showIP(webui_currentSsidOrIp().c_str());
  delay(2000);

  display_setStatus("Demarrage...");
  display_setStation("Radio", 0, 0);

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

  // Promote status from "Connexion..." to "En cours" once the audio library
  // has actually started decoding, or to an error after a reasonable timeout.
  static bool     s_wasPlaying = false;
  static uint32_t s_connectStart = 0;
  bool playing = audio_isPlaying();
  if (playing != s_wasPlaying) {
    s_wasPlaying = playing;
    if (playing) {
      display_setStatus("En cours");
      s_connectStart = 0;
    } else {
      // Stream stopped (EOF / disconnect). Note start of new connect attempt.
      s_connectStart = millis();
      display_setStatus(audio_currentUrl().length() ? "Connexion..." : "Arrete");
    }
  } else if (!playing && audio_currentUrl().length()) {
    if (s_connectStart == 0) s_connectStart = millis();
    if (millis() - s_connectStart > 10000) {
      display_setStatus("Erreur de connexion");
      s_connectStart = 0;
    }
  }

  // Wi-Fi status update (cheap; once a second)
  static uint32_t s_lastWifi = 0;
  if (millis() - s_lastWifi > 1000) {
    s_lastWifi = millis();
    display_setWifi(!webui_isApMode(), webui_currentSsidOrIp());
  }

  display_loop();
  webui_loop();
}
