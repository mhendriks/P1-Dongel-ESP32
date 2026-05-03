#include <Arduino.h>
#include "esp_timer.h"

static portMUX_TYPE waterMux = portMUX_INITIALIZER_UNLOCKED;

volatile uint32_t g_waterPulses = 0;
volatile uint32_t g_debounces   = 0;
volatile uint32_t g_lastPulseUs = 0;

static constexpr uint32_t WATER_MIN_PULSE_US = 2000000UL;

void IRAM_ATTR iWater() {
  uint32_t nowUs = (uint32_t)esp_timer_get_time();

  portENTER_CRITICAL_ISR(&waterMux);
  uint32_t dt = nowUs - g_lastPulseUs;

  if (dt >= WATER_MIN_PULSE_US) {
    g_lastPulseUs = nowUs;
    g_waterPulses++;
  } else {
    g_debounces++;
  }
  portEXIT_CRITICAL_ISR(&waterMux);
}

static inline uint32_t waterTakePulses() {
  static uint32_t last = 0;
  uint32_t cur;

  portENTER_CRITICAL(&waterMux);
  cur = g_waterPulses;
  portEXIT_CRITICAL(&waterMux);

  uint32_t diff = cur - last;
  last = cur;
  return diff;
}

static inline uint32_t waterGetDebounces() {
  uint32_t d;
  portENTER_CRITICAL(&waterMux);
  d = g_debounces;
  portEXIT_CRITICAL(&waterMux);
  return d;
}

static inline float waterCurrentM3() {
  return (float)P1Status.wtr_m3 + ((float)P1Status.wtr_l / 1000.0f);
}

static void waterResetCounters() {
  portENTER_CRITICAL(&waterMux);
  g_waterPulses = 0;
  g_debounces   = 0;
  g_lastPulseUs = (uint32_t)esp_timer_get_time();
  portEXIT_CRITICAL(&waterMux);
}

void handleWater() {
  if (!WtrMtr) return;

  uint32_t newPulses = waterTakePulses();
  if (newPulses == 0) return;

  // Preserve the existing interval semantics: seconds since the previous accepted reading.
  WtrTimeBetween = (uint32_t)(now() - WtrPrevReading);

  P1Status.wtr_l += (int)(newPulses * (float)WtrFactor);

  while (P1Status.wtr_l >= 1000) {
    P1Status.wtr_m3++;
    P1Status.wtr_l -= 1000;
    CHANGE_INTERVAL_MS(StatusTimer, 100);
  }

  WtrPrevReading = now();
  waterDeliveredTimestamp = actTimestamp;
  waterDelivered = waterCurrentM3();

  uint32_t deb = waterGetDebounces();
  DebugTf("Wtr delta readings: %lu | debounces: %lu | waterstand: %im3 en %i liters\n",
          (unsigned long)WtrTimeBetween,
          (unsigned long)deb,
          P1Status.wtr_m3,
          P1Status.wtr_l);

  WtrTimeBetween = 0;
}

void MQTTsendWater() {
  if (!WtrMtr && !mbusWater) return;

  if (mbusWater) {
    MQTTSend("water", waterDelivered);
    MQTTSend("water_ts", waterDeliveredTimestamp, true);
  } else {
    sprintf(cMsg, "%d.%3.3d", P1Status.wtr_m3, P1Status.wtr_l);
    MQTTSend("water", cMsg, true);
  }
}

void setupWater() {
  if (IOWater == -1) {
    DebugTln(F("Water sensor : N/A"));
    return;
  }

  pinMode(IOWater, INPUT_PULLUP);

  // Avoid a large first interval before the first accepted pulse.
  WtrPrevReading = now();
  WtrTimeBetween = 0;
  waterResetCounters();

  attachInterrupt(digitalPinToInterrupt(IOWater), iWater, RISING);
  DebugTln(F("WaterSensor setup completed"));
}
