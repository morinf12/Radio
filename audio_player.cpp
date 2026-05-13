#include "audio_player.h"
#include "config.h"
#include <Audio.h>
#include <Preferences.h>

// Single global Audio instance from ESP32-audioI2S (schreibfaul1).
static Audio   s_audio;
static uint8_t s_volume   = DEFAULT_VOLUME;
static uint8_t s_savedVol = DEFAULT_VOLUME;
static bool    s_muted    = false;
static String  s_title    = "";
static String  s_url      = "";

void audio_begin() {
  Preferences p;
  if (p.begin("radio", true)) {
    s_volume = p.getUChar("vol", DEFAULT_VOLUME);
    p.end();
  }
  if (s_volume > MAX_VOLUME) s_volume = MAX_VOLUME;
  s_savedVol = s_volume;

  s_audio.setPinout(I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DOUT_PIN);
  s_audio.setVolume(s_volume);
  s_audio.setBufsize(-1, 64 * 1024);   // PSRAM ring buffer
}

void audio_loop() {
  s_audio.loop();
}

bool audio_play(const String& url) {
  s_url   = url;
  s_title = "";
  if (url.length() == 0) { s_audio.stopSong(); return false; }
  return s_audio.connecttohost(url.c_str());
}

void audio_stop() {
  s_audio.stopSong();
  s_url = "";
  s_title = "";
}

bool audio_isPlaying() { return s_audio.isRunning(); }

void audio_setVolume(uint8_t v) {
  if (v > MAX_VOLUME) v = MAX_VOLUME;
  s_volume = v;
  if (!s_muted) s_audio.setVolume(v);
  Preferences p;
  if (p.begin("radio", false)) { p.putUChar("vol", v); p.end(); }
}

uint8_t audio_getVolume() { return s_volume; }
void    audio_volumeUp()   { if (s_volume < MAX_VOLUME) audio_setVolume(s_volume + 1); }
void    audio_volumeDown() { if (s_volume > 0)          audio_setVolume(s_volume - 1); }

void audio_setMute(bool m) {
  if (m == s_muted) return;
  s_muted = m;
  if (m) { s_savedVol = s_volume; s_audio.setVolume(0); }
  else   { s_audio.setVolume(s_savedVol); }
}

bool          audio_isMuted()     { return s_muted; }
const String& audio_streamTitle() { return s_title; }
const String& audio_currentUrl()  { return s_url; }

// ---- ESP32-audioI2S callbacks ----------------------------------------------
void audio_showstreamtitle(const char* info) { s_title = info ? info : ""; }
void audio_info(const char* /*info*/)        { /* noisy: keep silent */ }
void audio_eof_stream(const char* /*info*/)  { s_title = ""; }
