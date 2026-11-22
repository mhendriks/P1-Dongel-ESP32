AccuPwrSystems VictronAccu = {
  false, "kW", "", 0.0, 0
};

static uint32_t victronAccuLastUpdate = 0;
static const uint32_t VICTRON_ACCU_STALE_MS = 20000;

static bool victronAccuAvailable() {
  return VictronAccu.Available && (millis() - victronAccuLastUpdate <= max(VICTRON_ACCU_STALE_MS, uint32_t(victronModbusConfig.interval) * 2000UL));
}

AccuPwrSystems* dashboardAccu() {
  if (victronAccuAvailable()) return &VictronAccu;
  return nullptr;
}

void updateVictronAccu(float powerW, uint16_t chargeLevel, uint16_t state) {
  VictronAccu.currentPower = powerW / 1000.0f;
  VictronAccu.chargeLevel = (uint8_t)constrain((int)chargeLevel, 0, 100);
  if (state == uint16_t(victronModbusConfig.charging)) VictronAccu.status = "Charging";
  else if (state == uint16_t(victronModbusConfig.discharging)) VictronAccu.status = "Discharging";
  else if (state == uint16_t(victronModbusConfig.idle)) VictronAccu.status = "Idle";
  else VictronAccu.status = "Unknown";
  VictronAccu.Available = true;
  victronAccuLastUpdate = millis();
  apiWsMarkLiveDirty();
}

void invalidateVictronAccu() {
  if (!VictronAccu.Available) return;
  VictronAccu.Available = false;
  apiWsMarkLiveDirty();
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
