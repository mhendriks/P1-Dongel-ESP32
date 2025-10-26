#ifndef _BUTTON_H
#define _BUTTON_H

#include <Arduino.h>

void P1Reboot();
void FacReset();

static uint8_t BUTTON_PIN;

static volatile uint32_t pressStart = 0;
static volatile uint32_t pressDur   = 0;
static volatile uint32_t lastEdge   = 0;
static volatile bool     pressed    = false;
static volatile bool     eventReady = false;

static const uint32_t DEBOUNCE_MS = 30;
static const uint32_t LONG_MS     = 5000;

void IRAM_ATTR isrButton() {
  const uint32_t now = millis();
  if (now - lastEdge < DEBOUNCE_MS) return;   // debounce beide flanken
  lastEdge = now;

  const int level = digitalRead(BUTTON_PIN);
  if (level == LOW) {
    // FALLING: echte druk
    pressed    = true;
    pressStart = now;
  } else {
    // RISING: loslaten
    if (pressed) {
      pressDur   = now - pressStart;
      eventReady = true;        // signaal voor de main loop
      pressed    = false;
    }
  }
}

class Button {
public:
  void begin(uint8_t pin) {
    BUTTON_PIN = pin;
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    // Init state op basis van huidige pinstatus
    const int level = digitalRead(BUTTON_PIN);
    const uint32_t now = millis();
    lastEdge = now;
    if (level == LOW) {             // knop al ingedrukt bij start
      pressed    = true;
      pressStart = now;
    } else {
      pressed    = false;
      pressStart = 0;
    }
    pressDur   = 0;
    eventReady = false;

    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), isrButton, CHANGE);
    // DebugTln(F("BUTTON setup completed"));
  }

  void handler(){
    if (!eventReady) return;

    // Kopieer atomair waarden uit ISR
    noInterrupts();
    uint32_t dur = pressDur;
    eventReady = false;
    interrupts();

    // Debug("Button pressed: "); Debugln(dur);
    if (dur >= LONG_MS) {
      // DebugTln(F("Button LONG Press = Factory Reset"));
      FacReset();
    } else {
      // DebugTln(F("Button SHORT Press = Reboot"));
      P1Reboot();
    }
  }
} PushButton;

#endif
