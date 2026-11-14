// One setting list keeps persistence, the API schema and updates in sync.
#define CONNECTOR_FIELD(B,I,F,S,prefix,field) \
  I(prefix "_register", field.reg, 0, 65535) \
  I(prefix "_type", field.type, CONNECTOR_U16, CONNECTOR_F32) \
  F(prefix "_scale", field.scale, -100000, 100000) \
  B(prefix "_swap", field.swap)
#define CONNECTOR_SETTINGS(B,I,F,S) \
  I("victron_accu_port", victronModbusConfig.port, 1, 65535) \
  I("victron_accu_interval", victronModbusConfig.interval, 1, 3600) \
  CONNECTOR_FIELD(B,I,F,S,"battery_power",victronModbusConfig.power) \
  CONNECTOR_FIELD(B,I,F,S,"battery_soc",victronModbusConfig.soc) \
  CONNECTOR_FIELD(B,I,F,S,"battery_state",victronModbusConfig.state) \
  I("battery_idle_code", victronModbusConfig.idle, -32768, 32767) \
  I("battery_charging_code", victronModbusConfig.charging, -32768, 32767) \
  I("battery_discharging_code", victronModbusConfig.discharging, -32768, 32767) \
  B("pv_modbus_enabled", pvModbusConfig.enabled) \
  S("pv_modbus_ip", pvModbusConfig.ip) \
  I("pv_modbus_port", pvModbusConfig.port, 1, 65535) \
  I("pv_modbus_id", pvModbusConfig.id, 1, 247) \
  I("pv_modbus_interval", pvModbusConfig.interval, 1, 3600) \
  I("pv_modbus_wp", pvModbusConfig.wp, 0, 10000000) \
  CONNECTOR_FIELD(B,I,F,S,"pv_power",pvModbusConfig.power) \
  CONNECTOR_FIELD(B,I,F,S,"pv_energy",pvModbusConfig.energy) \
  I("pv_power_sf", pvModbusConfig.power.sf, 0, 65535) \
  I("pv_energy_sf", pvModbusConfig.energy.sf, 0, 65535)

void writeConnectorSettings(JsonDocument& doc) {
#define BOOL(k,v) doc[k] = v;
#define INT(k,v,lo,hi) doc[k] = v;
#define FLOAT(k,v,lo,hi) doc[k] = v;
#define TEXT(k,v) doc[k] = v;
  CONNECTOR_SETTINGS(BOOL,INT,FLOAT,TEXT)
#undef BOOL
#undef INT
#undef FLOAT
#undef TEXT
}
void readConnectorSettings(const JsonDocument& doc) {
#define BOOL(k,v) if (doc[k].is<bool>()) v = doc[k].as<bool>();
#define INT(k,v,lo,hi) if (doc[k].is<long>()) v = constrain(doc[k].as<long>(), long(lo), long(hi));
#define FLOAT(k,v,lo,hi) if (doc[k].is<float>() && isfinite(doc[k].as<float>())) v = constrain(doc[k].as<float>(), float(lo), float(hi));
#define TEXT(k,v) if (doc[k].is<const char*>()) strlcpy(v, doc[k].as<const char*>(), sizeof(v));
  CONNECTOR_SETTINGS(BOOL,INT,FLOAT,TEXT)
#undef BOOL
#undef INT
#undef FLOAT
#undef TEXT
}
void fillConnectorSettingsJson(JsonDocument& doc) {
#define BOOL(k,v) doc[k] = v;
#define INT(k,v,lo,hi) { JsonObject o = doc[k].to<JsonObject>(); o["type"] = "i"; o["min"] = lo; o["max"] = hi; o["value"] = v; }
#define FLOAT(k,v,lo,hi) { JsonObject o = doc[k].to<JsonObject>(); o["type"] = "f"; o["min"] = lo; o["max"] = hi; o["value"] = v; }
#define TEXT(k,v) { JsonObject o = doc[k].to<JsonObject>(); o["type"] = "s"; o["min"] = 0; o["max"] = sizeof(v)-1; o["value"] = v; }
  CONNECTOR_SETTINGS(BOOL,INT,FLOAT,TEXT)
#undef BOOL
#undef INT
#undef FLOAT
#undef TEXT
}
bool updateConnectorSetting(const char* field, const char* value) {
  bool changed = false;
  const String text(value);
#define BOOL(k,v) if (!stricmp(field,k)) { v = text.equalsIgnoreCase("true"); changed = true; }
#define INT(k,v,lo,hi) if (!stricmp(field,k)) { v = constrain(text.toInt(), long(lo), long(hi)); changed = true; }
#define FLOAT(k,v,lo,hi) if (!stricmp(field,k) && isfinite(text.toFloat())) { v = constrain(text.toFloat(), float(lo), float(hi)); changed = true; }
#define TEXT(k,v) if (!stricmp(field,k)) { strlcpy(v, value, sizeof(v)); changed = true; }
  CONNECTOR_SETTINGS(BOOL,INT,FLOAT,TEXT)
#undef BOOL
#undef INT
#undef FLOAT
#undef TEXT
  if (changed) {
    if (!strncmp(field, "pv_", 3)) pvModbusConfigChanged();
    else victronModbusConfigChanged();
  }
  return changed;
}
#undef CONNECTOR_SETTINGS
#undef CONNECTOR_FIELD
