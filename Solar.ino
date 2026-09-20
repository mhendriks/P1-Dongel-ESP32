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

SolarPwrSystems Enphase   = { false, "https://envoy/ivp/pdm/energy", "", 0, 0, 0, 0, 0,  60, 0, "/enphase.json"  };
SolarPwrSystems SolarEdge = { false, "", "", 0, 0, 0, 0, 0, 300, 0, "/solaredge.json"  };
SolarPwrSystems SMAinv    = { false, "http://192.168.1.231",      "", 0, 0, 0, 0, 0,  15, 0, "/sma.json" };
SolarPwrSystems Omniksol  = { false, "", "", 0, 0, 0, 0, 0,  15, 0, "/omniksol.json"  };
static uint16_t LastSolarFetchDurationMs = 0;
static PvEnergyResource g_pvEnergyResource;

static uint32_t pvStaleAfterMs() {
  uint16_t interval = 60;
  switch (pvConnectorDriver) {
    case PV_DRIVER_MODBUS_TCP: interval = sunSpecPvConfig.pollIntervalSeconds; break;
    case PV_DRIVER_SOLAREDGE_HTTP: interval = solarEdgeBatteryConfig.pollIntervalSeconds; break;
    case PV_DRIVER_ENPHASE_HTTP: interval = enphasePvConfig.pollIntervalSeconds; break;
    case PV_DRIVER_SMA_HTTP: interval = smaPvConfig.pollIntervalSeconds; break;
    case PV_DRIVER_OMNIKSOL_HTTP: interval = omniksolPvConfig.pollIntervalSeconds; break;
    default: break;
  }
  return (uint32_t)constrain((int)interval * 2, 30, 3600) * 1000UL;
}

void updatePvResourceValues(const char* connector, const char* protocol, const char* profile,
                            uint8_t unitId, uint32_t actualW, uint32_t dailyWh, uint32_t wattPeak) {
  const uint32_t nowMs = millis();
  g_pvEnergyResource.connectorId = connector;
  g_pvEnergyResource.sourceProtocol = protocol;
  g_pvEnergyResource.profileId = profile;
  g_pvEnergyResource.sourceUnitId = unitId;
  g_pvEnergyResource.lastSuccessfulPollMs = nowMs;
  g_pvEnergyResource.activePower = EnergyMeasurement(actualW, "W", nowMs, 0, true);
  g_pvEnergyResource.dailyEnergy = EnergyMeasurement(dailyWh, "Wh", nowMs, 0, true);
  g_pvEnergyResource.maximumPower = EnergyMeasurement(wattPeak, "Wp", nowMs, 0, true);
}

const PvEnergyResource& pvEnergyResource() { return g_pvEnergyResource; }

static void invalidatePvResource() {
  g_pvEnergyResource.activePower.available = false;
  g_pvEnergyResource.dailyEnergy.available = false;
  g_pvEnergyResource.maximumPower.available = false;
}

bool pvDashboardData(uint32_t& actualW, uint32_t& dailyWh, uint32_t& wattPeak) {
  const uint32_t nowMs = millis();
  if (batteryMeasurementQuality(g_pvEnergyResource.activePower, nowMs, pvStaleAfterMs()) != EnergyMeasurementQuality::GOOD ||
      batteryMeasurementQuality(g_pvEnergyResource.dailyEnergy, nowMs, pvStaleAfterMs()) != EnergyMeasurementQuality::GOOD) return false;
  actualW = (uint32_t)g_pvEnergyResource.activePower.value;
  dailyWh = (uint32_t)g_pvEnergyResource.dailyEnergy.value;
  wattPeak = (uint32_t)g_pvEnergyResource.maximumPower.value;
  return true;
}

ApiResponse pvEnergyApiResponse() {
  const PvEnergyResource& pv = pvEnergyResource();
  JsonDocument doc;
  const uint32_t nowMs = millis();
  doc["id"] = pv.resourceId;
  doc["type"] = "pv";
  doc["connector"] = pv.connectorId;
  doc["profile"] = pv.profileId;
  doc["protocol"] = pv.sourceProtocol;
  doc["unit_id"] = pv.sourceUnitId;
  doc["last_successful_poll_ms"] = pv.lastSuccessfulPollMs;
  JsonObject measurements = doc["measurements"].to<JsonObject>();
#define ADD_PV_MEASUREMENT(name, field) \
  { JsonObject measurement = measurements[name].to<JsonObject>(); \
    measurement["value"] = pv.field.value; measurement["unit"] = pv.field.unit; \
    measurement["timestamp_ms"] = pv.field.timestampMs; \
    measurement["quality"] = energyMeasurementQualityText(batteryMeasurementQuality(pv.field, nowMs, pvStaleAfterMs())); }
  ADD_PV_MEASUREMENT("active_power", activePower)
  ADD_PV_MEASUREMENT("daily_energy", dailyEnergy)
  ADD_PV_MEASUREMENT("maximum_power", maximumPower)
#undef ADD_PV_MEASUREMENT
  String body; serializeJson(doc, body);
  return {200, "application/json", body};
}

static String   _sma_sid;
static uint32_t _sma_sid_t0 = 0;
static const size_t SMA_MAX_RESPONSE_LEN = 2048;
static const uint16_t SOLAR_HTTP_CONNECT_TIMEOUT_MS = 4000;
static const uint16_t SOLAR_HTTP_TIMEOUT_MS = 5000;

static void* solarSystemForSource(SolarSource src) {
  switch (src) {
    case ENPHASE:    return &Enphase;
    case SOLAR_EDGE: return &SolarEdge;
    case SMA:        return &SMAinv;
    case OMNIKSOL:   return &Omniksol;
  }
  return &Omniksol;
}

static SolarSource pvDriverSource() {
  switch (pvConnectorDriver) {
    case PV_DRIVER_SOLAREDGE_HTTP: return SOLAR_EDGE;
    case PV_DRIVER_ENPHASE_HTTP: return ENPHASE;
    case PV_DRIVER_SMA_HTTP: return SMA;
    default: return OMNIKSOL;
  }
}

void pvConnectorConfigChanged() {
  Enphase.Available = SolarEdge.Available = SMAinv.Available = Omniksol.Available = false;
  if (pvConnectorDriver == PV_DRIVER_NONE) {
    invalidatePvResource();
    solarEdgeBatteryConfigChanged();
    return;
  }
  if (pvConnectorDriver == PV_DRIVER_MODBUS_TCP) {
    solarEdgeBatteryConfigChanged();
    return;
  }
  if (pvConnectorDriver == PV_DRIVER_SOLAREDGE_HTTP) { solarEdgeBatteryConfigChanged(); return; }
  SolarPwrSystems* target = (SolarPwrSystems*)solarSystemForSource(pvDriverSource());
  HttpPvConfig* config = pvConnectorDriver == PV_DRIVER_ENPHASE_HTTP ? &enphasePvConfig
                       : pvConnectorDriver == PV_DRIVER_SMA_HTTP ? &smaPvConfig : &omniksolPvConfig;
  if (!config->url[0]) return;
  target->Url = config->url;
  target->Token = config->token;
  target->Interval = config->pollIntervalSeconds;
  target->Wp = pvWattPeak;
  target->Available = true;
  target->LastRefresh = 0;
  // The battery HTTP driver shares SolarEdge's power-flow endpoint. Keep its
  // separate poll alive when PV itself uses another connector.
  solarEdgeBatteryConfigChanged();
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

static const char* solarFetchTag(SolarSource src) {
  switch (src) {
    case ENPHASE:    return "solar-enphase";
    case SOLAR_EDGE: return "solar-solaredge";
    case SMA:        return "solar-sma";
    case OMNIKSOL:   return "solar-omnik";
  }
  return "solar-fetch";
}

static void publishPvSource(SolarSource src, const SolarPwrSystems& system) {
  if (pvConnectorDriver == PV_DRIVER_NONE || pvConnectorDriver == PV_DRIVER_MODBUS_TCP || src != pvDriverSource()) return;
  const char* profile = src == ENPHASE ? "enphase-http" : src == SOLAR_EDGE ? "solaredge-monitoring-api-v1"
                      : src == SMA ? "sma-http" : "omniksol-http";
  updatePvResourceValues(profile, "http-json", profile, 0, system.Actual, system.Daily, system.Wp);
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

static void resetSolarEdgeRuntimeState() {
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

  // Migrate each historic JSON integration into the connector settings.  The
  // selected driver determines which one is polled; none of the old files is
  // consulted again by the scheduler afterwards.
#define MIGRATE_PV_HTTP(target, source) \
  if (!target.url[0] && source.Url.length()) { strlcpy(target.url, source.Url.c_str(), sizeof(target.url)); strlcpy(target.token, source.Token.c_str(), sizeof(target.token)); target.pollIntervalSeconds = source.Interval; target.wattPeak = source.Wp; }
  MIGRATE_PV_HTTP(enphasePvConfig, Enphase)
  MIGRATE_PV_HTTP(smaPvConfig, SMAinv)
  MIGRATE_PV_HTTP(omniksolPvConfig, Omniksol)
#undef MIGRATE_PV_HTTP

  // Previous connector versions kept Wp inside each driver profile. Retain
  // the active profile's value once, then use the installation-wide setting.
  if (!pvWattPeak) {
    pvWattPeak = pvConnectorDriver == PV_DRIVER_MODBUS_TCP ? sunSpecPvConfig.wattPeak
        : pvConnectorDriver == PV_DRIVER_SOLAREDGE_HTTP ? solarEdgePvConfig.wattPeak
        : pvConnectorDriver == PV_DRIVER_ENPHASE_HTTP ? enphasePvConfig.wattPeak
        : pvConnectorDriver == PV_DRIVER_SMA_HTTP ? smaPvConfig.wattPeak : omniksolPvConfig.wattPeak;
  }

  // Existing SolarEdge PV installations were configured in solaredge.json.
  // Adopt those values once so they appear in the new connector without
  // making users enter their monitoring credentials again.
  if (pvConnectorDriver == PV_DRIVER_NONE && !solarEdgeBatteryConfig.siteId && SolarEdge.SiteID && SolarEdge.Token.length()) {
    solarEdgeBatteryConfig.siteId = SolarEdge.SiteID;
    strlcpy(solarEdgeBatteryConfig.apiKey, SolarEdge.Token.c_str(), sizeof(solarEdgeBatteryConfig.apiKey));
    solarEdgeBatteryConfig.pollIntervalSeconds = constrain(SolarEdge.Interval, 300U, 3600U);
    solarEdgePvConfig.wattPeak = SolarEdge.Wp;
    pvConnectorDriver = PV_DRIVER_SOLAREDGE_HTTP;
    writeSettings();
  }

  // One-time migration: retain old SolarEdge credentials in the shared
  // connection. Do not activate a battery just because credentials exist;
  // STORAGE in the power-flow response decides that later.
  if (!solarEdgeBatteryConfig.siteId && SolarEdge.SiteID && SolarEdge.Token.length()) {
    solarEdgeBatteryConfig.siteId = SolarEdge.SiteID;
    strlcpy(solarEdgeBatteryConfig.apiKey, SolarEdge.Token.c_str(), sizeof(solarEdgeBatteryConfig.apiKey));
    solarEdgeBatteryConfig.pollIntervalSeconds = constrain(SolarEdge.Interval, 300U, 3600U);
    writeSettings();
  }
  // The connector owns the credentials when SolarEdge is its active battery
  // driver. The same HTTP request remains shared with the PV integration.
  solarEdgeBatteryConfigChanged();
  // Keep the legacy /solaredge.json route working, but use the connector
  // settings as soon as the user explicitly selects a PV driver.
  pvConnectorConfigChanged();

  if (telegramCount == 0) {
    if (pvConnectorDriver != PV_DRIVER_MODBUS_TCP && pvConnectorDriver != PV_DRIVER_NONE) GetSolarData(pvDriverSource(), true);
    if (batteryConnectorDriver == BATTERY_DRIVER_SOLAREDGE_HTTP && pvDriverSource() != SOLAR_EDGE) GetSolarData(SOLAR_EDGE, true);
  } else {
    WorkerEnqueueSolarFetch();
  }
}

void solarEdgePvConfigChanged() {
  solarEdgeBatteryConfigChanged();
}

void solarEdgeBatteryConfigChanged() {
  if ((batteryConnectorDriver != BATTERY_DRIVER_SOLAREDGE_HTTP && pvConnectorDriver != PV_DRIVER_SOLAREDGE_HTTP) ||
      !solarEdgeBatteryConfig.siteId || !solarEdgeBatteryConfig.apiKey[0]) return;
  SolarEdge.Available = true;
  SolarEdge.SiteID = solarEdgeBatteryConfig.siteId;
  SolarEdge.Token = solarEdgeBatteryConfig.apiKey;
  SolarEdge.Interval = solarEdgeBatteryConfig.pollIntervalSeconds;
  SolarEdge.Wp = pvWattPeak;
  // The normal low-priority scheduler sees LastRefresh == 0 and performs one
  // fetch. Do not enqueue a direct job here: settings may arrive in a burst
  // and must never turn into repeated HTTPS connection attempts.
  SolarEdge.LastRefresh = 0;
}

void GetSolarData(SolarSource src, bool forceUpdate) {
  // This source object also feeds the dashboard and history for the local
  // SunSpec driver.  Do not let the HTTP scheduler overwrite it.
  if (src == SOLAR_EDGE && pvConnectorDriver == PV_DRIVER_MODBUS_TCP) return;
  SolarPwrSystems* solarSystem = (SolarPwrSystems*)solarSystemForSource(src);

  if (!solarSystem->Available) return;
  // LastRefresh == 0 explicitly requests an immediate first poll. This is
  // set after changing connector settings and at startup.
  if (!forceUpdate && solarSystem->LastRefresh &&
      ((uptime() - solarSystem->LastRefresh) < solarSystem->Interval)) return;
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
    publishPvSource(src, *solarSystem);
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

    String payload;
    JsonDocument solarDoc;
    solarSystem->Actual = 0;
    solarSystem->Daily = 0;
    resetSolarEdgeRuntimeState();

    auto fetchJson = [&](const String& url) -> bool {
      HTTPClient http;
      WiFiClient client;
      WiFiClientSecure clientTLS;
      if (!solarHttpBegin(http, client, clientTLS, url)) return false;
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
        JsonObject storage = flow["STORAGE"];
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

        if (!storage.isNull()) {
          if (batteryConnectorDriver == BATTERY_DRIVER_SOLAREDGE_HTTP) {
            updateSolarEdgeBattery(storage["currentPower"].as<float>(), flowUnit.c_str(),
                                   storage["chargeLevel"].as<uint8_t>(), storage["status"] | "", millis());
          }
        }
      } else {
        SolarEdgeFlowPvValid = false;
      }
    } else {
      SolarEdgeFlowPvValid = false;
    }

    noteSolarFetchDuration(fetchStartMs);
    publishPvSource(src, *solarSystem);
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
  publishPvSource(src, *solarSystem);
}

void GetSolarDataNFromWorker() {
  if ( skipNetwork ) return;
  if (pvConnectorDriver != PV_DRIVER_NONE && pvConnectorDriver != PV_DRIVER_MODBUS_TCP) GetSolarData(pvDriverSource(), false);
  if (batteryConnectorDriver == BATTERY_DRIVER_SOLAREDGE_HTTP && pvDriverSource() != SOLAR_EDGE) GetSolarData(SOLAR_EDGE, false);
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
  uint32_t actualW, dailyWh, wattPeak;
  return pvDashboardData(actualW, dailyWh, wattPeak) ? dailyWh : 0;
}

ApiResponse solarApiResponse() {
  uint32_t totalActual, totalDaily, totalWp;
  if (!pvDashboardData(totalActual, totalDaily, totalWp)) return {200, "application/json", "{\"active\":false}"};

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
  uint32_t actualW, dailyWh, wattPeak;
  if (!pvDashboardData(actualW, dailyWh, wattPeak)) return false;

  JsonObject solar = doc["solar"].to<JsonObject>();
  solar["active"] = true;
  JsonObject total = solar["total"].to<JsonObject>();
  total["daily"] = dailyWh;
  total["actual"] = actualW;
  solar["Wp"] = wattPeak;

  return true;
}
