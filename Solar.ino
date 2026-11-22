struct SolarPwrSystems {
  bool      Available;
  String    Url;
  String    Token;
  uint32_t  Wp;
  uint32_t  Actual;
  uint32_t  Daily;
  uint32_t  Interval;
  time_t    LastRefresh;
  char    file_name[20];
};

SolarPwrSystems Enphase  = { false, "https://envoy/ivp/pdm/energy", "", 0, 0, 0, 60, 0, "/enphase.json" };
SolarPwrSystems SMAinv   = { false, "http://192.168.1.231", "", 0, 0, 0, 15, 0, "/sma.json" };
SolarPwrSystems Omniksol = { false, "", "", 0, 0, 0, 15, 0, "/omniksol.json" };
// Local dongle day belonging to the last accepted Envoy day counter.
static uint32_t enphaseDailyDay = 0;

static uint32_t enphaseDailyAt(time_t timestamp) {
  return Enphase.Url.indexOf("CMD=inv_query") > 0 || timestamp / SECS_PER_DAY == enphaseDailyDay
      ? Enphase.Daily : 0;
}
// Modbus TCP and the HTTP PV connectors contribute independently.
// Other existing PV integrations can continue to contribute to the total.
static uint32_t modbusPvPower = 0, modbusPvDaily = 0, modbusPvLastUpdate = 0;
static bool modbusPvValid = false;

static uint32_t pvEnergySourceHash() {
  uint32_t hash = 2166136261UL;
  auto add = [&hash](const void* data, size_t size) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    while (size--) { hash ^= *bytes++; hash *= 16777619UL; }
  };
  add(pvModbusConfig.ip, strlen(pvModbusConfig.ip));
  add(&pvModbusConfig.port, sizeof(pvModbusConfig.port));
  add(&pvModbusConfig.id, sizeof(pvModbusConfig.id));
  const ConnectorField& f = pvModbusConfig.energy;
  add(&f.reg, sizeof(f.reg)); add(&f.type, sizeof(f.type));
  add(&f.scale, sizeof(f.scale)); add(&f.swap, sizeof(f.swap)); add(&f.sf, sizeof(f.sf));
  return hash;
}

void invalidateModbusPv() {
  modbusPvValid = false;
  modbusPvPower = modbusPvDaily = 0;
  apiWsMarkLiveDirty();
}

void updateModbusPv(double powerW, double totalWh) {
  modbusPvPower = uint32_t(powerW);
  modbusPvLastUpdate = millis();
  modbusPvValid = true;
  // Only calculate today's yield after the clock is synchronized. Keep the
  // baseline in NVS so a reboot does not erase production already observed.
  if (year() >= 2024) {
    static bool loaded = false;
    static PvDailyBaseline baseline;
    Preferences nvs;
    if (!loaded && nvs.begin("pv-energy", true)) {
      if (nvs.getBytesLength("baseline-v1") == sizeof(baseline))
        nvs.getBytes("baseline-v1", &baseline, sizeof(baseline));
      nvs.end();
      loaded = true;
    }
    const uint32_t today = uint32_t(year()) * 10000 + month() * 100 + day();
    if (baseline.observe(totalWh, today, pvEnergySourceHash())) {
      if (nvs.begin("pv-energy", false)) {
        nvs.putBytes("baseline-v1", &baseline, sizeof(baseline));
        nvs.end();
        loaded = true;
      }
    }
    modbusPvDaily = baseline.yieldWh(totalWh);
  }
  apiWsMarkLiveDirty();
}

static bool modbusPvAvailable() {
  return pvModbusConfig.enabled && modbusPvValid &&
      uint32_t(millis() - modbusPvLastUpdate) <= max(20000UL, uint32_t(pvModbusConfig.interval) * 2000UL);
}
static uint32_t modbusPvActualW() { return modbusPvAvailable() ? modbusPvPower : 0; }
static uint32_t modbusPvDailyWh() { return pvModbusConfig.enabled ? modbusPvDaily : 0; }
static uint32_t modbusPvWp() { return pvModbusConfig.enabled ? pvModbusConfig.wp : 0; }
static bool anySolarAvailable() { return Enphase.Available || SMAinv.Available || Omniksol.Available ||
    modbusPvAvailable(); }

static uint16_t LastSolarFetchDurationMs = 0;

static String   _sma_sid;
static uint32_t _sma_sid_t0 = 0;
static const size_t SMA_MAX_RESPONSE_LEN = 2048;
static const uint16_t SOLAR_HTTP_CONNECT_TIMEOUT_MS = 4000;
static const uint16_t SOLAR_HTTP_TIMEOUT_MS = 5000;

static void* solarSystemForSource(SolarSource src) {
  switch (src) {
    case ENPHASE:    return &Enphase;
    case SMA:        return &SMAinv;
    case OMNIKSOL:   return &Omniksol;
  }
  return &Omniksol;
}

static uint32_t defaultSolarRefreshInterval(SolarSource src) {
  switch (src) {
    case ENPHASE:    return 60;
    case SMA:        return 15;
    case OMNIKSOL:   return 15;
  }
  return 15;
}

static const char* solarFetchTag(SolarSource src) {
  switch (src) {
    case ENPHASE:    return "solar-enphase";
    case SMA:        return "solar-sma";
    case OMNIKSOL:   return "solar-omnik";
  }
  return "solar-fetch";
}

static bool solarHttpBegin(HTTPClient& http, WiFiClient& client, WiFiClientSecure& clientTLS, const String& url) {
  bool beginOk;
  if (url.startsWith("https://")) {
    clientTLS.setInsecure();
    beginOk = http.begin(clientTLS, url);
  } else {
    beginOk = http.begin(client, url);
  }

  if (!beginOk) return false;
  http.setConnectTimeout(SOLAR_HTTP_CONNECT_TIMEOUT_MS);
  http.setTimeout(SOLAR_HTTP_TIMEOUT_MS);
  http.addHeader("Connection", "close");
  return true;
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
  WiFiClient client;
  WiFiClientSecure clientTLS;

  bool https = url.startsWith("https://");
  bool beginOk = false;
  if (https) {
    clientTLS.setInsecure();
    beginOk = http.begin(clientTLS, url);
  } else {
    beginOk = http.begin(client, url);
  }

  if (!beginOk) return false;

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Connection", "close");
  http.setConnectTimeout(SOLAR_HTTP_CONNECT_TIMEOUT_MS);
  http.setTimeout(SOLAR_HTTP_TIMEOUT_MS);
  WDT_FEED();
  int rc = http.POST(body);
  WDT_FEED();
  if (rc == 200) {
    if (!smaReadHttpResponseCapped(http, out)) out = "";
  }
  http.end();
  WDT_FEED();
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

  solarSystem->Available = doc["enabled"] | true;
  solarSystem->Url   = doc["gateway-url"].as<String>();
  solarSystem->Token = doc["token"].as<String>();    // SMA uses this as the inverter password.
  solarSystem->Wp    = doc["wp"].as<uint32_t>();
  uint32_t ri        = doc["refresh-interval"].as<uint32_t>();
  uint32_t def = defaultSolarRefreshInterval(src);

  solarSystem->Interval = max(ri, def);
  solarSystem->Actual = solarSystem->Daily = 0;
  if (src == SMA) _sma_sid = "";

#ifdef DEBUG
  Debug("url > "); Debugln(solarSystem->Url);
  Debug("token > "); Debugln(solarSystem->Token.length() ? F("<set>") : F("<empty>"));
  Debug("wp > "); Debugln(solarSystem->Wp);
  Debug("interval > "); Debugln(solarSystem->Interval);
#endif

  solarSystem->LastRefresh = 0;
}

// Preserve the existing per-brand files; old files without enabled remain active.
ApiResponse handleSolarConnectorApi(const ApiRequestContext& request) {
  SolarSource src;
  if (request.pathArg == "enphase") src = ENPHASE;
  else if (request.pathArg == "sma") src = SMA;
  else if (request.pathArg == "omniksol") src = OMNIKSOL;
  else return {404, "application/json", "{\"error\":\"Unknown PV connector\"}"};
  SolarPwrSystems* system = (SolarPwrSystems*)solarSystemForSource(src);
  if (request.method == HTTP_POST) {
    JsonDocument input;
    if (deserializeJson(input, request.body) || !input["enabled"].is<bool>() ||
        !input["gateway-url"].is<const char*>() || !input["token"].is<const char*>() ||
        !input["wp"].is<uint32_t>() || !input["refresh-interval"].is<uint32_t>())
      return {400, "application/json", "{\"error\":\"Invalid PV configuration\"}"};
    String url = input["gateway-url"].as<String>();
    uint32_t interval = input["refresh-interval"].as<uint32_t>();
    if ((input["enabled"].as<bool>() && !url.startsWith("http://") && !url.startsWith("https://")) ||
        url.length() > 512 || input["token"].as<String>().length() > 4096 ||
        interval < defaultSolarRefreshInterval(src) || interval > 86400)
      return {400, "application/json", "{\"error\":\"Invalid URL or interval\"}"};
    if (!FSmounted) return {503, "application/json", "{\"error\":\"Storage unavailable\"}"};
    String temporary = String(system->file_name) + ".tmp";
    File file = LittleFS.open(temporary, "w");
    if (!file) return {500, "application/json", "{\"error\":\"Cannot save PV configuration\"}"};
    size_t written = serializeJson(input, file);
    file.close();
    if (written != measureJson(input) || !LittleFS.rename(temporary, system->file_name)) {
      LittleFS.remove(temporary);
      return {500, "application/json", "{\"error\":\"Cannot save PV configuration\"}"};
    }
    ReadSolarConfig(src);
    apiWsMarkLiveDirty();
    if (!skipNetwork) WorkerEnqueueSolarFetch();
  } else if (request.method != HTTP_GET) {
    return {405, "application/json", "{\"error\":\"Method not allowed\"}"};
  }
  JsonDocument output;
  output["enabled"] = system->Available;
  output["gateway-url"] = system->Url;
  output["token"] = system->Token;
  output["wp"] = system->Wp;
  output["refresh-interval"] = system->Interval;
  String body;
  serializeJson(output, body);
  return {200, "application/json", body};
}

void ReadSolarConfigs() {
  if ( skipNetwork ) return;
  ReadSolarConfig(ENPHASE);
  ReadSolarConfig(SMA);
  ReadSolarConfig(OMNIKSOL);

  if (telegramCount == 0) {
    GetSolarData(ENPHASE, true);
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
  const time_t fetchTime = now();
  if (src == ENPHASE && solarSystem->Url.indexOf("CMD=inv_query") <= 0 &&
      (year(fetchTime) < 2024 || hour(fetchTime) >= 23 || hour(fetchTime) < 5)) {
    // Keep the final day counter through midnight; resume after Envoy's reset.
    solarSystem->Actual = 0;
    return;
  }
  CrashLogMark(solarFetchTag(src), __LINE__);
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


  HTTPClient http;
  WiFiClient client;
  WiFiClientSecure clientTLS;
  String urlcheck = solarSystem->Url;
  bool bSolis = (urlcheck.indexOf("CMD=inv_query") > 0);
  if (!solarHttpBegin(http, client, clientTLS, solarSystem->Url)) { noteSolarFetchDuration(fetchStartMs); return; }
  http.addHeader("Accept", "application/json");
  if (src == ENPHASE) {
    if (!bSolis) http.addHeader("Authorization", "Bearer " + solarSystem->Token);
  } else {
    http.addHeader("X-API-Key", solarSystem->Token);
  }

  int httpResponseCode = http.GET();
  DebugVerboseT(F("HTTP Response code: ")); DebugVerboseLn(httpResponseCode);
  if (httpResponseCode != 200) { http.end(); noteSolarFetchDuration(fetchStartMs); return; }

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

  if (src == ENPHASE && !bSolis) {
    JsonVariantConst production = solarSystem->Url.endsWith("energy")
        ? solarDoc["production"]["pcu"].as<JsonVariantConst>() : solarDoc.as<JsonVariantConst>();
    JsonVariantConst watts = production["wattsNow"], wh = production["wattHoursToday"];
    if (fetchTime / SECS_PER_DAY != now() / SECS_PER_DAY ||
        !watts.is<double>() || !wh.is<double>() ||
        !isfinite(watts.as<double>()) || !isfinite(wh.as<double>()) ||
        fabs(watts.as<double>()) > UINT32_MAX || wh.as<double>() < 0 || wh.as<double>() > UINT32_MAX) {
      DebugTln(F("Enphase: ignored invalid reading"));
      noteSolarFetchDuration(fetchStartMs);
      return;
    }
    solarSystem->Actual = watts.as<double>() > 0 ? uint32_t(watts.as<double>()) : 0;
    solarSystem->Daily = wh.as<uint32_t>();
    enphaseDailyDay = fetchTime / SECS_PER_DAY;
    noteSolarFetchDuration(fetchStartMs);
    return;
  }

  if (bSolis) {
    solarSystem->Actual = solarDoc["i_pow_n"].as<uint32_t>();
    solarSystem->Daily  = (uint32_t)((float)solarDoc["i_eday"] * 1000.0f);
  } else {
    solarSystem->Actual = solarDoc["overview"]["currentPower"]["power"].as<uint32_t>();
    solarSystem->Daily  = solarDoc["overview"]["lastDayData"]["energy"].as<uint32_t>();
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
  return enphaseDailyAt(now()) + modbusPvDailyWh() + SMAinv.Daily + Omniksol.Daily;
}

uint32_t totalSolarDailyWhForTimestamp(const char* timestamp) {
  return enphaseDailyAt(epoch(timestamp, strlen(timestamp), false)) +
      modbusPvDailyWh() + SMAinv.Daily + Omniksol.Daily;
}

uint32_t totalSolarInstalledWp() {
  return (Enphase.Available ? Enphase.Wp : 0) + modbusPvWp() +
      (SMAinv.Available ? SMAinv.Wp : 0) + (Omniksol.Available ? Omniksol.Wp : 0);
}

ApiResponse solarApiResponse() {
  bool any = anySolarAvailable();
  if (!any) return {200, "application/json", "{\"active\":false}"};

  uint32_t totalDaily  = totalSolarDailyWh();
  uint32_t totalActual = Enphase.Actual + modbusPvActualW() + SMAinv.Actual + Omniksol.Actual;
  uint32_t totalWp     = totalSolarInstalledWp();

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
  bool any = anySolarAvailable();
  if (!any) return false;

  JsonObject solar = doc["solar"].to<JsonObject>();
  solar["active"] = true;
  JsonObject total = solar["total"].to<JsonObject>();
  total["daily"] = totalSolarDailyWh();
  total["actual"] = Enphase.Actual + modbusPvActualW() + SMAinv.Actual + Omniksol.Actual;
  solar["Wp"] = totalSolarInstalledWp();

  return true;
}
