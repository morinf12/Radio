#pragma once
#include <Arduino.h>

enum CtrlEvent {
  CTRL_NONE = 0,
  CTRL_ROT_CW,         // encoder turn clockwise        -> volume up
  CTRL_ROT_CCW,        // encoder turn counter-clockwise -> volume down
  CTRL_ENC_PRESS,      // encoder short press            -> play/pause (mute)
  CTRL_ENC_LONG,       // encoder long press (>= 800 ms) -> reserved (menu)
  CTRL_PREV,           // previous station
  CTRL_NEXT,           // next station
};

void       controls_begin();
CtrlEvent  controls_poll();          // returns next event or CTRL_NONE
