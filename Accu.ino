struct AccuPwrSystems {
  bool      Available;
  String    unit;
  String    status;
  float     currentPower;
  uint8_t   chargeLevel;
};

AccuPwrSystems SolarEdgeAccu = {
  false, "", "", 0.0, 0
};

float SolarEdgeFlowPvPower = 0.0f;
bool  SolarEdgeFlowPvValid = false;

ApiResponse accuApiResponse() {
  if ( !SolarEdgeAccu.Available ) {
    return {200, "application/json", "{\"active\":false}"};
  }

  JsonDocument doc;
  doc["active"] = true;
  doc["status"] = SolarEdgeAccu.status;
  doc["unit"] = SolarEdgeAccu.unit;
  doc["currentPower"] = SolarEdgeAccu.currentPower;
  doc["chargeLevel"] = SolarEdgeAccu.chargeLevel;

  String body;
  serializeJson(doc, body);

#ifdef DEBUG
  DebugTln("SendAccuJson");
  DebugT("Accu Json: ");Debugln(body);
#endif

  return {200, "application/json", body};
}
