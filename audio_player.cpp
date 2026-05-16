#include "audio_player.h"
#include "config.h"
#include <Audio.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// Single global Audio instance from ESP32-audioI2S (schreibfaul1).
static Audio   s_audio;
static uint8_t s_volume   = DEFAULT_VOLUME;
static uint8_t s_savedVol = DEFAULT_VOLUME;
static bool    s_muted    = false;
static String  s_title    = "";
static String  s_url      = "";

// Audio runs in its own high-priority FreeRTOS task so that display SPI
// blits and webui HTTP handling on the main loop() (single core on S2)
// cannot starve the I2S DMA feeder. All public mutators below take the
// mutex before touching the Audio object.
static SemaphoreHandle_t s_audioMux = nullptr;
static TaskHandle_t      s_audioTask = nullptr;

struct AudioLock {
  AudioLock()  { if (s_audioMux) xSemaphoreTake(s_audioMux, portMAX_DELAY); }
  ~AudioLock() { if (s_audioMux) xSemaphoreGive(s_audioMux); }
};

static void audioTaskFn(void*) {
  for (;;) {
    {
      AudioLock lk;
      s_audio.loop();
    }
    // Yield briefly so lower-priority tasks (main loop, WiFi) can run.
    vTaskDelay(1);
  }
}

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
  // 64 KB ring buffer in INTERNAL RAM (this board has no PSRAM). ~4 s at
  // 128 kbps - enough to absorb Wi-Fi jitter while leaving plenty of RAM
  // for decoders, WiFi/lwIP and the rest of the app.
  s_audio.setBufsize(64 * 1024, 0);

  s_audioMux = xSemaphoreCreateMutex();
  // Priority 2 is higher than the Arduino loopTask (priority 1) so the
  // audio decoder preempts display/webui work whenever it has data ready.
  // 8 KB stack is enough for ESP32-audioI2S's decoder paths.
  xTaskCreate(audioTaskFn, "audio", 8192, nullptr, 2, &s_audioTask);
}

void audio_loop() {
  // Audio now runs on its own task; nothing to do here.
}

bool audio_play(const String& url) {
  AudioLock lk;
  s_url   = url;
  s_title = "";
  if (url.length() == 0) { s_audio.stopSong(); return false; }
  return s_audio.connecttohost(url.c_str());
}

void audio_stop() {
  AudioLock lk;
  s_audio.stopSong();
  s_url = "";
  s_title = "";
}

bool audio_isPlaying() {
  AudioLock lk;
  return s_audio.isRunning();
}

void audio_setVolume(uint8_t v) {
  if (v > MAX_VOLUME) v = MAX_VOLUME;
  {
    AudioLock lk;
    s_volume = v;
    if (!s_muted) s_audio.setVolume(v);
  }
  Preferences p;
  if (p.begin("radio", false)) { p.putUChar("vol", v); p.end(); }
}

uint8_t audio_getVolume() { return s_volume; }
void    audio_volumeUp()   { if (s_volume < MAX_VOLUME) audio_setVolume(s_volume + 1); }
void    audio_volumeDown() { if (s_volume > 0)          audio_setVolume(s_volume - 1); }

void audio_setMute(bool m) {
  if (m == s_muted) return;
  AudioLock lk;
  s_muted = m;
  if (m) { s_savedVol = s_volume; s_audio.setVolume(0); }
  else   { s_audio.setVolume(s_savedVol); }
}

bool          audio_isMuted()     { return s_muted; }
const String& audio_streamTitle() { return s_title; }
const String& audio_currentUrl()  { return s_url; }

// ---- ESP32-audioI2S callbacks ----------------------------------------------
// These run on the audio task, inside s_audio.loop(). The mutex is already
// held there so we just mutate strings directly.
void audio_showstreamtitle(const char* info) { s_title = info ? info : ""; }
void audio_info(const char* /*info*/)        { /* noisy: keep silent */ }
void audio_eof_stream(const char* /*info*/)  { s_title = ""; }

