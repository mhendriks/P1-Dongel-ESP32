struct SolarPwrSystems {
  bool      Available;
  String    Url;
  String    Token;
  uint64_t  TokenExpire;
  uint32_t  SiteID;
  uint32_t  Wp;
  uint32_t  Actual;
  uint32_t  Daily;
  uint32_t  Interval;
  time_t    LastRefresh;
  char    file_name[20];
};

// SolarPwrSystems Enphase   = { false, "https://envoy/api/v1/production", "", 0, 0, 0, 0, 0,  60, 0, "/enphase.json"  }, SolarEdge = { false, "", "", 0, 0, 0, 0, 0, 300, 0, "/solaredge.json"  };
SolarPwrSystems Enphase   = { false, "https://envoy/ivp/pdm/energy", "", 0, 0, 0, 0, 0,  60, 0, "/enphase.json"  };
SolarPwrSystems SolarEdge = { false, "", "", 0, 0, 0, 0, 0, 300, 0, "/solaredge.json"  };
SolarPwrSystems SMAinv    = { false, "http://192.168.1.231",      "", 0, 0, 0, 0, 0,  15, 0, "/sma.json" };

static String   _sma_sid;
static uint32_t _sma_sid_t0 = 0;

static bool smaHttpPOST(const String& url, const String& body, String& out) {
  out = "";
  HTTPClient http;
  WiFiClient *client = nullptr;
  WiFiClientSecure *clientTLS = nullptr;

  bool https = url.startsWith("https://");
  if (https) { clientTLS = new WiFiClientSecure(); clientTLS->setInsecure(); http.begin(*clientTLS, url); }
  else       { client   = new WiFiClient();        http.begin(*client,   url); }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Connection", "close");
  http.setTimeout(5000);
  int rc = http.POST(body);
  if (rc == 200) out = http.getString();
  http.end();
  if (clientTLS) delete clientTLS;
  if (client)    delete client;
  return (rc == 200 && out.length() > 0);
}

static bool smaLogin(const String& baseUrl, const String& password, const char* right = "usr") {
  String resp;
  String url  = baseUrl + "/dyn/login.json";
  String body = String("{\"pass\":\"") + password + "\",\"right\":\"" + right + "\"}";
  if (!smaHttpPOST(url, body, resp)) { _sma_sid = ""; DebugTln("Error smaHttpPOST"); return false; }
  DebugT("sma POST resp: ");Debugln(resp);
  JsonDocument doc;
  if (deserializeJson(doc, resp))     { _sma_sid = ""; DebugTln("Error sma deserializeJson error"); return false; }
  String sid = doc["result"]["sid"].as<String>();
  if (sid.isEmpty())                  { _sma_sid = ""; DebugTln("Error SMA sid empty"); return false; }
  _sma_sid = sid; _sma_sid_t0 = millis();
  return true;
}

static bool smaGetPacAndDay(const String& baseUrl, long& pac_W, long& day_Wh) {
  if (_sma_sid.isEmpty() || (millis() - _sma_sid_t0) > 12UL*60UL*1000UL) {
    if (!smaLogin(baseUrl, SMAinv.Token)) return false; // Token == password (bewust)
  }
  String resp;
  String url  = baseUrl + "/dyn/getValues.json?sid=" + _sma_sid;
  String body = "{\"destDev\":[],\"keys\":[\"6100_40263F00\",\"6400_00262200\"]}";
  if (!smaHttpPOST(url, body, resp)) { DebugTln("Error smaHttpPOST values"); return false; }
  DebugT("sma POST resp: ");Debugln(resp);
  JsonDocument doc; // ~8k is genoeg voor deze response
  if (deserializeJson(doc, resp)) return false;

  // eerste device
  JsonObject result = doc["result"];
  if (result.isNull()) return false;
  auto it = result.begin(); if (it == result.end()) return false;
  JsonObject dev = it->value();

  JsonVariant pac = dev["6100_40263F00"]["1"][0]["val"];
  JsonVariant day = dev["6400_00262200"]["1"][0]["val"];
  if (pac.isNull() || day.isNull()) return false;

  pac_W  = (long)pac.as<long>();
  day_Wh = (long)day.as<long>();
  return true;
}

void ReadSolarConfig(SolarSource src) {
  SolarPwrSystems* solarSystem =
      (src == ENPHASE)   ? &Enphase
    : (src == SOLAR_EDGE)? &SolarEdge
    :                      &SMAinv;

  if (!FSmounted || !LittleFS.exists(solarSystem->file_name)) return;
  DebugT("ReadSolarConfig: "); Debugln(solarSystem->file_name);

  JsonDocument doc;
  File f = LittleFS.open(solarSystem->file_name, "r");
  if (!f) { DebugTln("Error reading SolarFile"); return; }
  if (deserializeJson(doc, f)) { Debugln("Error deserialize SolarFile"); f.close(); return; }
  f.close();

  solarSystem->Available = true;
  solarSystem->Url   = doc["gateway-url"].as<String>();
  solarSystem->Token = doc["token"].as<String>();    // SMA: password hier
  solarSystem->Wp    = doc["wp"].as<uint32_t>();
  solarSystem->SiteID= doc["siteid"].as<uint32_t>(); // SMA: onbenut
  uint32_t ri        = doc["refresh-interval"].as<uint32_t>();
  if (ri > (src==ENPHASE?60:(src==SOLAR_EDGE?300:15))) solarSystem->Interval = ri;

#ifdef DEBUG
  Debug("url > "); Debugln(solarSystem->Url);
  Debug("token > "); Debugln(solarSystem->Token);
  Debug("wp > "); Debugln(solarSystem->Wp);
  Debug("interval > "); Debugln(solarSystem->Interval);
  Debug("siteid > "); Debugln(solarSystem->SiteID);
#endif

  GetSolarData(src, true);
}

void ReadSolarConfigs() {
  if ( skipNetwork ) return;
  ReadSolarConfig(ENPHASE);
  ReadSolarConfig(SOLAR_EDGE);
  ReadSolarConfig(SMA);
}

void GetSolarData(SolarSource src, bool forceUpdate) {
  SolarPwrSystems* solarSystem =
      (src == ENPHASE)    ? &Enphase
    : (src == SOLAR_EDGE) ? &SolarEdge
    :                       &SMAinv;

  if (!solarSystem->Available) return;
  if (!forceUpdate && ((uptime() - solarSystem->LastRefresh) < solarSystem->Interval)) return;
  solarSystem->LastRefresh = uptime();

  // ===== SMA route (POST login + POST getValues) =====
  if (src == SMA) {
    long pac, day;
    if (smaGetPacAndDay(solarSystem->Url, pac, day)) {
      solarSystem->Actual = (uint32_t)pac;
      solarSystem->Daily  = (uint32_t)day;
#ifdef DEBUG
      Debug("SMA Daily > ");  Debugln(solarSystem->Daily);
      Debug("SMA Actual > "); Debugln(solarSystem->Actual);
#endif
    }
    return;
  }

  // ===== Enphase / SolarEdge / Solis (bestaande GET flow) =====
  HTTPClient http;

  if (src == SOLAR_EDGE) {
    solarSystem->Url = "https://monitoringapi.solaredge.com/site/" + String(SolarEdge.SiteID) + "/overview";
    Debug("url > "); Debugln(solarSystem->Url);
  }

  String urlcheck = solarSystem->Url;
  bool bSolis = (urlcheck.indexOf("CMD=inv_query") > 0);
  http.begin(solarSystem->Url.c_str());
  http.addHeader("Accept", "application/json");

  if (src == ENPHASE) {
    if (!bSolis) http.addHeader("Authorization", "Bearer " + solarSystem->Token);
  } else {
    http.addHeader("X-API-Key", solarSystem->Token);
  }

  int httpResponseCode = http.GET();
  DebugT(F("HTTP Response code: ")); Debugln(httpResponseCode);
  if (httpResponseCode <= 0) { http.end(); return; }

  String payload = http.getString();
  #ifdef DEBUG
  if (payload.length() > 256) {
    Debugln(String("payload[256]: ") + payload.substring(0,256) + "...");
  } else {
    Debugln(payload);
  }
  #endif
  http.end();

  JsonDocument solarDoc;
  if (deserializeJson(solarDoc, payload)) { Debugln("Error deserialisation solarDoc"); return; }

  if (bSolis) {
    solarSystem->Actual = solarDoc["i_pow_n"].as<uint32_t>();
    solarSystem->Daily  = (uint32_t)((float)solarDoc["i_eday"] * 1000.0f);
  } else {
    if (solarSystem->Url.endsWith("energy")) { // new api
      solarSystem->Actual = (src == ENPHASE) ? solarDoc["production"]["pcu"]["wattsNow"].as<uint32_t>()
                                             : solarDoc["overview"]["currentPower"]["power"].as<uint32_t>();
      solarSystem->Daily  = (src == ENPHASE) ? solarDoc["production"]["pcu"]["wattHoursToday"].as<uint32_t>()
                                             : solarDoc["overview"]["lastDayData"]["energy"].as<uint32_t>();
    } else { // old api
      solarSystem->Actual = (src == ENPHASE) ? solarDoc["wattsNow"].as<uint32_t>()
                                             : solarDoc["overview"]["currentPower"]["power"].as<uint32_t>();
      solarSystem->Daily  = (src == ENPHASE) ? solarDoc["wattHoursToday"].as<uint32_t>()
                                             : solarDoc["overview"]["lastDayData"]["energy"].as<uint32_t>();
    }
  }

#ifdef DEBUG
  Debug("Daily > ");  Debugln(solarSystem->Daily);
  Debug("Actual > "); Debugln(solarSystem->Actual);
#endif
}

void GetSolarDataN() {
  if ( skipNetwork ) return;
  GetSolarData(ENPHASE,   false);
  GetSolarData(SOLAR_EDGE,false);
  GetSolarData(SMA,       false);
}

void SendSolarJson() {
  bool any = Enphase.Available || SolarEdge.Available || SMAinv.Available;
  if (!any) { httpServer.send(200, "application/json", "{\"active\":false}"); return; }

  uint32_t totalDaily  = Enphase.Daily + SolarEdge.Daily + SMAinv.Daily;
  uint32_t totalActual = Enphase.Actual + SolarEdge.Actual + SMAinv.Actual;
  uint32_t totalWp     = Enphase.Wp + SolarEdge.Wp + SMAinv.Wp;

  String Json = "{\"active\":true,\"total\":{\"daily\":";
  Json += String(totalDaily);
  Json += ",\"actual\":";
  Json += String(totalActual);
  Json += "}, \"Wp\":";
  Json += String(totalWp);
  Json += ", \"perc\":{\"seue\":";
  Json += String(0);
  Json += ",\"scr\":";
  Json += String(0);
  Json += ",\"pres\":";
  Json += String(0);
  Json += "}}";

#ifdef DEBUG
  DebugTln("SendSolarJson");
  DebugT("Solar Json: "); Debugln(Json);
#endif
  httpServer.send(200, "application/json", Json.c_str());
}