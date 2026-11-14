#pragma once
#include <stdint.h>
#include <math.h>
#include <string.h>

enum ConnectorValueType : uint8_t { CONNECTOR_U16, CONNECTOR_S16, CONNECTOR_U32, CONNECTOR_S32, CONNECTOR_F32 };
struct ConnectorField {
  uint16_t reg;
  uint8_t type;
  float scale;
  bool swap;
  uint16_t sf = 0; // Zero disables the optional SunSpec exponent register.
};
struct ConnectorConfig {
  bool enabled = false;
  char ip[16] = "";
  uint16_t port = 502;
  uint8_t id = 100;
  uint16_t interval = 5;
};
struct BatteryConnectorConfig : ConnectorConfig {
  ConnectorField power {842, CONNECTOR_S16, 1, false};
  ConnectorField soc {843, CONNECTOR_U16, 1, false};
  ConnectorField state {844, CONNECTOR_U16, 1, false};
  int16_t idle = 0, charging = 1, discharging = 2;
};
struct PvConnectorConfig : ConnectorConfig {
  PvConnectorConfig() { port = 1502; id = 1; }
  ConnectorField power {40083, CONNECTOR_S16, 1, false, 40084};
  ConnectorField energy {40093, CONNECTOR_U32, 1, false, 40095};
  uint32_t wp = 0;
};
inline uint8_t connectorWordCount(const ConnectorField& field) {
  return field.type >= CONNECTOR_U32 ? 2 : 1;
}
inline bool decodeConnectorValue(const ConnectorField& field, uint16_t first, uint16_t second, double& value) {
  uint32_t raw = field.swap ? (uint32_t(second) << 16) | first : (uint32_t(first) << 16) | second;
  switch (field.type) {
    case CONNECTOR_U16: value = first; break;
    case CONNECTOR_S16: value = int16_t(first); break;
    case CONNECTOR_U32: value = raw; break;
    case CONNECTOR_S32: value = int32_t(raw); break;
    case CONNECTOR_F32: { float f; memcpy(&f, &raw, sizeof(f)); value = f; break; }
    default: return false;
  }
  value *= field.scale;
  return isfinite(value);
}
inline bool applyConnectorExponent(double& value, uint16_t raw) {
  const int16_t exponent = int16_t(raw);
  if (exponent < -10 || exponent > 10) return false; // Includes SunSpec's unavailable 0x8000.
  value *= pow(10.0, exponent);
  return isfinite(value);
}

// Persist this one record atomically; keep the baseline separate from live power.
struct PvDailyBaseline {
  uint32_t day = 0, source = 0;
  double wattHours = 0;
  bool observe(double totalWh, uint32_t today, uint32_t sourceId) {
    if (day == today && source == sourceId && isfinite(wattHours) && totalWh >= wattHours) return false;
    day = today; source = sourceId; wattHours = totalWh;
    return true;
  }
  uint32_t yieldWh(double totalWh) const {
    return uint32_t(fmin(double(UINT32_MAX), fmax(0.0, totalWh - wattHours)));
  }
};
