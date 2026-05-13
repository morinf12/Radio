#pragma once
#include <Arduino.h>

// Input event codes. Names are kept for backward compatibility with the
// previous rotary-encoder build; the underlying source is now the same
// 6-button layout as the "horloge" project (Up/Down/Left/Right/A/B).
enum CtrlEvent {
  CTRL_NONE = 0,
  CTRL_ROT_CW,         // UP    -> volume up
  CTRL_ROT_CCW,        // DOWN  -> volume down
  CTRL_ENC_PRESS,      // A     -> play / pause (mute toggle)
  CTRL_ENC_LONG,       // B     -> reserved (menu)
  CTRL_PREV,           // LEFT  -> previous station
  CTRL_NEXT,           // RIGHT -> next station
};

void       controls_begin();
CtrlEvent  controls_poll();          // returns next event or CTRL_NONE
void       controls_inject(CtrlEvent ev); // queue a simulated event (web debug)
