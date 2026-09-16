#pragma once

#include <stdint.h>

// Protocol-neutral, read-only battery resource.  A value is only meaningful
// when available is true; zero is deliberately kept as a valid measurement.
enum class EnergyMeasurementQuality : uint8_t { UNAVAILABLE, STALE, GOOD };
enum class BatteryOperatingState : uint8_t { UNKNOWN, IDLE, CHARGING, DISCHARGING };

struct EnergyMeasurement {
  float value;
  const char* unit;
  uint32_t timestampMs;
  uint16_t sourceRegister;
  bool available;

  EnergyMeasurement(float initialValue = 0.0f, const char* initialUnit = "",
                    uint32_t initialTimestampMs = 0, uint16_t initialSourceRegister = 0,
                    bool initialAvailable = false)
      : value(initialValue), unit(initialUnit), timestampMs(initialTimestampMs),
        sourceRegister(initialSourceRegister), available(initialAvailable) {}
};

struct BatteryEnergyResource {
  const char* resourceId;
  const char* connectorId;
  const char* profileId;
  uint8_t sourceUnitId;
  uint32_t lastSuccessfulPollMs;
  uint16_t nativeOperatingState;
  BatteryOperatingState operatingState;
  EnergyMeasurement stateOfCharge {0.0f, "%"};
  EnergyMeasurement activePower {0.0f, "W"};
  EnergyMeasurement availableCapacity {0.0f, "Wh"};
  EnergyMeasurement chargeLimit {0.0f, "W"};
  EnergyMeasurement dischargeLimit {0.0f, "W"};

  BatteryEnergyResource()
      : resourceId("battery-1"), connectorId("modbus-tcp"), profileId(""),
        sourceUnitId(0), lastSuccessfulPollMs(0), nativeOperatingState(0),
        operatingState(BatteryOperatingState::UNKNOWN) {}
};

struct BatteryEnergyUpdate {
  const char* profileId;
  uint8_t sourceUnitId;
  uint32_t timestampMs;
  float activePowerW;
  uint16_t activePowerRegister;
  uint32_t activePowerTimestampMs;
  bool hasActivePower;
  float stateOfChargePercent;
  uint16_t stateOfChargeRegister;
  uint32_t stateOfChargeTimestampMs;
  bool hasStateOfCharge;
  uint16_t nativeOperatingState;
  BatteryOperatingState operatingState;
  uint32_t operatingStateTimestampMs;
  bool hasOperatingState;
  float availableCapacityWh;
  uint16_t availableCapacityRegister;
  uint32_t availableCapacityTimestampMs;
  bool hasAvailableCapacity;
  float chargeLimitW;
  uint16_t chargeLimitRegister;
  uint32_t chargeLimitTimestampMs;
  bool hasChargeLimit;
  float dischargeLimitW;
  uint16_t dischargeLimitRegister;
  uint32_t dischargeLimitTimestampMs;
  bool hasDischargeLimit;
};

inline EnergyMeasurementQuality batteryMeasurementQuality(const EnergyMeasurement& measurement,
                                                           uint32_t nowMs,
                                                           uint32_t staleAfterMs) {
  if (!measurement.available) return EnergyMeasurementQuality::UNAVAILABLE;
  return (nowMs - measurement.timestampMs <= staleAfterMs)
      ? EnergyMeasurementQuality::GOOD
      : EnergyMeasurementQuality::STALE;
}

inline const char* energyMeasurementQualityText(EnergyMeasurementQuality quality) {
  switch (quality) {
    case EnergyMeasurementQuality::GOOD: return "good";
    case EnergyMeasurementQuality::STALE: return "stale";
    default: return "unavailable";
  }
}

inline const char* batteryOperatingStateText(BatteryOperatingState state) {
  switch (state) {
    case BatteryOperatingState::IDLE: return "idle";
    case BatteryOperatingState::CHARGING: return "charging";
    case BatteryOperatingState::DISCHARGING: return "discharging";
    default: return "unknown";
  }
}
