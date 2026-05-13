#pragma once
#include <Arduino.h>

void   audio_begin();
void   audio_loop();                  // pump the decoder; call from loop()

bool   audio_play(const String& url); // start streaming a URL
void   audio_stop();
bool   audio_isPlaying();

void   audio_setVolume(uint8_t v);    // 0..21
uint8_t audio_getVolume();
void   audio_volumeUp();
void   audio_volumeDown();

void   audio_setMute(bool m);
bool   audio_isMuted();

// Latest stream metadata (ICY title), if any.
const String& audio_streamTitle();
const String& audio_currentUrl();
