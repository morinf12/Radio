#include "controls.h"
#include "config.h"

// Six-button input (Up/Down/Left/Right/A/B), same pinout as the "horloge"
// project. Pins are active-LOW with internal pull-ups.
//
// Mapping to the radio's CtrlEvent API:
//   UP    -> CTRL_ROT_CW    (volume up)
//   DOWN  -> CTRL_ROT_CCW   (volume down)
//   LEFT  -> CTRL_PREV
//   RIGHT -> CTRL_NEXT
//   A     -> CTRL_ENC_PRESS (play / pause)
//   B     -> CTRL_ENC_LONG  (reserved for future menu)
//
// Press behavior matches horloge/buttons.cpp: one event on initial press,
// then auto-repeat after 1 s, fired every 150 ms while held.

static const uint8_t   s_pins[]   = {
  BTN_UP_PIN, BTN_DOWN_PIN, BTN_LEFT_PIN,
  BTN_RIGHT_PIN, BTN_A_PIN, BTN_B_PIN
};
static const CtrlEvent s_events[] = {
  CTRL_ROT_CW, CTRL_ROT_CCW, CTRL_PREV,
  CTRL_NEXT,   CTRL_ENC_PRESS, CTRL_ENC_LONG
};
static const uint8_t NUM_BTNS = sizeof(s_pins) / sizeof(s_pins[0]);

static const uint32_t DEBOUNCE_MS     = 50;
static const uint32_t REPEAT_DELAY_MS = 1000;  // 1 s before repeat starts
static const uint32_t REPEAT_RATE_MS  = 150;   // repeat interval once started

static int8_t   s_heldIdx    = -1;
static uint32_t s_pressTime  = 0;
static bool     s_repeating  = false;
static uint32_t s_lastRepeat = 0;

// Simulated events injected from the web debug page. Tiny FIFO so several
// rapid clicks are not dropped between poll() calls.
static const uint8_t INJECT_CAP = 8;
static volatile CtrlEvent s_inject[INJECT_CAP];
static volatile uint8_t   s_injHead = 0;
static volatile uint8_t   s_injTail = 0;

void controls_inject(CtrlEvent ev) {
  if (ev == CTRL_NONE) return;
  uint8_t next = (uint8_t)((s_injTail + 1) % INJECT_CAP);
  if (next == s_injHead) return;        // queue full, drop
  s_inject[s_injTail] = ev;
  s_injTail = next;
}

static CtrlEvent injectPop() {
  if (s_injHead == s_injTail) return CTRL_NONE;
  CtrlEvent ev = s_inject[s_injHead];
  s_injHead = (uint8_t)((s_injHead + 1) % INJECT_CAP);
  return ev;
}

void controls_begin() {
  for (uint8_t i = 0; i < NUM_BTNS; i++) {
    pinMode(s_pins[i], INPUT_PULLUP);
  }
}

CtrlEvent controls_poll() {
  uint32_t now = millis();

  // Web-injected events take priority so the debug page feels responsive.
  CtrlEvent inj = injectPop();
  if (inj != CTRL_NONE) return inj;

  // Held button: handle auto-repeat or release.
  if (s_heldIdx >= 0) {
    if (digitalRead(s_pins[s_heldIdx]) == LOW) {
      if (!s_repeating) {
        if (now - s_pressTime >= REPEAT_DELAY_MS) {
          s_repeating  = true;
          s_lastRepeat = now;
          return s_events[s_heldIdx];
        }
      } else if (now - s_lastRepeat >= REPEAT_RATE_MS) {
        s_lastRepeat = now;
        return s_events[s_heldIdx];
      }
      return CTRL_NONE;
    }
    // Released
    s_heldIdx   = -1;
    s_repeating = false;
  }

  // Scan for a new press.
  for (uint8_t i = 0; i < NUM_BTNS; i++) {
    if (digitalRead(s_pins[i]) == LOW) {
      delay(DEBOUNCE_MS);
      if (digitalRead(s_pins[i]) == LOW) {
        s_heldIdx    = i;
        s_pressTime  = now;
        s_repeating  = false;
        return s_events[i];
      }
    }
  }
  return CTRL_NONE;
}
