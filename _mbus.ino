#ifdef MBUS

#define MBUS_DEV_ID       1
#define MBUS_CLIENTS      4
#define MBUS_TIMEOUT  10000
#define MBUS_VAL_UNAVAILABLE 0xFFFFFFFF

enum class ModbusDataType : uint8_t;
enum class MbSource : uint8_t;

float calculateLineVoltage(float V1, float V2) {
  return sqrt(3) * (V1 + V2) / 2.0;
}

// Set up a Modbus server
ModbusServerWiFi MBserver;

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

union {
  float    f;
  uint32_t u;
  int32_t  i;
} map_temp;



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

#include "_mbus_mapping.h"

static float mbSignedCurrent(float current, float returned);
static float readMbSourceValue(MbSource source);
static float readScaledMbSourceValue(MbSource source, int16_t scale);
static bool readMbSourceUInt32(MbSource source, int16_t scale, uint32_t value, uint32_t& outValue);
static uint32_t encodeActiveRecipeValue(const ActiveRecipe& recipe);
static const ActiveRecipe* findActiveRecipe(uint16_t reg);
static bool loadActiveRecipes(const ActiveRecipe* recipes, size_t recipeCount);
static bool loadPresetRecipes(int mappingChoice);

static float mbSignedCurrent(float current, float returned) {
  return current * (returned ? -1.0f : 1.0f);
}

static float readMbSourceValue(MbSource source) {
  switch (source) {
    case MbSource::timestamp_epoch:
      return (float)(actT - (actTimestamp[12] == 'S' ? 7200 : 3600));
    case MbSource::energy_delivered_tariff1_kwh:
      return (DSMRdata.energy_delivered_tariff1_present && !bUseEtotals) ? DSMRdata.energy_delivered_tariff1.val() : NAN;
    case MbSource::energy_delivered_tariff2_kwh:
      return (DSMRdata.energy_delivered_tariff2_present && !bUseEtotals) ? DSMRdata.energy_delivered_tariff2.val() : NAN;
    case MbSource::energy_returned_tariff1_kwh:
      return (DSMRdata.energy_returned_tariff1_present && !bUseEtotals) ? DSMRdata.energy_returned_tariff1.val() : NAN;
    case MbSource::energy_returned_tariff2_kwh:
      return (DSMRdata.energy_returned_tariff2_present && !bUseEtotals) ? DSMRdata.energy_returned_tariff2.val() : NAN;
    case MbSource::energy_delivered_total_kwh:
      return DSMRdata.energy_delivered_total_present
        ? DSMRdata.energy_delivered_total.val()
        : ((DSMRdata.energy_delivered_tariff1_present ? DSMRdata.energy_delivered_tariff1.val() : 0.0f) +
           (DSMRdata.energy_delivered_tariff2_present ? DSMRdata.energy_delivered_tariff2.val() : 0.0f));
    case MbSource::energy_returned_total_kwh:
      return DSMRdata.energy_returned_total_present
        ? DSMRdata.energy_returned_total.val()
        : ((DSMRdata.energy_returned_tariff1_present ? DSMRdata.energy_returned_tariff1.val() : 0.0f) +
           (DSMRdata.energy_returned_tariff2_present ? DSMRdata.energy_returned_tariff2.val() : 0.0f));
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
          ? (float)(
              (DSMRdata.energy_delivered_tariff1_present ? DSMRdata.energy_delivered_tariff1.val() : 0.0f) -
              (DSMRdata.energy_returned_tariff1_present ? DSMRdata.energy_returned_tariff1.val() : 0.0f) +
              (DSMRdata.energy_delivered_tariff2_present ? DSMRdata.energy_delivered_tariff2.val() : 0.0f) -
              (DSMRdata.energy_returned_tariff2_present ? DSMRdata.energy_returned_tariff2.val() : 0.0f)
            )
          : NAN;
    case MbSource::energy_net_tariff1_kwh:
      return
        (DSMRdata.energy_delivered_tariff1_present || DSMRdata.energy_returned_tariff1_present)
          ? (float)(
              (DSMRdata.energy_delivered_tariff1_present ? DSMRdata.energy_delivered_tariff1.val() : 0.0f) -
              (DSMRdata.energy_returned_tariff1_present ? DSMRdata.energy_returned_tariff1.val() : 0.0f)
            )
          : NAN;
    case MbSource::energy_net_tariff2_kwh:
      return
        (DSMRdata.energy_delivered_tariff2_present || DSMRdata.energy_returned_tariff2_present)
          ? (float)(
              (DSMRdata.energy_delivered_tariff2_present ? DSMRdata.energy_delivered_tariff2.val() : 0.0f) -
              (DSMRdata.energy_returned_tariff2_present ? DSMRdata.energy_returned_tariff2.val() : 0.0f)
            )
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
      return DSMRdata.power_delivered_present ? DSMRdata.power_delivered.val() : NAN;
    case MbSource::power_returned_kw:
      return DSMRdata.power_returned_present ? DSMRdata.power_returned.val() : NAN;
    case MbSource::net_power_total_kw:
      return DSMRdata.power_delivered_present ? (DSMRdata.power_delivered.val() - DSMRdata.power_returned.val()) : NAN;
    case MbSource::voltage_l1_v:
      return DSMRdata.voltage_l1_present ? (float)DSMRdata.voltage_l1.val() : NAN;
    case MbSource::voltage_l2_v:
      return DSMRdata.voltage_l2_present ? (float)DSMRdata.voltage_l2.val() : NAN;
    case MbSource::voltage_l3_v:
      return DSMRdata.voltage_l3_present ? (float)DSMRdata.voltage_l3.val() : NAN;
    case MbSource::current_l1_a:
      return DSMRdata.current_l1_present ? (float)DSMRdata.current_l1.val() : NAN;
    case MbSource::current_l2_a:
      return DSMRdata.current_l2_present ? (float)DSMRdata.current_l2.val() : NAN;
    case MbSource::current_l3_a:
      return DSMRdata.current_l3_present ? (float)DSMRdata.current_l3.val() : NAN;
    case MbSource::current_total_a:
      return DSMRdata.current_l1_present
        ? (float)(DSMRdata.current_l1.val() + DSMRdata.current_l2.val() + DSMRdata.current_l3.val())
        : NAN;
    case MbSource::signed_current_l1_a:
      return DSMRdata.current_l1_present ? mbSignedCurrent((float)DSMRdata.current_l1.val(), DSMRdata.power_returned_l1.val()) : NAN;
    case MbSource::signed_current_l2_a:
      return DSMRdata.current_l2_present ? mbSignedCurrent((float)DSMRdata.current_l2.val(), DSMRdata.power_returned_l2.val()) : NAN;
    case MbSource::signed_current_l3_a:
      return DSMRdata.current_l3_present ? mbSignedCurrent((float)DSMRdata.current_l3.val(), DSMRdata.power_returned_l3.val()) : NAN;
    case MbSource::gas_timestamp_epoch:
      return mbusGas ? (float)(epoch(gasDeliveredTimestamp.c_str(), 10, false) - (actTimestamp[12] == 'S' ? 7200 : 3600)) : NAN;
    case MbSource::gas_delivered_m3:
      return mbusGas ? (float)gasDelivered : NAN;
    case MbSource::electricity_tariff:
      return DSMRdata.electricity_tariff_present ? (float)atoi(DSMRdata.electricity_tariff.c_str()) : NAN;
    case MbSource::peak_pwr_last_q_kw:
      return DSMRdata.peak_pwr_last_q_present ? (float)DSMRdata.peak_pwr_last_q.val() : NAN;
    case MbSource::net_power_l1_kw:
      return DSMRdata.power_delivered_l1_present ? (float)(DSMRdata.power_delivered_l1.val() - DSMRdata.power_returned_l1.val()) : NAN;
    case MbSource::net_power_l2_kw:
      return DSMRdata.power_delivered_l2_present ? (float)(DSMRdata.power_delivered_l2.val() - DSMRdata.power_returned_l2.val()) : NAN;
    case MbSource::net_power_l3_kw:
      return DSMRdata.power_delivered_l3_present ? (float)(DSMRdata.power_delivered_l3.val() - DSMRdata.power_returned_l3.val()) : NAN;
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
      return (DSMRdata.voltage_l1_present && DSMRdata.voltage_l2_present) ? calculateLineVoltage(DSMRdata.voltage_l1, DSMRdata.voltage_l2) : NAN;
    case MbSource::line_voltage_l23_v:
      return (DSMRdata.voltage_l2_present && DSMRdata.voltage_l3_present) ? calculateLineVoltage(DSMRdata.voltage_l2, DSMRdata.voltage_l3) : NAN;
    case MbSource::line_voltage_l31_v:
      return (DSMRdata.voltage_l3_present && DSMRdata.voltage_l1_present) ? calculateLineVoltage(DSMRdata.voltage_l3, DSMRdata.voltage_l1) : NAN;
    case MbSource::line_voltage_avg_v: {
      float value = 0.0f;
      value += (DSMRdata.voltage_l1_present && DSMRdata.voltage_l2_present) ? calculateLineVoltage(DSMRdata.voltage_l1, DSMRdata.voltage_l2) : 0.0f;
      value += (DSMRdata.voltage_l2_present && DSMRdata.voltage_l3_present) ? calculateLineVoltage(DSMRdata.voltage_l2, DSMRdata.voltage_l3) : 0.0f;
      value += (DSMRdata.voltage_l3_present && DSMRdata.voltage_l1_present) ? calculateLineVoltage(DSMRdata.voltage_l3, DSMRdata.voltage_l1) : 0.0f;
      return value / 3.0f;
    }
    case MbSource::water_delivered_m3:
      return WtrMtr ? (float)(P1Status.wtr_m3 + (P1Status.wtr_l / 1000.0f)) : NAN;
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
    case MbSource::voltage_l1_v:
      return DSMRdata.voltage_l1_present ? sign * (float)DSMRdata.voltage_l1.int_val() : NAN;
    case MbSource::voltage_l2_v:
      return DSMRdata.voltage_l2_present ? sign * (float)DSMRdata.voltage_l2.int_val() : NAN;
    case MbSource::voltage_l3_v:
      return DSMRdata.voltage_l3_present ? sign * (float)DSMRdata.voltage_l3.int_val() : NAN;
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
      return WtrMtr ? sign * (float)(P1Status.wtr_m3 * 1000 + P1Status.wtr_l) : NAN;
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


static inline uint32_t packF(float f) { union { float f; uint32_t u; } x{f}; return x.u; }

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
        
        uint8_t typeId = static_cast<uint8_t>(ModbusDataType::UINT32);
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
                response.add(val.u);
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
