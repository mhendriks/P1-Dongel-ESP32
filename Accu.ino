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

void SendAccuJson() {
  if ( !SolarEdgeAccu.Available ) {
    httpServer.send(200, "application/json", "{\"active\":false}");
    return;
  }

  String Json = "{\"active\":true,\"status\":\"";
  Json += String(SolarEdgeAccu.status);
  Json += "\",\"unit\":\"";
  Json += String(SolarEdgeAccu.unit);
  Json += "\", \"currentPower\":";
  Json += String( SolarEdgeAccu.currentPower );
  Json += ", \"chargeLevel\":";
  Json += String( SolarEdgeAccu.chargeLevel );
  Json += "}";

#ifdef DEBUG
  DebugTln("SendAccuJson");
  DebugT("Accu Json: ");Debugln(Json);
#endif

  httpServer.send(200, "application/json", Json.c_str() );
}
