#pragma once
#include <Arduino.h>

// Default Montréal stations. URLs are public radio streams; can be overridden
// at runtime via the Web UI (saved in NVS namespace "radio", key "stations").
struct Station {
  String name;
  String url;
};

// Maximum number of stations stored in NVS / managed by the UI.
#define STATIONS_MAX  16

void     stations_begin();                    // load from NVS or seed defaults
int      stations_count();
const Station& stations_get(int idx);
int      stations_currentIndex();
void     stations_setCurrentIndex(int idx);
void     stations_save();                     // persist list to NVS
void     stations_setAll(const Station* list, int n);
void     stations_resetDefaults();
