static const uint32_t VICTRON_ACCU_STALE_MS = 20000;
static BatteryEnergyResource g_batteryEnergyResource;

static uint32_t batteryStaleAfterMs() {
  if (batteryConnectorDriver == BATTERY_DRIVER_SOLAREDGE_HTTP) {
    return (uint32_t)constrain((int)solarEdgeBatteryConfig.pollIntervalSeconds * 2, 60, 3600) * 1000UL;
  }
  return VICTRON_ACCU_STALE_MS;
}

static void setBatteryMeasurement(EnergyMeasurement& measurement, float value,
                                  uint16_t sourceRegister, uint32_t timestampMs) {
  measurement.value = value;
  measurement.sourceRegister = sourceRegister;
  measurement.timestampMs = timestampMs;
  measurement.available = true;
}

static void invalidateBatteryMeasurements() {
  g_batteryEnergyResource.stateOfCharge.available = false;
  g_batteryEnergyResource.activePower.available = false;
  g_batteryEnergyResource.availableCapacity.available = false;
  g_batteryEnergyResource.chargeLimit.available = false;
  g_batteryEnergyResource.dischargeLimit.available = false;
  g_batteryEnergyResource.operatingState = BatteryOperatingState::UNKNOWN;
}

float SolarEdgeFlowPvPower = 0.0f;
bool  SolarEdgeFlowPvValid = false;

bool batteryDashboardData(float& powerKw, uint8_t& stateOfCharge, BatteryOperatingState& operatingState) {
  const BatteryEnergyResource& battery = batteryEnergyResource();
  const uint32_t nowMs = millis();
  if (batteryMeasurementQuality(battery.activePower, nowMs, batteryStaleAfterMs()) != EnergyMeasurementQuality::GOOD ||
      batteryMeasurementQuality(battery.stateOfCharge, nowMs, batteryStaleAfterMs()) != EnergyMeasurementQuality::GOOD) return false;
  powerKw = battery.activePower.value / 1000.0f;
  stateOfCharge = (uint8_t)constrain((int)battery.stateOfCharge.value, 0, 100);
  operatingState = battery.operatingState;
  return true;
}

static void updateBatteryResource(const BatteryEnergyUpdate& update, const char* connectorId,
                                  const char* sourceProtocol) {
  // All drivers write the one canonical resource. Dashboard/API/ESP-NOW use
  // this resource directly; no driver-specific battery cache remains.
  g_batteryEnergyResource.profileId = update.profileId;
  g_batteryEnergyResource.connectorId = connectorId;
  g_batteryEnergyResource.sourceProtocol = sourceProtocol;
  g_batteryEnergyResource.sourceUnitId = update.sourceUnitId;
  g_batteryEnergyResource.lastSuccessfulPollMs = update.timestampMs;
  if (update.hasOperatingState) {
    g_batteryEnergyResource.nativeOperatingState = update.nativeOperatingState;
    g_batteryEnergyResource.operatingState = update.operatingState;
  }
  if (update.hasActivePower) setBatteryMeasurement(g_batteryEnergyResource.activePower, update.activePowerW,
                        update.activePowerRegister, update.activePowerTimestampMs);
  if (update.hasStateOfCharge) setBatteryMeasurement(g_batteryEnergyResource.stateOfCharge, update.stateOfChargePercent,
                        update.stateOfChargeRegister, update.stateOfChargeTimestampMs);
  if (update.hasAvailableCapacity) setBatteryMeasurement(g_batteryEnergyResource.availableCapacity, update.availableCapacityWh,
                        update.availableCapacityRegister, update.availableCapacityTimestampMs);
  if (update.hasChargeLimit) setBatteryMeasurement(g_batteryEnergyResource.chargeLimit, update.chargeLimitW,
                        update.chargeLimitRegister, update.chargeLimitTimestampMs);
  if (update.hasDischargeLimit) setBatteryMeasurement(g_batteryEnergyResource.dischargeLimit, update.dischargeLimitW,
                        update.dischargeLimitRegister, update.dischargeLimitTimestampMs);
  apiWsMarkLiveDirty();
}

void updateModbusBattery(const BatteryEnergyUpdate& update) {
  updateBatteryResource(update, "modbus-tcp", "modbus-tcp");
}

void updateSolarEdgeBattery(float power, const char* powerUnit, uint8_t stateOfCharge,
                            const char* status, uint32_t timestampMs) {
  BatteryEnergyUpdate update = {};
  update.profileId = "solaredge-monitoring-api-v1";
  update.sourceUnitId = 0;
  update.timestampMs = timestampMs;
  update.activePowerW = String(powerUnit).equalsIgnoreCase("kW") ? power * 1000.0f : power;
  String stateText(status ? status : "");
  stateText.toLowerCase();
  update.operatingState = stateText.indexOf("discharg") >= 0 ? BatteryOperatingState::DISCHARGING
      : stateText.indexOf("charg") >= 0 ? BatteryOperatingState::CHARGING : BatteryOperatingState::IDLE;
  if (update.operatingState == BatteryOperatingState::DISCHARGING && update.activePowerW > 0) update.activePowerW = -update.activePowerW;
  if (update.operatingState == BatteryOperatingState::CHARGING && update.activePowerW < 0) update.activePowerW = -update.activePowerW;
  update.activePowerTimestampMs = timestampMs;
  update.stateOfChargePercent = stateOfCharge;
  update.stateOfChargeTimestampMs = timestampMs;
  update.operatingStateTimestampMs = timestampMs;
  update.hasActivePower = true;
  update.hasStateOfCharge = true;
  update.hasOperatingState = true;
  updateBatteryResource(update, "solaredge-http", "http-json");
}

void invalidateBatteryResource() {
  bool changed = g_batteryEnergyResource.activePower.available || g_batteryEnergyResource.stateOfCharge.available;
  invalidateBatteryMeasurements();
  if (changed) apiWsMarkLiveDirty();
}

const BatteryEnergyResource& batteryEnergyResource() {
  return g_batteryEnergyResource;
}

static void addBatteryMeasurementJson(JsonObject measurements, const char* name,
                                      const EnergyMeasurement& measurement, const char* protocol,
                                      uint32_t nowMs) {
  JsonObject output = measurements[name].to<JsonObject>();
  EnergyMeasurementQuality quality = batteryMeasurementQuality(measurement, nowMs, batteryStaleAfterMs());
  output["quality"] = energyMeasurementQualityText(quality);
  output["unit"] = measurement.unit;
  output["timestamp_ms"] = measurement.available ? measurement.timestampMs : 0;
  JsonObject source = output["source"].to<JsonObject>();
  source["protocol"] = protocol;
  if (!strcmp(protocol, "modbus-tcp")) source["register_type"] = "holding";
  if (measurement.sourceRegister) source["register"] = measurement.sourceRegister;
  else source["register"] = nullptr;
  if (measurement.available) {
    output["value"] = measurement.value;
  }
}

ApiResponse batteryEnergyApiResponse() {
  const BatteryEnergyResource& battery = batteryEnergyResource();
  const uint32_t nowMs = millis();
  JsonDocument doc;
  doc["id"] = battery.resourceId;
  doc["type"] = "battery";
  doc["connector"] = battery.connectorId;
  doc["profile"] = battery.profileId;
  doc["unit_id"] = battery.sourceUnitId;
  doc["last_successful_poll_ms"] = battery.lastSuccessfulPollMs;

  JsonObject state = doc["operating_state"].to<JsonObject>();
  state["value"] = batteryOperatingStateText(battery.operatingState);
  state["native_code"] = battery.nativeOperatingState;
  state["quality"] = energyMeasurementQualityText(
      batteryMeasurementQuality(battery.activePower, nowMs, batteryStaleAfterMs()));

  JsonObject measurements = doc["measurements"].to<JsonObject>();
  addBatteryMeasurementJson(measurements, "state_of_charge", battery.stateOfCharge, battery.sourceProtocol, nowMs);
  addBatteryMeasurementJson(measurements, "active_power", battery.activePower, battery.sourceProtocol, nowMs);
  addBatteryMeasurementJson(measurements, "available_capacity", battery.availableCapacity, battery.sourceProtocol, nowMs);
  addBatteryMeasurementJson(measurements, "charge_limit", battery.chargeLimit, battery.sourceProtocol, nowMs);
  addBatteryMeasurementJson(measurements, "discharge_limit", battery.dischargeLimit, battery.sourceProtocol, nowMs);

  String body;
  serializeJson(doc, body);
  return {200, "application/json", body};
}

ApiResponse accuApiResponse() {
  float powerKw;
  uint8_t stateOfCharge;
  BatteryOperatingState operatingState;
  if (!batteryDashboardData(powerKw, stateOfCharge, operatingState)) {
    return {200, "application/json", "{\"active\":false}"};
  }

  JsonDocument doc;
  doc["active"] = true;
  doc["status"] = operatingState == BatteryOperatingState::CHARGING ? "Charging" :
                  operatingState == BatteryOperatingState::DISCHARGING ? "Discharging" :
                  operatingState == BatteryOperatingState::UNKNOWN ? "Unknown" : "Idle";
  doc["unit"] = "kW";
  doc["currentPower"] = powerKw;
  doc["chargeLevel"] = stateOfCharge;

  String body;
  serializeJson(doc, body);

#ifdef DEBUG
  DebugTln("SendAccuJson");
  DebugT("Accu Json: ");Debugln(body);
#endif

  return {200, "application/json", body};
}

bool fillDashAccuJson(JsonDocument& doc) {
  float powerKw;
  uint8_t stateOfCharge;
  BatteryOperatingState operatingState;
  if (!batteryDashboardData(powerKw, stateOfCharge, operatingState)) return false;

  JsonObject accu = doc["accu"].to<JsonObject>();
  accu["active"] = true;
  accu["status"] = operatingState == BatteryOperatingState::CHARGING ? "Charging" :
                   operatingState == BatteryOperatingState::DISCHARGING ? "Discharging" :
                   operatingState == BatteryOperatingState::UNKNOWN ? "Unknown" : "Idle";
  accu["currentPower"] = powerKw;
  accu["chargeLevel"] = stateOfCharge;

  return true;
}
