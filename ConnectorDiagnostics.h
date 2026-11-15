#pragma once
#include <stdint.h>

// Volatile, bounded capture. Never persisted; explicitly enabled after each reboot.
struct ConnectorDiagnosticEntry {
  uint32_t atMs = 0;
  uint16_t reg = 0, raw[2] = {}, error = 0;
  uint8_t field = 0, count = 0;
  bool exponent = false, hasRaw = false, hasValue = false;
  double value = 0;
};
struct ConnectorDiagnostics {
  static constexpr uint8_t capacity = 12;
  bool enabled = false, hasSuccess = false, hasError = false, latestFailed = false;
  uint32_t startedMs = 0, lastSuccessMs = 0, lastErrorMs = 0;
  uint32_t requests = 0, cycles = 0, failures = 0;
  uint16_t lastError = 0, lastErrorRegister = 0;
  uint8_t count = 0, head = 0;
  ConnectorDiagnosticEntry entries[capacity];

  void reset(bool capture, uint32_t now) {
    *this = ConnectorDiagnostics{};
    enabled = capture;
    if (capture) startedMs = now;
  }
  void record(const ConnectorDiagnosticEntry& entry) {
    if (!enabled) return;
    entries[head] = entry;
    head = (head + 1) % capacity;
    if (count < capacity) ++count;
    if (entry.error) {
      hasError = latestFailed = true;
      lastError = entry.error; lastErrorRegister = entry.reg; lastErrorMs = entry.atMs;
      ++failures;
    }
  }
  void accepted(uint32_t now) {
    if (!enabled) return;
    hasSuccess = true; latestFailed = false; lastSuccessMs = now; ++cycles;
  }
};
