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

extern float SolarEdgeFlowPvPower;
extern bool  SolarEdgeFlowPvValid;
extern AccuPwrSystems SolarEdgeAccu;

SolarPwrSystems Enphase   = { false, "https://envoy/ivp/pdm/energy", "", 0, 0, 0, 0, 0,  60, 0, "/enphase.json"  };
SolarPwrSystems SolarEdge = { false, "", "", 0, 0, 0, 0, 0, 300, 0, "/solaredge.json"  };
SolarPwrSystems SMAinv    = { false, "http://192.168.1.231",      "", 0, 0, 0, 0, 0,  15, 0, "/sma.json" };
SolarPwrSystems Omniksol  = { false, "", "", 0, 0, 0, 0, 0,  15, 0, "/omniksol.json"  };
static uint16_t LastSolarFetchDurationMs = 0;

static String   _sma_sid;
static uint32_t _sma_sid_t0 = 0;
static const size_t SMA_MAX_RESPONSE_LEN = 2048;

static void* solarSystemForSource(SolarSource src) {
  switch (src) {
    case ENPHASE:    return &Enphase;
    case SOLAR_EDGE: return &SolarEdge;
    case SMA:        return &SMAinv;
    case OMNIKSOL:   return &Omniksol;
  }
  return &Omniksol;
}

static uint32_t defaultSolarRefreshInterval(SolarSource src) {
  switch (src) {
    case ENPHASE:    return 60;
    case SOLAR_EDGE: return 300;
    case SMA:        return 15;
    case OMNIKSOL:   return 15;
  }
  return 15;
}

static void noteSolarFetchDuration(uint32_t startMs) {
  uint32_t duration = millis() - startMs;
  LastSolarFetchDurationMs = duration > UINT16_MAX ? UINT16_MAX : (uint16_t)duration;
}

uint16_t SolarLastFetchDurationMs() {
  return LastSolarFetchDurationMs;
}

uint16_t SolarEnphaseAgeSec() {
  if (!Enphase.Available || !Enphase.LastRefresh) return UINT16_MAX;
  time_t age = uptime() - Enphase.LastRefresh;
  if (age < 0) return 0;
  return age > UINT16_MAX ? UINT16_MAX : (uint16_t)age;
}

static void resetSolarEdgeRuntimeState() {
  SolarEdgeAccu.Available = false;
  SolarEdgeAccu.unit = "";
  SolarEdgeAccu.status = "";
  SolarEdgeAccu.currentPower = 0.0f;
  SolarEdgeAccu.chargeLevel = 0;
  SolarEdgeFlowPvPower = 0.0f;
  SolarEdgeFlowPvValid = false;
}

static bool smaReadMetricValue(JsonObject dev, const char* key, long& out) {
  if (dev.isNull()) return false;

  JsonObject metric = dev[key];
  if (metric.isNull()) return false;

  JsonArray values = metric["1"];
  if (values.isNull() || values.size() == 0) return false;

  JsonVariant val = values[0]["val"];
  if (val.isNull()) return false;

  out = val.as<long>();
  return true;
}

static String smaJsonEscape(const String& value) {
  String escaped;
  escaped.reserve(value.length() + 4);
  for (size_t i = 0; i < value.length(); i++) {
    char c = value.charAt(i);
    if (c == '"' || c == '\\') escaped += '\\';
    escaped += c;
  }
  return escaped;
}

static bool smaReadHttpResponseCapped(HTTPClient& http, String& out) {
  out = "";

  int len = http.getSize();
  if (len > 0 && len > (int)SMA_MAX_RESPONSE_LEN) return false;
  if (!out.reserve(len > 0 ? len : SMA_MAX_RESPONSE_LEN)) return false;

  WiFiClient* stream = http.getStreamPtr();
  if (!stream) return false;

  uint32_t lastDataMs = millis();
  while (http.connected() || stream->available()) {
    int available = stream->available();
    if (available > 0) {
      while (available-- > 0) {
        int c = stream->read();
        if (c < 0) break;
        if (out.length() >= SMA_MAX_RESPONSE_LEN) {
          out = "";
          return false;
        }
        out += (char)c;
      }
      lastDataMs = millis();
      WDT_FEED();
      if (len > 0 && out.length() >= (size_t)len) break;
      continue;
    }

    if ((uint32_t)(millis() - lastDataMs) > 5000) {
      out = "";
      return false;
    }
    delay(1);
    WDT_FEED();
  }

  out.trim();
  return out.length() > 0 && out.startsWith("{");
}

static bool smaHttpPOST(const String& url, const String& body, String& out) {
  out = "";
  HTTPClient http;
  WiFiClientSecure *clientTLS = nullptr;

  bool https = url.startsWith("https://");
  bool beginOk = false;
  if (https) {
    clientTLS = new WiFiClientSecure();
    if (!clientTLS) return false;
    clientTLS->setInsecure();
    beginOk = http.begin(*clientTLS, url);
  } else {
    beginOk = http.begin(url);
  }

  if (!beginOk) {
    if (clientTLS) delete clientTLS;
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Connection", "close");
  http.setTimeout(5000);
  WDT_FEED();
  int rc = http.POST(body);
  WDT_FEED();
  if (rc == 200) {
    if (!smaReadHttpResponseCapped(http, out)) out = "";
  }
  http.end();
  WDT_FEED();
  if (clientTLS) delete clientTLS;
  return (rc == 200 && out.length() > 0);
}

static bool smaLogin(const String& baseUrl, const String& password, const char* right = "usr") {
  String resp;
  String url  = baseUrl + "/dyn/login.json";
  String body = String("{\"pass\":\"") + smaJsonEscape(password) + "\",\"right\":\"" + right + "\"}";
  if (!smaHttpPOST(url, body, resp)) { _sma_sid = ""; DebugTln("Error smaHttpPOST"); return false; }
  JsonDocument doc;
  if (deserializeJson(doc, resp))     { _sma_sid = ""; DebugTln("Error sma deserializeJson error"); return false; }
  String sid = doc["result"]["sid"].as<String>();
  if (sid.isEmpty())                  { _sma_sid = ""; DebugTln("Error SMA sid empty"); return false; }
  _sma_sid = sid; _sma_sid_t0 = millis();
  return true;
}

static bool smaGetPacAndDay(const String& baseUrl, long& pac_W, long& day_Wh) {
  if (_sma_sid.isEmpty() || (millis() - _sma_sid_t0) > 12UL*60UL*1000UL) {
    if (!smaLogin(baseUrl, SMAinv.Token)) return false;
  }
  String resp;
  String url  = baseUrl + "/dyn/getValues.json?sid=" + _sma_sid;
  // SMA metric keys: 6100_40263F00 = current AC power, 6400_00262200 = daily yield.
  String body = "{\"destDev\":[],\"keys\":[\"6100_40263F00\",\"6400_00262200\"]}";
  if (!smaHttpPOST(url, body, resp)) { _sma_sid = ""; DebugTln("Error smaHttpPOST values"); return false; }
  JsonDocument doc;
  if (deserializeJson(doc, resp)) { _sma_sid = ""; return false; }

  JsonObject result = doc["result"];
  if (result.isNull()) { _sma_sid = ""; return false; }
  auto it = result.begin(); if (it == result.end()) { _sma_sid = ""; return false; }
  JsonObject dev = it->value();
  if (dev.isNull()) { _sma_sid = ""; return false; }

  long pac = 0;
  long day = 0;
  if (!smaReadMetricValue(dev, "6100_40263F00", pac)) return false;
  if (!smaReadMetricValue(dev, "6400_00262200", day)) return false;

  pac_W  = pac;
  day_Wh = day;
  return true;
}

void ReadSolarConfig(SolarSource src) {
  SolarPwrSystems* solarSystem = (SolarPwrSystems*)solarSystemForSource(src);

  if (!FSmounted || !LittleFS.exists(solarSystem->file_name)) return;
  DebugVerboseT(F("ReadSolarConfig: ")); DebugVerboseLn(solarSystem->file_name);

  JsonDocument doc;
  File f = LittleFS.open(solarSystem->file_name, "r");
  if (!f) { DebugTln("Error reading SolarFile"); return; }
  if (deserializeJson(doc, f)) { Debugln("Error deserialize SolarFile"); f.close(); return; }
  f.close();

  solarSystem->Available = true;
  solarSystem->Url   = doc["gateway-url"].as<String>();
  solarSystem->Token = doc["token"].as<String>();    // SMA uses this as the inverter password.
  solarSystem->Wp    = doc["wp"].as<uint32_t>();
  solarSystem->SiteID= doc["siteid"].as<uint32_t>();
  uint32_t ri        = doc["refresh-interval"].as<uint32_t>();
  uint32_t def = defaultSolarRefreshInterval(src);

  if (ri > def) solarSystem->Interval = ri;

#ifdef DEBUG
  Debug("url > "); Debugln(solarSystem->Url);
  Debug("token > "); Debugln(solarSystem->Token.length() ? F("<set>") : F("<empty>"));
  Debug("wp > "); Debugln(solarSystem->Wp);
  Debug("interval > "); Debugln(solarSystem->Interval);
  Debug("siteid > "); Debugln(solarSystem->SiteID);
#endif

  solarSystem->LastRefresh = 0;
}

void ReadSolarConfigs() {
  if ( skipNetwork ) return;
  ReadSolarConfig(ENPHASE);
  ReadSolarConfig(SOLAR_EDGE);
  ReadSolarConfig(SMA);
  ReadSolarConfig(OMNIKSOL);

  if (telegramCount == 0) {
    GetSolarData(ENPHASE, true);
    GetSolarData(SOLAR_EDGE, true);
    GetSolarData(SMA, true);
    GetSolarData(OMNIKSOL, true);
  } else {
    WorkerEnqueueSolarFetch();
  }
}

void GetSolarData(SolarSource src, bool forceUpdate) {
  SolarPwrSystems* solarSystem = (SolarPwrSystems*)solarSystemForSource(src);

  if (!solarSystem->Available) return;
  if (!forceUpdate && ((uptime() - solarSystem->LastRefresh) < solarSystem->Interval)) return;
  CrashLogMark(src == SMA ? "solar-sma" : "solar-fetch", __LINE__);
  solarSystem->LastRefresh = uptime();
  uint32_t fetchStartMs = millis();

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
    noteSolarFetchDuration(fetchStartMs);
    return;
  }

  if (src == SOLAR_EDGE) {
    String baseUrl = "https://monitoringapi.solaredge.com/site/" + String(SolarEdge.SiteID);
    String token = solarSystem->Token;
    token.trim();
    String today;
    if (strlen(actTimestamp) > 10) {
      today = buildDateTimeString(actTimestamp, strlen(actTimestamp)).substring(0, 10);
    } else {
      today = "1970-01-01";
    }
    String timeFrameUrl = baseUrl + "/timeFrameEnergy?startDate=" + today + "&endDate=" + today;
    String powerFlowUrl = baseUrl + "/currentPowerFlow.json";
    if (token.length() > 0) {
      timeFrameUrl += "&api_key=" + token;
      powerFlowUrl += "?api_key=" + token;
    }

    HTTPClient http;
    String payload;
    JsonDocument solarDoc;
    solarSystem->Actual = 0;
    solarSystem->Daily = 0;
    resetSolarEdgeRuntimeState();

    auto fetchJson = [&](const String& url) -> bool {
      http.begin(url.c_str());
      http.addHeader("Accept", "application/json");
      int rc = http.GET();
      DebugVerboseLn(F("Solaredge request"));
      DebugVerboseT(F("HTTP Response code: ")); DebugVerboseLn(rc);
      if (rc != 200) {
        payload = http.getString();
        if (payload.length()) {
          DebugTraceT(F("Solaredge response body: ")); DebugTraceLn(payload);
        }
        http.end();
        return false;
      }
      payload = http.getString();
      http.end();
      DeserializationError err = deserializeJson(solarDoc, payload);
      return !err;
    };

    if (fetchJson(timeFrameUrl)) {
      JsonObject timeFrame = solarDoc["timeFrameEnergy"];
      if (!timeFrame.isNull()) {
        JsonVariant energy;
        if (!timeFrame["energy"].isNull()) energy = timeFrame["energy"];
        if (energy.isNull() && !timeFrame["totalEnergy"].isNull()) energy = timeFrame["totalEnergy"];
        if (energy.isNull() && timeFrame["meters"].is<JsonArray>()) {
          JsonArray meters = timeFrame["meters"];
          if (meters.size() > 0 && !meters[0]["energy"].isNull()) {
            energy = meters[0]["energy"];
          }
        }
        if (!energy.isNull()) {
          solarSystem->Daily = (uint32_t)energy.as<float>();
        }
      }
      solarDoc.clear();
    }

    if (fetchJson(powerFlowUrl)) {
      JsonObject flow = solarDoc["siteCurrentPowerFlow"];
      if (!flow.isNull()) {
        String flowUnit = flow["unit"].as<const char*>();
        JsonObject pv = flow["PV"];
        if (!pv.isNull()) {
          float pvPower = pv["currentPower"].as<float>();
          SolarEdgeFlowPvPower = pvPower;
          SolarEdgeFlowPvValid = true;
          if (flowUnit == "kW") {
            solarSystem->Actual = (uint32_t)(pvPower * 1000.0f);
          } else {
            solarSystem->Actual = (uint32_t)(pvPower);
          }
        } else {
          SolarEdgeFlowPvPower = 0.0f;
          SolarEdgeFlowPvValid = false;
        }

        JsonObject storage = flow["STORAGE"];
        if (!storage.isNull()) {
          SolarEdgeAccu.Available = true;
          SolarEdgeAccu.unit = flowUnit;
          SolarEdgeAccu.status = storage["status"].as<const char*>();
          SolarEdgeAccu.currentPower = storage["currentPower"].as<float>();
          SolarEdgeAccu.chargeLevel = storage["chargeLevel"].as<uint8_t>();
        }
      } else {
        SolarEdgeFlowPvValid = false;
      }
    } else {
      SolarEdgeFlowPvValid = false;
    }

    noteSolarFetchDuration(fetchStartMs);
    return;
  }

  HTTPClient http;
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
  DebugVerboseT(F("HTTP Response code: ")); DebugVerboseLn(httpResponseCode);
  if (httpResponseCode <= 0) { http.end(); noteSolarFetchDuration(fetchStartMs); return; }

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
  if (deserializeJson(solarDoc, payload)) { Debugln("Error deserialisation solarDoc"); noteSolarFetchDuration(fetchStartMs); return; }

  if (bSolis) {
    solarSystem->Actual = solarDoc["i_pow_n"].as<uint32_t>();
    solarSystem->Daily  = (uint32_t)((float)solarDoc["i_eday"] * 1000.0f);
  } else {
    if (solarSystem->Url.endsWith("energy")) {
      solarSystem->Actual = (src == ENPHASE) ? solarDoc["production"]["pcu"]["wattsNow"].as<uint32_t>()
                                             : solarDoc["overview"]["currentPower"]["power"].as<uint32_t>();
      solarSystem->Daily  = (src == ENPHASE) ? solarDoc["production"]["pcu"]["wattHoursToday"].as<uint32_t>()
                                             : solarDoc["overview"]["lastDayData"]["energy"].as<uint32_t>();
    } else {
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
  noteSolarFetchDuration(fetchStartMs);
}

void GetSolarDataNFromWorker() {
  if ( skipNetwork ) return;
  GetSolarData(ENPHASE,    false);
  WDT_FEED();
  GetSolarData(SOLAR_EDGE, false);
  WDT_FEED();
  GetSolarData(SMA,        false);
  WDT_FEED();
  GetSolarData(OMNIKSOL,   false);
  WDT_FEED();
}

void GetSolarDataN() {
  if (RngWritePending()) return;

  static uint32_t nextScheduleMs = 0;
  uint32_t now = millis();
  if ((int32_t)(now - nextScheduleMs) < 0) return;
  nextScheduleMs = now + 1000;
  WorkerEnqueueSolarFetch();
}

uint32_t totalSolarDailyWh() {
  return Enphase.Daily + SolarEdge.Daily + SMAinv.Daily + Omniksol.Daily;
}

ApiResponse solarApiResponse() {
  bool any = Enphase.Available || SolarEdge.Available || SMAinv.Available || Omniksol.Available;
  if (!any) return {200, "application/json", "{\"active\":false}"};

  uint32_t totalDaily  = totalSolarDailyWh();
  uint32_t totalActual = Enphase.Actual + SolarEdge.Actual + SMAinv.Actual + Omniksol.Actual;
  uint32_t totalWp     = Enphase.Wp     + SolarEdge.Wp     + SMAinv.Wp     + Omniksol.Wp;

  JsonDocument doc;
  doc["active"] = true;
  JsonObject total = doc["total"].to<JsonObject>();
  total["daily"] = totalDaily;
  total["actual"] = totalActual;
  doc["Wp"] = totalWp;
  JsonObject perc = doc["perc"].to<JsonObject>();
  perc["seue"] = 0;
  perc["scr"] = 0;
  perc["pres"] = 0;

  String body;
  serializeJson(doc, body);

#ifdef DEBUG
  DebugTln("SendSolarJson");
  DebugT("Solar Json: "); Debugln(body);
#endif
  return {200, "application/json", body};
}

bool fillDashSolarJson(JsonDocument& doc) {
  bool any = Enphase.Available || SolarEdge.Available || SMAinv.Available || Omniksol.Available;
  if (!any) return false;

  JsonObject solar = doc["solar"].to<JsonObject>();
  solar["active"] = true;
  JsonObject total = solar["total"].to<JsonObject>();
  total["daily"] = totalSolarDailyWh();
  total["actual"] = Enphase.Actual + SolarEdge.Actual + SMAinv.Actual + Omniksol.Actual;
  solar["Wp"] = Enphase.Wp + SolarEdge.Wp + SMAinv.Wp + Omniksol.Wp;

  return true;
}
