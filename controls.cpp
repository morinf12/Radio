#include "controls.h"
#include "config.h"

// Lightweight polled rotary encoder + button handling.
// Encoder is decoded on every change of the A line (half-step). Direction is
// taken from B at the time of A's falling edge.

static volatile int8_t s_rotDelta = 0;   // +1 per CW step, -1 per CCW step
static uint8_t s_lastA = HIGH;
static uint8_t s_lastB = HIGH;

static uint32_t s_encPressedAt = 0;
static bool     s_encWasDown   = false;
static bool     s_encLongFired = false;

static uint32_t s_prevDownAt   = 0;
static bool     s_prevWasDown  = false;
static uint32_t s_nextDownAt   = 0;
static bool     s_nextWasDown  = false;

#define DEBOUNCE_MS   25
#define LONG_MS      800

static bool readBtn(uint8_t pin) { return digitalRead(pin) == LOW; }

void controls_begin() {
  pinMode(ENC_A_PIN,    INPUT_PULLUP);
  pinMode(ENC_B_PIN,    INPUT_PULLUP);
  pinMode(ENC_SW_PIN,   INPUT_PULLUP);
  pinMode(BTN_PREV_PIN, INPUT_PULLUP);
  pinMode(BTN_NEXT_PIN, INPUT_PULLUP);
  s_lastA = digitalRead(ENC_A_PIN);
  s_lastB = digitalRead(ENC_B_PIN);
}

CtrlEvent controls_poll() {
  uint32_t now = millis();

  // ---- Rotary encoder ------------------------------------------------------
  uint8_t a = digitalRead(ENC_A_PIN);
  uint8_t b = digitalRead(ENC_B_PIN);
  if (a != s_lastA) {
    if (a == LOW) {                 // falling edge on A
      if (b != a) s_rotDelta++;     // CW
      else        s_rotDelta--;     // CCW
    }
    s_lastA = a;
    s_lastB = b;
  }
  if (s_rotDelta > 0) { s_rotDelta--; return CTRL_ROT_CW; }
  if (s_rotDelta < 0) { s_rotDelta++; return CTRL_ROT_CCW; }

  // ---- Encoder push button -------------------------------------------------
  bool encDown = readBtn(ENC_SW_PIN);
  if (encDown && !s_encWasDown) {
    s_encWasDown   = true;
    s_encPressedAt = now;
    s_encLongFired = false;
  } else if (encDown && s_encWasDown && !s_encLongFired
             && (now - s_encPressedAt) >= LONG_MS) {
    s_encLongFired = true;
    return CTRL_ENC_LONG;
  } else if (!encDown && s_encWasDown) {
    s_encWasDown = false;
    if ((now - s_encPressedAt) >= DEBOUNCE_MS && !s_encLongFired) {
      return CTRL_ENC_PRESS;
    }
  }

  // ---- Prev/Next buttons ---------------------------------------------------
  bool prevDown = readBtn(BTN_PREV_PIN);
  if (prevDown && !s_prevWasDown) { s_prevWasDown = true; s_prevDownAt = now; }
  else if (!prevDown && s_prevWasDown) {
    s_prevWasDown = false;
    if ((now - s_prevDownAt) >= DEBOUNCE_MS) return CTRL_PREV;
  }

  bool nextDown = readBtn(BTN_NEXT_PIN);
  if (nextDown && !s_nextWasDown) { s_nextWasDown = true; s_nextDownAt = now; }
  else if (!nextDown && s_nextWasDown) {
    s_nextWasDown = false;
    if ((now - s_nextDownAt) >= DEBOUNCE_MS) return CTRL_NEXT;
  }

  return CTRL_NONE;
}
