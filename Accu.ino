AccuPwrSystems SolarEdgeAccu = {
  false, "", "", 0.0, 0
};

AccuPwrSystems VictronAccu = {
  false, "kW", "", 0.0, 0
};

static uint32_t victronAccuLastUpdate = 0;
static const uint32_t VICTRON_ACCU_STALE_MS = 20000;
static BatteryEnergyResource g_batteryEnergyResource;

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

static bool victronAccuAvailable() {
  return VictronAccu.Available && (millis() - victronAccuLastUpdate <= VICTRON_ACCU_STALE_MS);
}

AccuPwrSystems* dashboardAccu() {
  if (victronAccuAvailable()) return &VictronAccu;
  if (SolarEdgeAccu.Available) return &SolarEdgeAccu;
  return nullptr;
}

void updateModbusBattery(const BatteryEnergyUpdate& update) {
  if (update.hasActivePower) VictronAccu.currentPower = update.activePowerW / 1000.0f;
  if (update.hasStateOfCharge) VictronAccu.chargeLevel = (uint8_t)constrain((int)update.stateOfChargePercent, 0, 100);
  switch (update.operatingState) {
    case BatteryOperatingState::CHARGING: VictronAccu.status = "Charging"; break;
    case BatteryOperatingState::DISCHARGING: VictronAccu.status = "Discharging"; break;
    default: VictronAccu.status = "Idle"; break;
  }
  VictronAccu.Available = true;
  victronAccuLastUpdate = update.timestampMs;

  // Keep the dashboard projection separate from the canonical battery
  // resource.  These registers are profile metadata, not connector logic.
  g_batteryEnergyResource.profileId = update.profileId;
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

void invalidateVictronAccu() {
  bool changed = VictronAccu.Available;
  VictronAccu.Available = false;
  invalidateBatteryMeasurements();
  if (changed) apiWsMarkLiveDirty();
}

const BatteryEnergyResource& batteryEnergyResource() {
  return g_batteryEnergyResource;
}

static void addBatteryMeasurementJson(JsonObject measurements, const char* name,
                                      const EnergyMeasurement& measurement,
                                      uint32_t nowMs) {
  JsonObject output = measurements[name].to<JsonObject>();
  EnergyMeasurementQuality quality = batteryMeasurementQuality(measurement, nowMs, VICTRON_ACCU_STALE_MS);
  output["quality"] = energyMeasurementQualityText(quality);
  output["unit"] = measurement.unit;
  output["timestamp_ms"] = measurement.available ? measurement.timestampMs : 0;
  JsonObject source = output["source"].to<JsonObject>();
  source["protocol"] = "modbus-tcp";
  source["register_type"] = "holding";
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
      batteryMeasurementQuality(battery.activePower, nowMs, VICTRON_ACCU_STALE_MS));

  JsonObject measurements = doc["measurements"].to<JsonObject>();
  addBatteryMeasurementJson(measurements, "state_of_charge", battery.stateOfCharge, nowMs);
  addBatteryMeasurementJson(measurements, "active_power", battery.activePower, nowMs);
  addBatteryMeasurementJson(measurements, "available_capacity", battery.availableCapacity, nowMs);
  addBatteryMeasurementJson(measurements, "charge_limit", battery.chargeLimit, nowMs);
  addBatteryMeasurementJson(measurements, "discharge_limit", battery.dischargeLimit, nowMs);

  String body;
  serializeJson(doc, body);
  return {200, "application/json", body};
}

ApiResponse accuApiResponse() {
  AccuPwrSystems* accu = dashboardAccu();
  if (!accu) {
    return {200, "application/json", "{\"active\":false}"};
  }

  JsonDocument doc;
  doc["active"] = true;
  doc["status"] = accu->status;
  doc["unit"] = accu->unit;
  doc["currentPower"] = accu->currentPower;
  doc["chargeLevel"] = accu->chargeLevel;

  String body;
  serializeJson(doc, body);

#ifdef DEBUG
  DebugTln("SendAccuJson");
  DebugT("Accu Json: ");Debugln(body);
#endif

  return {200, "application/json", body};
}

bool fillDashAccuJson(JsonDocument& doc) {
  AccuPwrSystems* source = dashboardAccu();
  if (!source) return false;

  JsonObject accu = doc["accu"].to<JsonObject>();
  accu["active"] = true;
  accu["status"] = source->status;
  accu["currentPower"] = source->currentPower;
  accu["chargeLevel"] = source->chargeLevel;

  return true;
}
