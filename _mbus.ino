#ifdef MBUS

#include "ModbusBatteryDecoding.h"

#define MBUS_DEV_ID       1
#define MBUS_CLIENTS      4
#define MBUS_TIMEOUT  10000
#define MBUS_VAL_UNAVAILABLE 0xFFFFFFFF

enum class ModbusDataType : uint8_t;
enum class MbSource : uint8_t;
struct ActiveRecipe;

float calculateLineVoltage(float V1, float V2) {
  return sqrt(3) * (V1 + V2) / 2.0;
}

// Set up a Modbus server
ModbusServerWiFi MBserver;

// Configurable Modbus-TCP battery connector. The energy model is fixed; only
// this compact driver configuration changes per physical battery system.
static WiFiClient victronModbusSocket;
static ModbusClientTCP victronModbusClient(victronModbusSocket, 2);
static bool victronModbusClientStarted = false;
static bool victronModbusTargetValid = false;
static volatile bool victronModbusRequestPending = false;
static volatile uint32_t victronModbusActiveToken = 0;
static uint32_t victronModbusToken = 0;
static uint32_t victronModbusLastPoll = 0;

static portMUX_TYPE victronModbusDataMux = portMUX_INITIALIZER_UNLOCKED;
static ModbusBatteryPollField victronModbusPendingField = ModbusBatteryPollField::ACTIVE_POWER;
static ModbusBatteryPollField victronModbusNextField = ModbusBatteryPollField::ACTIVE_POWER;
static bool victronModbusHavePower = false;
static bool victronModbusHaveSoc = false;
static bool victronModbusHaveState = false;
static BatteryEnergyUpdate victronModbusSample = {};

static const ModbusBatteryFieldConfig& modbusBatteryFieldConfig(ModbusBatteryPollField field) {
  switch (field) {
    case ModbusBatteryPollField::ACTIVE_POWER: return modbusBatteryConfig.activePower;
    case ModbusBatteryPollField::STATE_OF_CHARGE: return modbusBatteryConfig.stateOfCharge;
    case ModbusBatteryPollField::OPERATING_STATE: return modbusBatteryConfig.operatingState;
    case ModbusBatteryPollField::AVAILABLE_CAPACITY: return modbusBatteryConfig.availableCapacity;
    case ModbusBatteryPollField::CHARGE_LIMIT: return modbusBatteryConfig.chargeLimit;
    default: return modbusBatteryConfig.dischargeLimit;
  }
}

static uint8_t modbusBatteryWordCount(uint8_t valueType) {
  return valueType == MODBUS_BATTERY_U32 || valueType == MODBUS_BATTERY_S32 ||
         valueType == MODBUS_BATTERY_F32 ? 2 : 1;
}

static bool decodeModbusBatteryValue(const ModbusBatteryFieldConfig& field,
                                     uint16_t firstWord, uint16_t secondWord,
                                     float& value, int32_t& nativeValue) {
  uint32_t raw32 = decodeModbusU32(firstWord, secondWord, field.wordSwap);
  switch (field.valueType) {
    case MODBUS_BATTERY_U16: nativeValue = firstWord; value = scaleModbusValue(firstWord, field.scale); return true;
    case MODBUS_BATTERY_S16: nativeValue = decodeModbusS16(firstWord); value = scaleModbusValue(nativeValue, field.scale); return true;
    case MODBUS_BATTERY_U32: nativeValue = raw32 > INT32_MAX ? INT32_MAX : (int32_t)raw32; value = (float)raw32 * field.scale; return true;
    case MODBUS_BATTERY_S32: nativeValue = (int32_t)raw32; value = scaleModbusValue(nativeValue, field.scale); return true;
    case MODBUS_BATTERY_F32: {
      float rawFloat;
      memcpy(&rawFloat, &raw32, sizeof(rawFloat));
      if (!isfinite(rawFloat)) return false;
      nativeValue = (int32_t)rawFloat;
      value = rawFloat * field.scale;
      return true;
    }
  }
  return false;
}

static void handleVictronModbusData(ModbusMessage response, uint32_t token) {
  if (token != victronModbusActiveToken) return;
  victronModbusRequestPending = false;

  const ModbusBatteryFieldConfig& field = modbusBatteryFieldConfig(victronModbusPendingField);
  const uint8_t words = modbusBatteryWordCount(field.valueType);
  if (response.getServerID() != modbusBatteryConfig.id ||
      response.getFunctionCode() != READ_HOLD_REGISTER ||
      response.size() < (size_t)(3 + words * 2) || response[2] != words * 2) {
    DebugVerboseTln(F("Modbus battery: invalid response"));
    return;
  }
  uint16_t firstWord = 0, secondWord = 0;
  if (words == 1) response.get(3, firstWord);
  else response.get(3, firstWord, secondWord);
  float value;
  int32_t nativeValue;
  if (!decodeModbusBatteryValue(field, firstWord, secondWord, value, nativeValue)) return;
  portENTER_CRITICAL(&victronModbusDataMux);
  victronModbusSample.profileId = "configured-modbus-tcp-battery";
  victronModbusSample.sourceUnitId = modbusBatteryConfig.id;
  const uint32_t timestampMs = millis();
  victronModbusSample.timestampMs = timestampMs;
  switch (victronModbusPendingField) {
    case ModbusBatteryPollField::ACTIVE_POWER:
      victronModbusSample.activePowerW = value;
      victronModbusSample.activePowerRegister = field.registerAddress;
      victronModbusSample.activePowerTimestampMs = timestampMs;
      victronModbusSample.hasActivePower = true;
      victronModbusHavePower = true;
      break;
    case ModbusBatteryPollField::STATE_OF_CHARGE:
      victronModbusSample.stateOfChargePercent = constrain(value, 0.0f, 100.0f);
      victronModbusSample.stateOfChargeRegister = field.registerAddress;
      victronModbusSample.stateOfChargeTimestampMs = timestampMs;
      victronModbusSample.hasStateOfCharge = true;
      victronModbusHaveSoc = true;
      break;
    case ModbusBatteryPollField::OPERATING_STATE:
      victronModbusSample.nativeOperatingState = (uint16_t)nativeValue;
      victronModbusSample.operatingState = nativeValue == modbusBatteryConfig.idleStateCode ? BatteryOperatingState::IDLE
          : nativeValue == modbusBatteryConfig.chargingStateCode ? BatteryOperatingState::CHARGING
          : nativeValue == modbusBatteryConfig.dischargingStateCode ? BatteryOperatingState::DISCHARGING
          : BatteryOperatingState::UNKNOWN;
      victronModbusSample.operatingStateTimestampMs = timestampMs;
      victronModbusSample.hasOperatingState = true;
      victronModbusHaveState = true;
      break;
    case ModbusBatteryPollField::AVAILABLE_CAPACITY:
      victronModbusSample.availableCapacityWh = value;
      victronModbusSample.availableCapacityRegister = field.registerAddress;
      victronModbusSample.availableCapacityTimestampMs = timestampMs;
      victronModbusSample.hasAvailableCapacity = true;
      break;
    case ModbusBatteryPollField::CHARGE_LIMIT:
      victronModbusSample.chargeLimitW = value;
      victronModbusSample.chargeLimitRegister = field.registerAddress;
      victronModbusSample.chargeLimitTimestampMs = timestampMs;
      victronModbusSample.hasChargeLimit = true;
      break;
    case ModbusBatteryPollField::DISCHARGE_LIMIT:
      victronModbusSample.dischargeLimitW = value;
      victronModbusSample.dischargeLimitRegister = field.registerAddress;
      victronModbusSample.dischargeLimitTimestampMs = timestampMs;
      victronModbusSample.hasDischargeLimit = true;
      break;
    default: break;
  }
  portEXIT_CRITICAL(&victronModbusDataMux);
}

static void handleVictronModbusError(Error error, uint32_t token) {
  if (token != victronModbusActiveToken) return;
  victronModbusRequestPending = false;
  DebugVerboseTf("Victron Modbus error: 0x%02X\r\n", (uint8_t)error);
}

void modbusBatteryConfigChanged() {
  victronModbusActiveToken = 0;
  victronModbusRequestPending = false;
  portENTER_CRITICAL(&victronModbusDataMux);
  victronModbusHavePower = false;
  victronModbusHaveSoc = false;
  victronModbusHaveState = false;
  victronModbusSample = {};
  victronModbusNextField = ModbusBatteryPollField::ACTIVE_POWER;
  portEXIT_CRITICAL(&victronModbusDataMux);
  victronModbusTargetValid = false;
  victronModbusSocket.stop();

  IPAddress target;
  if (batteryConnectorDriver != BATTERY_DRIVER_MODBUS_TCP || !modbusBatteryConfig.enabled || !modbusBatteryConfig.activePower.registerAddress ||
      !modbusBatteryConfig.stateOfCharge.registerAddress || !modbusBatteryConfig.operatingState.registerAddress ||
      !target.fromString(modbusBatteryConfig.ip)) {
    invalidateBatteryResource();
    return;
  }

  if (!victronModbusClientStarted) {
    victronModbusClient.onDataHandler(&handleVictronModbusData);
    victronModbusClient.onErrorHandler(&handleVictronModbusError);
    victronModbusClient.setTimeout(1500, 200);
    victronModbusClient.begin();
    victronModbusClientStarted = true;
  }

  victronModbusClient.setTarget(target, modbusBatteryConfig.port);
  victronModbusTargetValid = true;
  victronModbusLastPoll = 0;
  invalidateBatteryResource();
  DebugVerboseTf("Modbus battery target: %s:%u id=%u\r\n",
                 modbusBatteryConfig.ip, modbusBatteryConfig.port, modbusBatteryConfig.id);
}

void setupModbusBattery() {
  modbusBatteryConfigChanged();
}

void handleModbusBattery() {
  if (victronModbusHavePower && victronModbusHaveSoc && victronModbusHaveState) {
    BatteryEnergyUpdate update;
    portENTER_CRITICAL(&victronModbusDataMux);
    update = victronModbusSample;
    victronModbusHavePower = false;
    victronModbusHaveSoc = false;
    victronModbusHaveState = false;
    portEXIT_CRITICAL(&victronModbusDataMux);

    if (batteryConnectorDriver == BATTERY_DRIVER_MODBUS_TCP && modbusBatteryConfig.enabled) updateModbusBattery(update);
  }

  if (batteryConnectorDriver != BATTERY_DRIVER_MODBUS_TCP || !modbusBatteryConfig.enabled || !victronModbusTargetValid ||
      (netw_state != NW_ETH && netw_state != NW_WIFI) ||
      victronModbusRequestPending) return;

  uint32_t nowMs = millis();
  uint32_t fieldInterval = (uint32_t)constrain((int)modbusBatteryConfig.pollIntervalSeconds, 1, 3600) * 1000UL / 6;
  if (fieldInterval < 250) fieldInterval = 250;
  if (victronModbusLastPoll && nowMs - victronModbusLastPoll < fieldInterval) return;
  victronModbusLastPoll = nowMs;

  uint32_t token = ++victronModbusToken;
  victronModbusActiveToken = token;
  victronModbusRequestPending = true;
  // Register 0 means this optional energy-model value is not mapped.  Skip it
  // without creating traffic or an artificial zero measurement.
  for (uint8_t attempts = 0; attempts < (uint8_t)ModbusBatteryPollField::COUNT; attempts++) {
    if (modbusBatteryFieldConfig(victronModbusNextField).registerAddress) break;
    victronModbusNextField = (ModbusBatteryPollField)(((uint8_t)victronModbusNextField + 1) % (uint8_t)ModbusBatteryPollField::COUNT);
  }
  const ModbusBatteryFieldConfig& field = modbusBatteryFieldConfig(victronModbusNextField);
  if (!field.registerAddress) { victronModbusRequestPending = false; return; }
  const uint8_t words = modbusBatteryWordCount(field.valueType);
  victronModbusPendingField = victronModbusNextField;
  victronModbusNextField = (ModbusBatteryPollField)(((uint8_t)victronModbusNextField + 1) % (uint8_t)ModbusBatteryPollField::COUNT);
  Error error = victronModbusClient.addRequest(token,
                                               modbusBatteryConfig.id,
                                               READ_HOLD_REGISTER,
                                               field.registerAddress,
                                               words);
  if (error != SUCCESS) {
    victronModbusRequestPending = false;
    DebugVerboseTf("Victron Modbus queue error: 0x%02X\r\n", (uint8_t)error);
  }
}

// A configurable Modbus TCP PV mapper. SolarEdge SunSpec addresses are only
// its defaults: AC power 40083/40084 and lifetime energy 40094..40096.
enum class SunSpecPvPollField : uint8_t { POWER, POWER_SCALE, ENERGY, ENERGY_SCALE, COUNT };
static WiFiClient sunSpecPvSocket;
static ModbusClientTCP sunSpecPvClient(sunSpecPvSocket, 2);
static bool sunSpecPvClientStarted = false;
static bool sunSpecPvTargetValid = false;
static volatile bool sunSpecPvRequestPending = false;
static volatile uint32_t sunSpecPvActiveToken = 0;
static uint32_t sunSpecPvToken = 0;
static uint32_t sunSpecPvLastPoll = 0;
static SunSpecPvPollField sunSpecPvPendingField = SunSpecPvPollField::POWER;
static SunSpecPvPollField sunSpecPvNextField = SunSpecPvPollField::POWER;
static float sunSpecPvPower = 0;
static int16_t sunSpecPvPowerScale = 0;
static float sunSpecPvEnergy = 0;
static int16_t sunSpecPvEnergyScale = 0;
static float sunSpecPvDayStartEnergy = 0;
static uint8_t sunSpecPvEnergyDay = 0;

static uint16_t sunSpecPvRegister(SunSpecPvPollField field) {
  switch (field) {
    case SunSpecPvPollField::POWER: return sunSpecPvConfig.activePower.registerAddress;
    case SunSpecPvPollField::POWER_SCALE: return sunSpecPvConfig.activePowerScaleRegister;
    case SunSpecPvPollField::ENERGY: return sunSpecPvConfig.dailyEnergy.registerAddress;
    default: return sunSpecPvConfig.dailyEnergyScaleRegister;
  }
}

static uint8_t sunSpecPvWordCount(SunSpecPvPollField field) {
  if (field == SunSpecPvPollField::POWER) return modbusBatteryWordCount(sunSpecPvConfig.activePower.valueType);
  if (field == SunSpecPvPollField::ENERGY) return modbusBatteryWordCount(sunSpecPvConfig.dailyEnergy.valueType);
  return 1;
}

static float sunSpecScale(int16_t exponent) {
  float scale = 1.0f;
  while (exponent > 0) { scale *= 10.0f; exponent--; }
  while (exponent < 0) { scale /= 10.0f; exponent++; }
  return scale;
}

static void updateSunSpecPvOutput() {
  if (pvConnectorDriver != PV_DRIVER_MODBUS_TCP) return;
  float watts = sunSpecPvPower *
                (sunSpecPvConfig.useRegisterScaleFactors ? sunSpecScale(sunSpecPvPowerScale) : 1.0f);
  SolarEdge.Actual = watts > 0.0f ? (uint32_t)watts : 0;
  const uint8_t today = day();
  if (!sunSpecPvEnergyDay || sunSpecPvEnergyDay != today || sunSpecPvEnergy < sunSpecPvDayStartEnergy) {
    sunSpecPvEnergyDay = today;
    sunSpecPvDayStartEnergy = sunSpecPvEnergy;
  }
  float energyWh = (sunSpecPvEnergy - sunSpecPvDayStartEnergy) *
                   (sunSpecPvConfig.useRegisterScaleFactors ? sunSpecScale(sunSpecPvEnergyScale) : 1.0f);
  SolarEdge.Daily = energyWh > 0.0f ? (uint32_t)energyWh : 0;
  updatePvResourceValues("modbus-tcp", "modbus-tcp", "sunspec-inverter", sunSpecPvConfig.id,
                         SolarEdge.Actual, SolarEdge.Daily, pvWattPeak);
}

static void handleSunSpecPvData(ModbusMessage response, uint32_t token) {
  if (token != sunSpecPvActiveToken) return;
  sunSpecPvRequestPending = false;
  const uint8_t words = sunSpecPvWordCount(sunSpecPvPendingField);
  if (response.getServerID() != sunSpecPvConfig.id || response.getFunctionCode() != READ_HOLD_REGISTER ||
      response.size() < (size_t)(3 + words * 2) || response[2] != words * 2) {
    DebugVerboseTln(F("SunSpec PV: invalid response"));
    return;
  }
  uint16_t first = 0, second = 0;
  if (words == 1) response.get(3, first); else response.get(3, first, second);
  float value = 0; int32_t nativeValue = 0;
  switch (sunSpecPvPendingField) {
    case SunSpecPvPollField::POWER:
      if (!decodeModbusBatteryValue(sunSpecPvConfig.activePower, first, second, value, nativeValue)) return;
      sunSpecPvPower = value;
      break;
    case SunSpecPvPollField::POWER_SCALE: sunSpecPvPowerScale = decodeModbusS16(first); break;
    case SunSpecPvPollField::ENERGY:
      if (!decodeModbusBatteryValue(sunSpecPvConfig.dailyEnergy, first, second, value, nativeValue)) return;
      sunSpecPvEnergy = value;
      if (!sunSpecPvConfig.useRegisterScaleFactors) updateSunSpecPvOutput();
      break;
    case SunSpecPvPollField::ENERGY_SCALE:
      sunSpecPvEnergyScale = decodeModbusS16(first);
      updateSunSpecPvOutput();
      break;
    default: break;
  }
}

static void handleSunSpecPvError(Error error, uint32_t token) {
  if (token != sunSpecPvActiveToken) return;
  sunSpecPvRequestPending = false;
  DebugVerboseTf("SunSpec PV Modbus error: 0x%02X\r\n", (uint8_t)error);
}

void sunSpecPvConfigChanged() {
  sunSpecPvActiveToken = 0;
  sunSpecPvRequestPending = false;
  sunSpecPvTargetValid = false;
  sunSpecPvSocket.stop();
  IPAddress target;
  if (pvConnectorDriver != PV_DRIVER_MODBUS_TCP) return;
  if (!target.fromString(sunSpecPvConfig.ip)) {
    SolarEdge.Available = false;
    SolarEdge.Actual = 0;
    SolarEdge.Daily = 0;
    return;
  }
  if (!sunSpecPvClientStarted) {
    sunSpecPvClient.onDataHandler(&handleSunSpecPvData);
    sunSpecPvClient.onErrorHandler(&handleSunSpecPvError);
    sunSpecPvClient.setTimeout(1500, 200);
    sunSpecPvClient.begin();
    sunSpecPvClientStarted = true;
  }
  sunSpecPvClient.setTarget(target, sunSpecPvConfig.port);
  sunSpecPvTargetValid = true;
  sunSpecPvLastPoll = 0;
  SolarEdge.Available = true;
  SolarEdge.Wp = pvWattPeak;
  SolarEdge.Actual = 0;
  SolarEdge.Daily = 0;
  DebugVerboseTf("SunSpec PV target: %s:%u id=%u\r\n", sunSpecPvConfig.ip, sunSpecPvConfig.port, sunSpecPvConfig.id);
}

void setupModbusPv() { sunSpecPvConfigChanged(); }

void handleModbusPv() {
  if (pvConnectorDriver != PV_DRIVER_MODBUS_TCP || !sunSpecPvTargetValid ||
      (netw_state != NW_ETH && netw_state != NW_WIFI) || sunSpecPvRequestPending) return;
  const uint32_t interval = max(250UL, (uint32_t)constrain((int)sunSpecPvConfig.pollIntervalSeconds, 1, 3600) * 250UL);
  const uint32_t nowMs = millis();
  if (sunSpecPvLastPoll && nowMs - sunSpecPvLastPoll < interval) return;
  sunSpecPvLastPoll = nowMs;
  // A generic device may provide values with manual factors only; do not
  // request scale-factor registers in that mode.
  if (!sunSpecPvConfig.useRegisterScaleFactors &&
      (sunSpecPvNextField == SunSpecPvPollField::POWER_SCALE || sunSpecPvNextField == SunSpecPvPollField::ENERGY_SCALE)) {
    sunSpecPvNextField = sunSpecPvNextField == SunSpecPvPollField::POWER_SCALE
        ? SunSpecPvPollField::ENERGY : SunSpecPvPollField::POWER;
  }
  const uint32_t token = ++sunSpecPvToken;
  sunSpecPvActiveToken = token;
  sunSpecPvRequestPending = true;
  sunSpecPvPendingField = sunSpecPvNextField;
  sunSpecPvNextField = (SunSpecPvPollField)(((uint8_t)sunSpecPvNextField + 1) % (uint8_t)SunSpecPvPollField::COUNT);
  Error error = sunSpecPvClient.addRequest(token, sunSpecPvConfig.id, READ_HOLD_REGISTER,
                                            sunSpecPvRegister(sunSpecPvPendingField), sunSpecPvWordCount(sunSpecPvPendingField));
  if (error != SUCCESS) {
    sunSpecPvRequestPending = false;
    DebugVerboseTf("SunSpec PV Modbus queue error: 0x%02X\r\n", (uint8_t)error);
  }
}

static constexpr uint8_t kModbusTransportTcp = 0;
static constexpr uint8_t kModbusTransportRtu = 1;

static constexpr size_t kModbusMonitorCapacity = 30;

struct ModbusMonitorEntry {
  char timestamp[20];
  uint8_t serverId;
  uint8_t functionCode;
  uint16_t address;
  uint16_t words;
  uint8_t resultCode;
  uint8_t transport;
};

static ModbusMonitorEntry g_modbusMonitorEntries[kModbusMonitorCapacity];
static size_t g_modbusMonitorCount = 0;
static size_t g_modbusMonitorHead = 0;

static const char* modbusTransportText(uint8_t transport) {
  switch (transport) {
    case kModbusTransportTcp: return "TCP";
    case kModbusTransportRtu: return "RTU";
  }
  return "?";
}

static void logModbusMonitorRequest(uint8_t transport, const ModbusMessage& request, uint16_t address, uint16_t words, uint8_t resultCode) {
  if (!bModbusMonitor) return;

  ModbusMonitorEntry& entry = g_modbusMonitorEntries[g_modbusMonitorHead];
  strlcpy(entry.timestamp, actTimestamp, sizeof(entry.timestamp));
  entry.serverId = request.getServerID();
  entry.functionCode = request.getFunctionCode();
  entry.address = address;
  entry.words = words;
  entry.resultCode = resultCode;
  entry.transport = transport;

  g_modbusMonitorHead = (g_modbusMonitorHead + 1) % kModbusMonitorCapacity;
  if (g_modbusMonitorCount < kModbusMonitorCapacity) g_modbusMonitorCount++;
}

String modbusMonitorJson() {
  JsonDocument doc;
  doc["enabled"] = bModbusMonitor;
  doc["capacity"] = kModbusMonitorCapacity;
  doc["count"] = g_modbusMonitorCount;

  JsonArray data = doc["data"].to<JsonArray>();
  for (size_t i = 0; i < g_modbusMonitorCount; i++) {
    const size_t index = (g_modbusMonitorHead + kModbusMonitorCapacity - 1 - i) % kModbusMonitorCapacity;
    const ModbusMonitorEntry& entry = g_modbusMonitorEntries[index];
    JsonObject row = data.add<JsonObject>();
    row["timestamp"] = entry.timestamp;
    row["mode"] = modbusTransportText(entry.transport);
    row["id"] = entry.serverId;
    row["fc"] = entry.functionCode;
    row["address"] = entry.address;
    row["words"] = entry.words;
    row["result"] = (entry.resultCode == 0) ? "ok" : "error";
    if (entry.resultCode != 0) row["error_code"] = entry.resultCode;
  }

  String body;
  serializeJson(doc, body);
  return body;
}

void clearModbusMonitorEntries() {
  g_modbusMonitorCount = 0;
  g_modbusMonitorHead = 0;
}

// Modbus data types
enum class ModbusDataType : uint8_t { UINT32, INT32, INT16, FLOAT };

enum class MbSource : uint8_t {
  timestamp_epoch,
  energy_delivered_tariff1_kwh,
  energy_delivered_tariff2_kwh,
  energy_returned_tariff1_kwh,
  energy_returned_tariff2_kwh,
  energy_delivered_total_kwh,
  energy_returned_total_kwh,
  energy_total_abs_kwh,
  reactive_energy_total_varh,
  energy_net_total_kwh,
  energy_net_tariff1_kwh,
  energy_net_tariff2_kwh,
  energy_net_avg_kwh,
  energy_delivered_avg_kwh,
  energy_returned_avg_kwh,
  power_delivered_kw,
  power_returned_kw,
  net_power_total_kw,
  voltage_l1_v,
  voltage_l2_v,
  voltage_l3_v,
  phase_voltage_avg_v,
  current_l1_a,
  current_l2_a,
  current_l3_a,
  current_total_a,
  signed_current_l1_a,
  signed_current_l2_a,
  signed_current_l3_a,
  gas_timestamp_epoch,
  gas_delivered_m3,
  electricity_tariff,
  peak_pwr_last_q_kw,
  net_power_l1_kw,
  net_power_l2_kw,
  net_power_l3_kw,
  power_factor_total,
  power_factor_l1,
  power_factor_l2,
  power_factor_l3,
  apparent_power_total_va,
  direction_total,
  direction_l1,
  direction_l2,
  direction_l3,
  line_voltage_l12_v,
  line_voltage_l23_v,
  line_voltage_l31_v,
  line_voltage_avg_v,
  water_delivered_m3,
  unavailable_float,
  constant,
  p1_device_id,
  device_serial_u32,
  firmware_version_packed,
  device_online,
  uptime_seconds,
  power_delivered_l1_kw,
  power_delivered_l2_kw,
  power_delivered_l3_kw,
  power_returned_l1_kw,
  power_returned_l2_kw,
  power_returned_l3_kw,
};

struct ActiveRecipe {
  uint16_t registerAddress;
  int16_t scale;
  uint8_t source;
  uint8_t type;
  int32_t value;
  float valueFloat = NAN;
};

static const ActiveRecipe* activeRecipes = nullptr;
static size_t activeRecipeCount = 0;
static uint16_t activeRecipeMaxReg = 0;
static bool activeRecipeLswFirst = false;
static constexpr int kModbusMappingEm24Tcp = 16;
static constexpr int kModbusMappingFroniusSunSpec203 = 15;

#include "_mbus_mapping.h"

static float mbSignedCurrent(float current, float returned);
static float readMbSourceValue(MbSource source);
static float readScaledMbSourceValue(MbSource source, int16_t scale);
static bool readMbSourceUInt32(MbSource source, int16_t scale, uint32_t value, uint32_t& outValue);
static uint32_t encodeActiveRecipeValue(const ActiveRecipe& recipe);
static const ActiveRecipe* findActiveRecipe(uint16_t reg);
static bool loadActiveRecipes(const ActiveRecipe* recipes, size_t recipeCount);
static bool loadPresetRecipes(int mappingChoice);
static int32_t em24ScaledValue(MbSource source, int16_t scale);

static float mbSignedCurrent(float current, float returned) {
  return current * (returned ? -1.0f : 1.0f);
}

static float readMbSourceValue(MbSource source) {
  switch (source) {
    case MbSource::timestamp_epoch:
      return (float)(actT - (actTimestamp[12] == 'S' ? 7200 : 3600));
    case MbSource::energy_delivered_tariff1_kwh:
      return (DSMRdata.energy_delivered_tariff1_present && !bUseEtotals) ? outputEnergy(DSMRdata.energy_delivered_tariff1.val()) : NAN;
    case MbSource::energy_delivered_tariff2_kwh:
      return (DSMRdata.energy_delivered_tariff2_present && !bUseEtotals) ? outputEnergy(DSMRdata.energy_delivered_tariff2.val()) : NAN;
    case MbSource::energy_returned_tariff1_kwh:
      return (DSMRdata.energy_returned_tariff1_present && !bUseEtotals) ? outputEnergy(DSMRdata.energy_returned_tariff1.val()) : NAN;
    case MbSource::energy_returned_tariff2_kwh:
      return (DSMRdata.energy_returned_tariff2_present && !bUseEtotals) ? outputEnergy(DSMRdata.energy_returned_tariff2.val()) : NAN;
    case MbSource::energy_delivered_total_kwh:
      return outputEnergy(DSMRdata.energy_delivered_total_present
        ? DSMRdata.energy_delivered_total.val()
        : ((DSMRdata.energy_delivered_tariff1_present ? DSMRdata.energy_delivered_tariff1.val() : 0.0f) +
           (DSMRdata.energy_delivered_tariff2_present ? DSMRdata.energy_delivered_tariff2.val() : 0.0f)));
    case MbSource::energy_returned_total_kwh:
      return outputEnergy(DSMRdata.energy_returned_total_present
        ? DSMRdata.energy_returned_total.val()
        : ((DSMRdata.energy_returned_tariff1_present ? DSMRdata.energy_returned_tariff1.val() : 0.0f) +
           (DSMRdata.energy_returned_tariff2_present ? DSMRdata.energy_returned_tariff2.val() : 0.0f)));
    case MbSource::energy_total_abs_kwh: {
      const float delivered = readMbSourceValue(MbSource::energy_delivered_total_kwh);
      const float returned = readMbSourceValue(MbSource::energy_returned_total_kwh);
      if (isnan(delivered) && isnan(returned)) return NAN;
      return (isnan(delivered) ? 0.0f : delivered) + (isnan(returned) ? 0.0f : returned);
    }
    case MbSource::reactive_energy_total_varh:
      return 0.0f;
    case MbSource::energy_net_total_kwh:
      return
        (DSMRdata.energy_delivered_tariff1_present || DSMRdata.energy_returned_tariff1_present ||
         DSMRdata.energy_delivered_tariff2_present || DSMRdata.energy_returned_tariff2_present)
          ? outputEnergy((float)(
              (DSMRdata.energy_delivered_tariff1_present ? DSMRdata.energy_delivered_tariff1.val() : 0.0f) -
              (DSMRdata.energy_returned_tariff1_present ? DSMRdata.energy_returned_tariff1.val() : 0.0f) +
              (DSMRdata.energy_delivered_tariff2_present ? DSMRdata.energy_delivered_tariff2.val() : 0.0f) -
              (DSMRdata.energy_returned_tariff2_present ? DSMRdata.energy_returned_tariff2.val() : 0.0f)
            ))
          : NAN;
    case MbSource::energy_net_tariff1_kwh:
      return
        (DSMRdata.energy_delivered_tariff1_present || DSMRdata.energy_returned_tariff1_present)
          ? outputEnergy((float)(
              (DSMRdata.energy_delivered_tariff1_present ? DSMRdata.energy_delivered_tariff1.val() : 0.0f) -
              (DSMRdata.energy_returned_tariff1_present ? DSMRdata.energy_returned_tariff1.val() : 0.0f)
            ))
          : NAN;
    case MbSource::energy_net_tariff2_kwh:
      return
        (DSMRdata.energy_delivered_tariff2_present || DSMRdata.energy_returned_tariff2_present)
          ? outputEnergy((float)(
              (DSMRdata.energy_delivered_tariff2_present ? DSMRdata.energy_delivered_tariff2.val() : 0.0f) -
              (DSMRdata.energy_returned_tariff2_present ? DSMRdata.energy_returned_tariff2.val() : 0.0f)
            ))
          : NAN;
    case MbSource::energy_net_avg_kwh: {
      float value = readMbSourceValue(MbSource::energy_net_total_kwh);
      return isnan(value) ? NAN : (value / 3.0f);
    }
    case MbSource::energy_delivered_avg_kwh: {
      float value = readMbSourceValue(MbSource::energy_delivered_total_kwh);
      return isnan(value) ? NAN : (value / 3.0f);
    }
    case MbSource::energy_returned_avg_kwh: {
      float value = readMbSourceValue(MbSource::energy_returned_total_kwh);
      return isnan(value) ? NAN : (value / 3.0f);
    }
    case MbSource::power_delivered_kw:
      return DSMRdata.power_delivered_present ? outputPower(DSMRdata.power_delivered.val()) : NAN;
    case MbSource::power_returned_kw:
      return DSMRdata.power_returned_present ? outputPower(DSMRdata.power_returned.val()) : NAN;
    case MbSource::power_delivered_l1_kw:
      return DSMRdata.power_delivered_l1_present ? outputPower(DSMRdata.power_delivered_l1.val()) : NAN;
    case MbSource::power_delivered_l2_kw:
      return DSMRdata.power_delivered_l2_present ? outputPower(DSMRdata.power_delivered_l2.val()) : NAN;
    case MbSource::power_delivered_l3_kw:
      return DSMRdata.power_delivered_l3_present ? outputPower(DSMRdata.power_delivered_l3.val()) : NAN;
    case MbSource::power_returned_l1_kw:
      return DSMRdata.power_returned_l1_present ? outputPower(DSMRdata.power_returned_l1.val()) : NAN;
    case MbSource::power_returned_l2_kw:
      return DSMRdata.power_returned_l2_present ? outputPower(DSMRdata.power_returned_l2.val()) : NAN;
    case MbSource::power_returned_l3_kw:
      return DSMRdata.power_returned_l3_present ? outputPower(DSMRdata.power_returned_l3.val()) : NAN;
    case MbSource::net_power_total_kw:
      return DSMRdata.power_delivered_present ? outputPower(DSMRdata.power_delivered.val() - DSMRdata.power_returned.val()) : NAN;
    case MbSource::voltage_l1_v:
      return DSMRdata.voltage_l1_present ? outputVoltage((float)DSMRdata.voltage_l1.val()) : NAN;
    case MbSource::voltage_l2_v:
      return DSMRdata.voltage_l2_present ? outputVoltage((float)DSMRdata.voltage_l2.val()) : NAN;
    case MbSource::voltage_l3_v:
      return DSMRdata.voltage_l3_present ? outputVoltage((float)DSMRdata.voltage_l3.val()) : NAN;
    case MbSource::phase_voltage_avg_v: {
      float value = 0.0f;
      uint8_t count = 0;
      if (DSMRdata.voltage_l1_present) { value += (float)DSMRdata.voltage_l1.val(); count++; }
      if (DSMRdata.voltage_l2_present) { value += (float)DSMRdata.voltage_l2.val(); count++; }
      if (DSMRdata.voltage_l3_present) { value += (float)DSMRdata.voltage_l3.val(); count++; }
      return count ? outputVoltage(value / (float)count) : NAN;
    }
    case MbSource::current_l1_a:
      return DSMRdata.current_l1_present ? outputCurrent((float)DSMRdata.current_l1.val()) : NAN;
    case MbSource::current_l2_a:
      return DSMRdata.current_l2_present ? outputCurrent((float)DSMRdata.current_l2.val()) : NAN;
    case MbSource::current_l3_a:
      return DSMRdata.current_l3_present ? outputCurrent((float)DSMRdata.current_l3.val()) : NAN;
    case MbSource::current_total_a:
      return DSMRdata.current_l1_present
        ? outputCurrent((float)(DSMRdata.current_l1.val() + DSMRdata.current_l2.val() + DSMRdata.current_l3.val()))
        : NAN;
    case MbSource::signed_current_l1_a:
      return DSMRdata.current_l1_present ? outputCurrent(mbSignedCurrent((float)DSMRdata.current_l1.val(), DSMRdata.power_returned_l1.val())) : NAN;
    case MbSource::signed_current_l2_a:
      return DSMRdata.current_l2_present ? outputCurrent(mbSignedCurrent((float)DSMRdata.current_l2.val(), DSMRdata.power_returned_l2.val())) : NAN;
    case MbSource::signed_current_l3_a:
      return DSMRdata.current_l3_present ? outputCurrent(mbSignedCurrent((float)DSMRdata.current_l3.val(), DSMRdata.power_returned_l3.val())) : NAN;
    case MbSource::gas_timestamp_epoch:
      return mbusGas ? (float)(epoch(gasDeliveredTimestamp.c_str(), 10, false) - (actTimestamp[12] == 'S' ? 7200 : 3600)) : NAN;
    case MbSource::gas_delivered_m3:
      return mbusGas ? (float)gasDelivered : NAN;
    case MbSource::electricity_tariff:
      return DSMRdata.electricity_tariff_present ? (float)atoi(DSMRdata.electricity_tariff.c_str()) : NAN;
    case MbSource::peak_pwr_last_q_kw:
      return DSMRdata.peak_pwr_last_q_present ? outputPower((float)DSMRdata.peak_pwr_last_q.val()) : NAN;
    case MbSource::net_power_l1_kw:
      return DSMRdata.power_delivered_l1_present ? outputPower((float)(DSMRdata.power_delivered_l1.val() - DSMRdata.power_returned_l1.val())) : NAN;
    case MbSource::net_power_l2_kw:
      return DSMRdata.power_delivered_l2_present ? outputPower((float)(DSMRdata.power_delivered_l2.val() - DSMRdata.power_returned_l2.val())) : NAN;
    case MbSource::net_power_l3_kw:
      return DSMRdata.power_delivered_l3_present ? outputPower((float)(DSMRdata.power_delivered_l3.val() - DSMRdata.power_returned_l3.val())) : NAN;
    case MbSource::power_factor_total: {
      float value = readMbSourceValue(MbSource::net_power_total_kw);
      return isnan(value) ? NAN : (value < 0.0f ? -1.0f : 1.0f);
    }
    case MbSource::power_factor_l1: {
      float value = readMbSourceValue(MbSource::net_power_l1_kw);
      return isnan(value) ? NAN : (value < 0.0f ? -1.0f : 1.0f);
    }
    case MbSource::power_factor_l2: {
      float value = readMbSourceValue(MbSource::net_power_l2_kw);
      return isnan(value) ? NAN : (value < 0.0f ? -1.0f : 1.0f);
    }
    case MbSource::power_factor_l3: {
      float value = readMbSourceValue(MbSource::net_power_l3_kw);
      return isnan(value) ? NAN : (value < 0.0f ? -1.0f : 1.0f);
    }
    case MbSource::apparent_power_total_va: {
      float value = readMbSourceValue(MbSource::net_power_total_kw);
      return isnan(value) ? NAN : fabsf(value) * 1000.0f;
    }
    case MbSource::direction_total:
      return DSMRdata.power_returned > 0 ? -1.0f : 1.0f;
    case MbSource::direction_l1:
      return DSMRdata.power_returned_l1 > 0 ? -1.0f : 1.0f;
    case MbSource::direction_l2:
      return DSMRdata.power_returned_l2_present ? (DSMRdata.power_returned_l2 > 0 ? -1.0f : 1.0f) : NAN;
    case MbSource::direction_l3:
      return DSMRdata.power_returned_l3_present ? (DSMRdata.power_returned_l3 > 0 ? -1.0f : 1.0f) : NAN;
    case MbSource::line_voltage_l12_v:
      return (DSMRdata.voltage_l1_present && DSMRdata.voltage_l2_present) ? outputVoltage(calculateLineVoltage(DSMRdata.voltage_l1, DSMRdata.voltage_l2)) : NAN;
    case MbSource::line_voltage_l23_v:
      return (DSMRdata.voltage_l2_present && DSMRdata.voltage_l3_present) ? outputVoltage(calculateLineVoltage(DSMRdata.voltage_l2, DSMRdata.voltage_l3)) : NAN;
    case MbSource::line_voltage_l31_v:
      return (DSMRdata.voltage_l3_present && DSMRdata.voltage_l1_present) ? outputVoltage(calculateLineVoltage(DSMRdata.voltage_l3, DSMRdata.voltage_l1)) : NAN;
    case MbSource::line_voltage_avg_v: {
      float value = 0.0f;
      value += (DSMRdata.voltage_l1_present && DSMRdata.voltage_l2_present) ? calculateLineVoltage(DSMRdata.voltage_l1, DSMRdata.voltage_l2) : 0.0f;
      value += (DSMRdata.voltage_l2_present && DSMRdata.voltage_l3_present) ? calculateLineVoltage(DSMRdata.voltage_l2, DSMRdata.voltage_l3) : 0.0f;
      value += (DSMRdata.voltage_l3_present && DSMRdata.voltage_l1_present) ? calculateLineVoltage(DSMRdata.voltage_l3, DSMRdata.voltage_l1) : 0.0f;
      return outputVoltage(value / 3.0f);
    }
    case MbSource::water_delivered_m3:
      return mbusWater ? (float)waterDelivered
                       : (WtrMtr ? (float)(P1Status.wtr_m3 + (P1Status.wtr_l / 1000.0f)) : NAN);
    case MbSource::unavailable_float:
      return NAN;
    case MbSource::constant:
      return NAN;
    case MbSource::p1_device_id:
      return 0x5031444F;
    case MbSource::device_serial_u32:
      return (float)(uint32_t)_getChipId();
    case MbSource::firmware_version_packed:
      return (float)packed_version_u32();
    case MbSource::device_online:
      return (float)(bP1offline ? 0 : 1);
    case MbSource::uptime_seconds:
      return (float)(esp_log_timestamp() / 1000);
  }

  return NAN;
}

static float readScaledMbSourceValue(MbSource source, int16_t scale) {
  if (settingCTFactor != 1 || settingVTFactor != 1) {
    float value = readMbSourceValue(source);
    return isnan(value) ? NAN : value * scale;
  }
  if (scale != 1000 && scale != -1000 && scale != 10000 && scale != -10000) {
    float value = readMbSourceValue(source);
    return isnan(value) ? NAN : value * scale;
  }

  const float sign = (scale < 0) ? -1.0f : 1.0f;

  if (scale == 10000 || scale == -10000) {
    switch (source) {
      case MbSource::net_power_total_kw:
        return DSMRdata.power_delivered_present
          ? sign * (float)(((int32_t)DSMRdata.power_delivered.int_val() - (int32_t)DSMRdata.power_returned.int_val()) * 10)
          : NAN;
      case MbSource::net_power_l1_kw:
        return DSMRdata.power_delivered_l1_present
          ? sign * (float)(((int32_t)DSMRdata.power_delivered_l1.int_val() - (int32_t)DSMRdata.power_returned_l1.int_val()) * 10)
          : NAN;
      case MbSource::net_power_l2_kw:
        return DSMRdata.power_delivered_l2_present
          ? sign * (float)(((int32_t)DSMRdata.power_delivered_l2.int_val() - (int32_t)DSMRdata.power_returned_l2.int_val()) * 10)
          : NAN;
      case MbSource::net_power_l3_kw:
        return DSMRdata.power_delivered_l3_present
          ? sign * (float)(((int32_t)DSMRdata.power_delivered_l3.int_val() - (int32_t)DSMRdata.power_returned_l3.int_val()) * 10)
          : NAN;
      default: {
        float value = readMbSourceValue(source);
        return isnan(value) ? NAN : value * scale;
      }
    }
  }

  switch (source) {
    case MbSource::energy_delivered_tariff1_kwh:
      return (DSMRdata.energy_delivered_tariff1_present && !bUseEtotals) ? sign * DSMRdata.energy_delivered_tariff1.int_val() : NAN;
    case MbSource::energy_delivered_tariff2_kwh:
      return (DSMRdata.energy_delivered_tariff2_present && !bUseEtotals) ? sign * DSMRdata.energy_delivered_tariff2.int_val() : NAN;
    case MbSource::energy_returned_tariff1_kwh:
      return (DSMRdata.energy_returned_tariff1_present && !bUseEtotals) ? sign * DSMRdata.energy_returned_tariff1.int_val() : NAN;
    case MbSource::energy_returned_tariff2_kwh:
      return (DSMRdata.energy_returned_tariff2_present && !bUseEtotals) ? sign * DSMRdata.energy_returned_tariff2.int_val() : NAN;
    case MbSource::energy_delivered_total_kwh:
      return DSMRdata.energy_delivered_total_present
        ? sign * DSMRdata.energy_delivered_total.int_val()
        : sign * ((DSMRdata.energy_delivered_tariff1_present ? DSMRdata.energy_delivered_tariff1.int_val() : 0) +
                  (DSMRdata.energy_delivered_tariff2_present ? DSMRdata.energy_delivered_tariff2.int_val() : 0));
    case MbSource::energy_returned_total_kwh:
      return DSMRdata.energy_returned_total_present
        ? sign * DSMRdata.energy_returned_total.int_val()
        : sign * ((DSMRdata.energy_returned_tariff1_present ? DSMRdata.energy_returned_tariff1.int_val() : 0) +
                  (DSMRdata.energy_returned_tariff2_present ? DSMRdata.energy_returned_tariff2.int_val() : 0));
    case MbSource::power_delivered_kw:
      return DSMRdata.power_delivered_present ? sign * DSMRdata.power_delivered.int_val() : NAN;
    case MbSource::power_returned_kw:
      return DSMRdata.power_returned_present ? sign * DSMRdata.power_returned.int_val() : NAN;
    case MbSource::power_delivered_l1_kw:
      return DSMRdata.power_delivered_l1_present ? sign * DSMRdata.power_delivered_l1.int_val() : NAN;
    case MbSource::power_delivered_l2_kw:
      return DSMRdata.power_delivered_l2_present ? sign * DSMRdata.power_delivered_l2.int_val() : NAN;
    case MbSource::power_delivered_l3_kw:
      return DSMRdata.power_delivered_l3_present ? sign * DSMRdata.power_delivered_l3.int_val() : NAN;
    case MbSource::power_returned_l1_kw:
      return DSMRdata.power_returned_l1_present ? sign * DSMRdata.power_returned_l1.int_val() : NAN;
    case MbSource::power_returned_l2_kw:
      return DSMRdata.power_returned_l2_present ? sign * DSMRdata.power_returned_l2.int_val() : NAN;
    case MbSource::power_returned_l3_kw:
      return DSMRdata.power_returned_l3_present ? sign * DSMRdata.power_returned_l3.int_val() : NAN;
    case MbSource::voltage_l1_v:
      return DSMRdata.voltage_l1_present ? sign * (float)DSMRdata.voltage_l1.int_val() : NAN;
    case MbSource::voltage_l2_v:
      return DSMRdata.voltage_l2_present ? sign * (float)DSMRdata.voltage_l2.int_val() : NAN;
    case MbSource::voltage_l3_v:
      return DSMRdata.voltage_l3_present ? sign * (float)DSMRdata.voltage_l3.int_val() : NAN;
    case MbSource::phase_voltage_avg_v: {
      int32_t value = 0;
      uint8_t count = 0;
      if (DSMRdata.voltage_l1_present) { value += DSMRdata.voltage_l1.int_val(); count++; }
      if (DSMRdata.voltage_l2_present) { value += DSMRdata.voltage_l2.int_val(); count++; }
      if (DSMRdata.voltage_l3_present) { value += DSMRdata.voltage_l3.int_val(); count++; }
      return count ? sign * (float)(value / count) : NAN;
    }
    case MbSource::current_l1_a:
      return DSMRdata.current_l1_present ? sign * (float)DSMRdata.current_l1.int_val() : NAN;
    case MbSource::current_l2_a:
      return DSMRdata.current_l2_present ? sign * (float)DSMRdata.current_l2.int_val() : NAN;
    case MbSource::current_l3_a:
      return DSMRdata.current_l3_present ? sign * (float)DSMRdata.current_l3.int_val() : NAN;
    case MbSource::current_total_a:
      return DSMRdata.current_l1_present
        ? sign * (float)(DSMRdata.current_l1.int_val() + DSMRdata.current_l2.int_val() + DSMRdata.current_l3.int_val())
        : NAN;
    case MbSource::peak_pwr_last_q_kw:
      return DSMRdata.peak_pwr_last_q_present ? sign * DSMRdata.peak_pwr_last_q.int_val() : NAN;
    case MbSource::net_power_total_kw:
      return DSMRdata.power_delivered_present
        ? sign * (float)((int32_t)DSMRdata.power_delivered.int_val() - (int32_t)DSMRdata.power_returned.int_val())
        : NAN;
    case MbSource::net_power_l1_kw:
      return DSMRdata.power_delivered_l1_present
        ? sign * (float)((int32_t)DSMRdata.power_delivered_l1.int_val() - (int32_t)DSMRdata.power_returned_l1.int_val())
        : NAN;
    case MbSource::net_power_l2_kw:
      return DSMRdata.power_delivered_l2_present
        ? sign * (float)((int32_t)DSMRdata.power_delivered_l2.int_val() - (int32_t)DSMRdata.power_returned_l2.int_val())
        : NAN;
    case MbSource::net_power_l3_kw:
      return DSMRdata.power_delivered_l3_present
        ? sign * (float)((int32_t)DSMRdata.power_delivered_l3.int_val() - (int32_t)DSMRdata.power_returned_l3.int_val())
        : NAN;
    case MbSource::gas_delivered_m3:
      return mbusGas ? sign * (float)lroundf((float)gasDelivered * 1000.0f) : NAN;
    case MbSource::water_delivered_m3:
      return mbusWater ? sign * (float)lroundf(waterDelivered * 1000.0f)
                       : (WtrMtr ? sign * (float)(P1Status.wtr_m3 * 1000 + P1Status.wtr_l) : NAN);
    default: {
      float value = readMbSourceValue(source);
      return isnan(value) ? NAN : value * scale;
    }
  }
}

static bool readMbSourceUInt32(MbSource source, int16_t scale, uint32_t value, uint32_t& outValue) {
  if (source == MbSource::constant && scale == 1) {
    outValue = (uint32_t)value;
    return true;
  }

  if (scale != 1) return false;

  switch (source) {
    case MbSource::timestamp_epoch:
      outValue = (uint32_t)(actT - (actTimestamp[12] == 'S' ? 7200 : 3600));
      return true;
    case MbSource::gas_timestamp_epoch:
      if (!mbusGas) return false;
      outValue = (uint32_t)(epoch(gasDeliveredTimestamp.c_str(), 10, false) - (actTimestamp[12] == 'S' ? 7200 : 3600));
      return true;
    case MbSource::electricity_tariff:
      if (!DSMRdata.electricity_tariff_present) return false;
      outValue = (uint32_t)atoi(DSMRdata.electricity_tariff.c_str());
      return true;
    case MbSource::p1_device_id:
      outValue = 0x5031444F;
      return true;
    case MbSource::device_serial_u32:
      outValue = (uint32_t)_getChipId();
      return true;
    case MbSource::firmware_version_packed:
      outValue = packed_version_u32();
      return true;
    case MbSource::device_online:
      outValue = bP1offline ? 0U : 1U;
      return true;
    case MbSource::uptime_seconds:
      outValue = (uint32_t)(esp_log_timestamp() / 1000);
      return true;
    default:
      return false;
  }
}

static uint32_t encodeActiveRecipeValue(const ActiveRecipe& recipe) {
  const MbSource source = static_cast<MbSource>(recipe.source);
  const ModbusDataType type = static_cast<ModbusDataType>(recipe.type);

  if (type == ModbusDataType::UINT32) {
    uint32_t u32Value = 0;
    if (readMbSourceUInt32(source, recipe.scale, recipe.value, u32Value)) {
      return u32Value;
    }
  }

  float value = (source == MbSource::constant)
    ? ((type == ModbusDataType::FLOAT && !isnan(recipe.valueFloat)) ? recipe.valueFloat : (float)recipe.value)
    : readScaledMbSourceValue(source, recipe.scale);

  if (type == ModbusDataType::FLOAT) {
    return packF(value);
  }

  if (isnan(value)) {
    return MBUS_VAL_UNAVAILABLE;
  }

  switch (type) {
    case ModbusDataType::UINT32:
      return (uint32_t)value;
    case ModbusDataType::INT32:
      return (uint32_t)((int32_t)value);
    case ModbusDataType::INT16:
      return (uint32_t)((int16_t)value);
    case ModbusDataType::FLOAT:
      return packF(value);
  }

  return MBUS_VAL_UNAVAILABLE;
}

static const ActiveRecipe* findActiveRecipe(uint16_t reg) {
  for (size_t i = 0; i < activeRecipeCount; i++) {
    if (activeRecipes[i].registerAddress == reg) {
      return &activeRecipes[i];
    }
  }
  return nullptr;
}

static bool loadActiveRecipes(const ActiveRecipe* recipes, size_t recipeCount) {
  activeRecipes = recipes;
  activeRecipeCount = recipeCount;
  activeRecipeMaxReg = 0;

  for (size_t i = 0; i < activeRecipeCount; i++) {
    const ActiveRecipe& recipe = recipes[i];

    const ModbusDataType type = static_cast<ModbusDataType>(recipe.type);
    const uint16_t regSize = (type == ModbusDataType::INT16) ? 1 : 2;
    const uint16_t endReg = recipe.registerAddress + regSize;
    if (endReg > activeRecipeMaxReg) activeRecipeMaxReg = endReg;
  }

  return activeRecipeCount > 0;
}

static bool loadPresetRecipes(int mappingChoice) {
  activeRecipeLswFirst = (mappingChoice == 4 || mappingChoice == kModbusMappingEm24Tcp);

  switch (mappingChoice) {
    case 0:
      return loadActiveRecipes(kDefaultRecipes0, sizeof(kDefaultRecipes0) / sizeof(kDefaultRecipes0[0]));
    case 1:
      return loadActiveRecipes(kSdm630Recipes, sizeof(kSdm630Recipes) / sizeof(kSdm630Recipes[0]));
    case 2:
      return loadActiveRecipes(kDtsu666Recipes, sizeof(kDtsu666Recipes) / sizeof(kDtsu666Recipes[0]));
    case 3:
      return loadActiveRecipes(kAlfenSocomecRecipes, sizeof(kAlfenSocomecRecipes) / sizeof(kAlfenSocomecRecipes[0]));
    case 4:
      return loadActiveRecipes(kEm330Recipes, sizeof(kEm330Recipes) / sizeof(kEm330Recipes[0]));
    case 5:
      return loadActiveRecipes(kAbbB21Recipes, sizeof(kAbbB21Recipes) / sizeof(kAbbB21Recipes[0]));
    case 6:
      return loadActiveRecipes(kMx3xxRecipes, sizeof(kMx3xxRecipes) / sizeof(kMx3xxRecipes[0]));
    case 7:
      return loadActiveRecipes(kDefaultFloatRecipes, sizeof(kDefaultFloatRecipes) / sizeof(kDefaultFloatRecipes[0]));
    case 8:
      return loadActiveRecipes(kKlefrRecipes, sizeof(kKlefrRecipes) / sizeof(kKlefrRecipes[0]));
    case 9:
      return loadActiveRecipes(kPhoenixEemXm3xxRecipes, sizeof(kPhoenixEemXm3xxRecipes) / sizeof(kPhoenixEemXm3xxRecipes[0]));
    case kModbusMappingEm24Tcp:
      // EM24 requests are handled per 16-bit register below. Keep a valid
      // recipe set here so the generic mapping state remains initialized.
      return loadActiveRecipes(kEm24TcpRecipes, sizeof(kEm24TcpRecipes) / sizeof(kEm24TcpRecipes[0]));
    case kModbusMappingFroniusSunSpec203:
      return loadActiveRecipes(kFroniusSunSpec203Recipes, sizeof(kFroniusSunSpec203Recipes) / sizeof(kFroniusSunSpec203Recipes[0]));
    default:
      return loadActiveRecipes(kDefaultRecipes0, sizeof(kDefaultRecipes0) / sizeof(kDefaultRecipes0[0]));
  }
}

static void initMbMapping(int mappingChoice) {
  loadPresetRecipes(mappingChoice);
}

static uint16_t getMbActiveMaxReg() {
  if (!activeRecipes) initMbMapping(SelMap);
  return activeRecipeMaxReg;
}

static bool readMbActiveRegister(uint16_t reg, uint8_t& type, uint32_t& valueU32, int16_t& valueI16) {
  if (!activeRecipes) initMbMapping(SelMap);
  const ActiveRecipe* recipe = findActiveRecipe(reg);
  if (!recipe) return false;

  type = recipe->type;
  const uint32_t encoded = encodeActiveRecipeValue(*recipe);

  if (type == static_cast<uint8_t>(ModbusDataType::INT16)) {
    valueI16 = (int16_t)encoded;
  } else {
    valueU32 = encoded;
  }

  return true;
}

static void addMbU32(ModbusMessage& response, uint32_t value) {
  if (activeRecipeLswFirst) {
    response.add((uint16_t)(value & 0xFFFFu), (uint16_t)(value >> 16));
    return;
  }

  response.add(value);
}


static inline uint32_t packF(float f) { union { float f; uint32_t u; } x{f}; return x.u; }

static bool em24IsThreePhase() {
  if (settingPhases == 1) return false;
  if (settingPhases >= 2) return true;
  return DSMRdata.voltage_l2_present || DSMRdata.voltage_l3_present ||
         DSMRdata.current_l2_present || DSMRdata.current_l3_present ||
         DSMRdata.power_delivered_l2_present || DSMRdata.power_delivered_l3_present ||
         DSMRdata.power_returned_l2_present || DSMRdata.power_returned_l3_present;
}

static int32_t em24ScaledValue(MbSource source, int16_t scale) {
  const float value = readScaledMbSourceValue(source, scale);
  return isnan(value) ? 0 : (int32_t)lroundf(value);
}

static uint32_t em24TotalPower() {
  if (!em24IsThreePhase()) {
    return (uint32_t)em24ScaledValue(MbSource::net_power_total_kw, 10000);
  }

  const int32_t total = em24ScaledValue(MbSource::net_power_l1_kw, 10000) +
                        em24ScaledValue(MbSource::net_power_l2_kw, 10000) +
                        em24ScaledValue(MbSource::net_power_l3_kw, 10000);
  return (uint32_t)total;
}

static uint16_t em24Word(uint32_t value, uint16_t base, uint16_t address) {
  return address == base ? (uint16_t)(value & 0xFFFFu) : (uint16_t)(value >> 16);
}

static uint16_t readEm24Register(uint16_t address) {
  const bool threePhase = em24IsThreePhase();

  if (address <= 0x0005) {
    const uint16_t base = address & 0xFFFEu;
    const uint8_t phase = base / 2;
    if (!threePhase && phase > 0) return 0;
    const MbSource source = phase == 0 ? MbSource::voltage_l1_v
                           : phase == 1 ? MbSource::voltage_l2_v
                                        : MbSource::voltage_l3_v;
    return em24Word((uint32_t)em24ScaledValue(source, 10), base, address);
  }

  if (address == 0x000B) return 1648;

  if (address >= 0x000C && address <= 0x0011) {
    const uint16_t base = 0x000C + ((address - 0x000C) / 2) * 2;
    const uint8_t phase = (base - 0x000C) / 2;
    if (!threePhase && phase > 0) return 0;
    const MbSource source = phase == 0 ? MbSource::signed_current_l1_a
                           : phase == 1 ? MbSource::signed_current_l2_a
                                        : MbSource::signed_current_l3_a;
    return em24Word((uint32_t)em24ScaledValue(source, 1000), base, address);
  }

  if (address >= 0x0012 && address <= 0x0017) {
    const uint16_t base = 0x0012 + ((address - 0x0012) / 2) * 2;
    const uint8_t phase = (base - 0x0012) / 2;
    if (!threePhase && phase > 0) return 0;
    const MbSource source = phase == 0 ? (threePhase ? MbSource::net_power_l1_kw : MbSource::net_power_total_kw)
                           : phase == 1 ? MbSource::net_power_l2_kw
                                        : MbSource::net_power_l3_kw;
    return em24Word((uint32_t)em24ScaledValue(source, 10000), base, address);
  }

  if (address >= 0x0028 && address <= 0x0029) {
    return em24Word(em24TotalPower(), 0x0028, address);
  }
  if (address == 0x0033) return 500;
  if (address >= 0x0034 && address <= 0x0035) {
    return em24Word((uint32_t)em24ScaledValue(MbSource::energy_delivered_total_kwh, 10), 0x0034, address);
  }
  if (address >= 0x0040 && address <= 0x0041) {
    return em24Word((uint32_t)em24ScaledValue(MbSource::energy_delivered_total_kwh, 10), 0x0040, address);
  }
  if (address >= 0x0046 && address <= 0x0047) {
    return em24Word((uint32_t)em24ScaledValue(MbSource::energy_returned_total_kwh, 10), 0x0046, address);
  }
  if (address >= 0x004E && address <= 0x004F) {
    return em24Word((uint32_t)em24ScaledValue(MbSource::energy_returned_total_kwh, 10), 0x004E, address);
  }
  if (address == 0x0302 || address == 0x0304) return 0x0100;
  if (address == 0x1002) return threePhase ? 0 : 3;

  if (address >= 0x5000 && address <= 0x5006) {
    char serial[15];
    snprintf(serial, sizeof(serial), "P1%012llX", (unsigned long long)(_getChipId() & 0xFFFFFFFFFFFFULL));
    const uint8_t offset = (address - 0x5000) * 2;
    return ((uint16_t)(uint8_t)serial[offset] << 8) | (uint8_t)serial[offset + 1];
  }

  if (address == 0xA000) return 7;
  if (address == 0xA100) return 3;
  return 0;
}

static bool em24DataAvailable() {
  if (last_telegram_t == 0) return false;

  const uint32_t timeoutMs = bV5meter ? 3500UL : 35000UL;
  return (uint32_t)(millis() - (uint32_t)last_telegram_t) <= timeoutMs;
}

static ModbusMessage handleEm24Read(ModbusMessage request, uint8_t transport, uint16_t address, uint16_t words) {
  ModbusMessage response;
  if (words == 0 || (uint32_t)address + words > 0xA101UL) {
    response.setError(request.getServerID(), request.getFunctionCode(), ILLEGAL_DATA_ADDRESS);
    logModbusMonitorRequest(transport, request, address, words, ILLEGAL_DATA_ADDRESS);
    return response;
  }

  if (!em24DataAvailable()) {
    response.setError(request.getServerID(), request.getFunctionCode(), SERVER_DEVICE_BUSY);
    logModbusMonitorRequest(transport, request, address, words, SERVER_DEVICE_BUSY);
    return response;
  }

  response.add(request.getServerID(), request.getFunctionCode(), (uint8_t)(words * 2));
  for (uint16_t i = 0; i < words; i++) response.add(readEm24Register(address + i));
  logModbusMonitorRequest(transport, request, address, words, 0);
  return response;
}

// Change active mapping
void setModbusMapping(int mappingChoice) {
    SelMap = mappingChoice;
    initMbMapping(mappingChoice);
}

static ModbusMessage MBusHandleRequestInternal(ModbusMessage request, uint8_t transport) {
    uint16_t address;
    uint16_t words;
    digitalWrite(statusled, LOW);

    ModbusMessage response;
    request.get(2, address);
    request.get(4, words);
    
    Debugf("addr [0x%X] words [0x%02X] DevID [0x%02X] FC [0x%02X]\n", address, words, request.getServerID(), request.getFunctionCode());
#ifdef DEBUG
    String actualJson = smActualJsonDebug();
    Debugf("--MODBUS actual snapshot: %s\n", actualJson.c_str());
#endif

    if (SelMap == kModbusMappingEm24Tcp) {
      return handleEm24Read(request, transport, address, words);
    }

    uint16_t maxReg = getMbActiveMaxReg();

    if ( (address + words) > maxReg ){
        response.setError(request.getServerID(), request.getFunctionCode(), ILLEGAL_DATA_ADDRESS);
        logModbusMonitorRequest(transport, request, address, words, ILLEGAL_DATA_ADDRESS);
        return response;
    }

    response.add(request.getServerID(), request.getFunctionCode(), (uint8_t)(words * 2));

    uint16_t currentAddr = address;
    union uMbusData {
      float    f;
      uint32_t u;
      int32_t  i;
      uint8_t  b[4];
      int16_t  w;
    } val;

    while (currentAddr < (address + words)) {
        
        uint8_t typeId = static_cast<uint8_t>(
          SelMap == kModbusMappingFroniusSunSpec203 ? ModbusDataType::INT16 : ModbusDataType::UINT32
        );
        bool hasValue = readMbActiveRegister(currentAddr, typeId, val.u, val.w);
        ModbusDataType type = static_cast<ModbusDataType>(typeId);

        if (!hasValue) {
            Debugf("MBUS WRONG VALUE -- addr: %d\n", currentAddr);
            val.u = MBUS_VAL_UNAVAILABLE;
        }

#ifdef DEBUG
        Debugf("\n--MODBUS uint32 : %u\n", val.u);
        Debugf("--MODBUS int32  : %i\n", val.i);
        Debugf("--MODBUS HEX    : %08X\n", val.u);
        Debugf("--MODBUS FLOAT  : %f\n",   val.f);
        Debugf("--MODBUS Bytes  : [%d] [%d]\n", val.b[1], val.b[0]);
#endif
        switch (type) {
            case ModbusDataType::FLOAT: 
            case ModbusDataType::UINT32:
            case ModbusDataType::INT32: {                
                addMbU32(response, val.u);
                currentAddr += 2;
                break;
            }
            case ModbusDataType::INT16: {
                response.add((int16_t)(val.w));
                currentAddr += 1;
                break;
            }
        }
    }

    digitalWrite(statusled, HIGH);
    logModbusMonitorRequest(transport, request, address, words, 0);
    return response;
}

ModbusMessage MBusHandleRequestTCP(ModbusMessage request) {
  return MBusHandleRequestInternal(request, kModbusTransportTcp);
}

ModbusMessage MBusHandleRequestRTU(ModbusMessage request) {
  return MBusHandleRequestInternal(request, kModbusTransportRtu);
}

void mbusSetup(){
  if (skipNetwork) return;
  MBserver.registerWorker(mb_config.id, READ_HOLD_REGISTER, &MBusHandleRequestTCP);//FC03
  MBserver.registerWorker(mb_config.id, READ_INPUT_REGISTER, &MBusHandleRequestTCP);//FC04
  MBserver.start(mb_config.port, MBUS_CLIENTS, MBUS_TIMEOUT);
}

#endif //MBUS

#ifdef MB_RTU

#define MBUS_RTU_TIMEOUT      2000

#include "ModbusServerRTU.h"
#include "DSMRloggerAPI.h"

ModbusServerRTU* MBserverRTU;

//used by the D1MC : switchable 120 Ohm termination
void MBSetTermination (bool value){
  pinMode(3, OUTPUT);
  digitalWrite(3,value);
}

void SetupMB_RTU(){

  // Debugf("mb_rx : %d \n",mb_rx);
  // Debugf("mb_tx : %d \n",mb_tx);
  // Debugf("mb_rts: %d \n",mb_rts);

  if ( mb_rx == -1 ) {
    DebugTln("Setup Modbus RTU TERMINATED");
    return;
  }
  DebugTln("Setup Modbus RTU");
  RTU_SERIAL.end();
  RTUutils::prepareHardwareSerial(RTU_SERIAL);
  RTU_SERIAL.begin( mb_config.baud , mb_config.parity , mb_rx, mb_tx );

  MBserverRTU = new ModbusServerRTU(MBUS_RTU_TIMEOUT, mb_rts);
  MBserverRTU->registerWorker(mb_config.id , READ_HOLD_REGISTER, &MBusHandleRequestRTU);//FC03
  MBserverRTU->registerWorker(mb_config.id, READ_INPUT_REGISTER, &MBusHandleRequestRTU);//FC04
  MBserverRTU->begin(RTU_SERIAL);
}
#else
void SetupMB_RTU(){}
void MBSetTermination (bool value){}
#endif

void updateModbusServerId(uint8_t oldId, uint8_t newId) {
  if (oldId == newId) return;

#ifdef MBUS
  if (!skipNetwork) {
    MBserver.stop();
    MBserver.unregisterWorker(oldId);
    MBserver.registerWorker(newId, READ_HOLD_REGISTER, &MBusHandleRequestTCP);
    MBserver.registerWorker(newId, READ_INPUT_REGISTER, &MBusHandleRequestTCP);
    MBserver.start(mb_config.port, MBUS_CLIENTS, MBUS_TIMEOUT);
  }
#endif

#ifdef MB_RTU
  if (MBserverRTU != nullptr) {
    MBserverRTU->end();
    MBserverRTU->unregisterWorker(oldId);
    MBserverRTU->registerWorker(newId, READ_HOLD_REGISTER, &MBusHandleRequestRTU);
    MBserverRTU->registerWorker(newId, READ_INPUT_REGISTER, &MBusHandleRequestRTU);
    MBserverRTU->begin(RTU_SERIAL);
  }
#endif

  DebugTf("Modbus ID changed at runtime: %u -> %u\r\n", oldId, newId);
}
