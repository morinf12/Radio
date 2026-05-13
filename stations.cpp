#include "stations.h"
#include <Preferences.h>
#include <ArduinoJson.h>

static Station s_list[STATIONS_MAX];
static int     s_count = 0;
static int     s_cur   = 0;

// Default Montréal stations.
//   97.7  -> Planète (CKLX-FM Montréal) - jazz/lounge
//   98.5  -> 98,5 FM Montréal (CHMP-FM) - parlé / talk
// Stream URLs sourced from public iHeart / Cogeco radio listings.
static const Station DEFAULTS[] PROGMEM = {
  { "98,5 FM Montreal", "https://stream.iheart.com/cogeco-985fm" },
  { "Planete 97,7",     "https://stream.iheart.com/cogeco-cklx" },
};
static const int DEFAULT_COUNT = sizeof(DEFAULTS) / sizeof(DEFAULTS[0]);

static void seedDefaults() {
  s_count = 0;
  for (int i = 0; i < DEFAULT_COUNT && s_count < STATIONS_MAX; ++i) {
    s_list[s_count].name = DEFAULTS[i].name;
    s_list[s_count].url  = DEFAULTS[i].url;
    s_count++;
  }
}

static bool loadFromPrefs() {
  Preferences p;
  if (!p.begin("radio", true)) return false;
  String json = p.getString("stations", "");
  s_cur       = p.getInt("cur", 0);
  p.end();
  if (json.length() == 0) return false;

  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  if (!doc.is<JsonArray>()) return false;
  s_count = 0;
  for (JsonObject o : doc.as<JsonArray>()) {
    if (s_count >= STATIONS_MAX) break;
    s_list[s_count].name = String((const char*)(o["name"] | ""));
    s_list[s_count].url  = String((const char*)(o["url"]  | ""));
    if (s_list[s_count].url.length() > 0) s_count++;
  }
  return s_count > 0;
}

void stations_begin() {
  if (!loadFromPrefs()) seedDefaults();
  if (s_cur < 0 || s_cur >= s_count) s_cur = 0;
}

int stations_count() { return s_count; }

const Station& stations_get(int idx) {
  static Station empty;
  if (idx < 0 || idx >= s_count) return empty;
  return s_list[idx];
}

int stations_currentIndex() { return s_cur; }

void stations_setCurrentIndex(int idx) {
  if (idx < 0 || idx >= s_count) return;
  s_cur = idx;
  Preferences p;
  if (p.begin("radio", false)) { p.putInt("cur", s_cur); p.end(); }
}

void stations_save() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < s_count; ++i) {
    JsonObject o = arr.add<JsonObject>();
    o["name"] = s_list[i].name;
    o["url"]  = s_list[i].url;
  }
  String out;
  serializeJson(doc, out);
  Preferences p;
  if (p.begin("radio", false)) {
    p.putString("stations", out);
    p.putInt("cur", s_cur);
    p.end();
  }
}

void stations_setAll(const Station* list, int n) {
  if (n > STATIONS_MAX) n = STATIONS_MAX;
  s_count = 0;
  for (int i = 0; i < n; ++i) {
    if (list[i].url.length() == 0) continue;
    s_list[s_count++] = list[i];
  }
  if (s_cur >= s_count) s_cur = 0;
  stations_save();
}

void stations_resetDefaults() {
  seedDefaults();
  s_cur = 0;
  stations_save();
}
