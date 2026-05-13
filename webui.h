#pragma once
#include <Arduino.h>

void webui_begin();
void webui_loop();
bool webui_isApMode();
String webui_currentSsidOrIp();
