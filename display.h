#pragma once
#include <Arduino.h>

void   display_begin();
void   display_loop();                 // periodic redraws (scrolling title etc.)

void   display_setStation(const String& name, int index, int total);
void   display_setStreamTitle(const String& title);
void   display_setVolume(uint8_t v, uint8_t maxV, bool muted);
void   display_setStatus(const String& s);     // "Connexion...", "En cours", etc.
void   display_setWifi(bool connected, const String& ssidOrIp);

void   display_setBacklight(uint8_t pct);     // 0..100
uint8_t display_getBacklight();

void   display_forceRedraw();
