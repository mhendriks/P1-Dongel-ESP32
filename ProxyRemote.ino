/*
***************************************************************************
**  Program  : ProxyRemote, optional outbound proxy publisher
**
**  Copyright (c) 2026 Smartstuff
**
**  TERMS OF USE: MIT License.
***************************************************************************
*/

namespace {
#if REMOTE_PROXY
WiFiClientSecure proxyTlsClient;
WiFiClient proxyClient;
uint32_t proxyNextAttemptMs = 0;

uint32_t proxyCurrentEpoch() {
  if (actT > 1000) return (uint32_t)actT;
  if (newT > 1000) return (uint32_t)newT;
  return (uint32_t)now();
}

void proxySetError(const char* errorText) {
  proxyConnected = false;
  proxyErrorCount++;
  strlcpy(proxyLastError, errorText ? errorText : "", sizeof(proxyLastError));
}

void proxyEnsureIdentity() {
  bool changed = false;

  if (strlen(settingProxyDeviceId) == 0) {
    snprintf(settingProxyDeviceId, sizeof(settingProxyDeviceId), "P1-%s", macID);
    changed = true;
  }

  if (strlen(settingProxySecret) == 0) {
    const uint32_t chipTail = (uint32_t)(_getChipId() & 0xFFFF);
    snprintf(settingProxySecret, sizeof(settingProxySecret), "%s-%04X", DongleID, chipTail);
    changed = true;
  }

  if (strlen(settingProxyPath) == 0 || settingProxyPath[0] != '/') {
    strlcpy(settingProxyPath, PROXY_REMOTE_DEFAULT_PATH, sizeof(settingProxyPath));
    changed = true;
  }

  if (strlen(settingProxyHost) == 0) {
    strlcpy(settingProxyHost, PROXY_REMOTE_DEFAULT_HOST, sizeof(settingProxyHost));
    changed = true;
  }

  settingProxyInterval = constrain(settingProxyInterval, (uint32_t)5, (uint32_t)3600);
  settingProxyPort = constrain(settingProxyPort, (uint16_t)1, (uint16_t)65535);

  if (changed && FSmounted) writeSettings();
}

String proxyRemoteUrl() {
  String url = bProxyUseTLS ? "https://" : "http://";
  url += settingProxyHost;
  url += ":";
  url += settingProxyPort;
  url += settingProxyPath;
  return url;
}

float proxyCurrentWaterM3() {
  if (mbusWater) return waterDelivered;
  if (WtrMtr) return (float)P1Status.wtr_m3 + (P1Status.wtr_l / 1000.0f);
  return 0.0f;
}

float proxyDailyDelta(uint32_t current, uint32_t baseline) {
  if (current < baseline) return 0.0f;
  return (current - baseline) / 1000.0f;
}

String proxyBuildPayload() {
  JsonDocument doc;
  const float importT1 = DSMRdata.energy_delivered_tariff1_present ? DSMRdata.energy_delivered_tariff1.val() : 0.0f;
  const float importT2 = DSMRdata.energy_delivered_tariff2_present ? DSMRdata.energy_delivered_tariff2.val() : 0.0f;
  const float exportT1 = DSMRdata.energy_returned_tariff1_present ? DSMRdata.energy_returned_tariff1.val() : 0.0f;
  const float exportT2 = DSMRdata.energy_returned_tariff2_present ? DSMRdata.energy_returned_tariff2.val() : 0.0f;
  const float gasM3 = mbusGas ? gasDelivered : 0.0f;
  const float waterM3 = proxyCurrentWaterM3();

  doc["device_id"] = settingProxyDeviceId;
  doc["hostname"] = settingHostname;
  doc["mac"] = macStr;
  doc["fw_version"] = _VERSION_ONLY;
  doc["hardware"] = HWTypeNames[HardwareType];
  doc["timestamp"] = buildDateTimeString(actTimestamp, sizeof(actTimestamp));
  doc["epoch"] = proxyCurrentEpoch();
  doc["online"] = true;
  doc["ip"] = IP_Address();

  JsonObject live = doc["live"].to<JsonObject>();
  live["power_delivered_kw"] = DSMRdata.power_delivered_present ? DSMRdata.power_delivered.val() : 0.0f;
  live["power_returned_kw"] = DSMRdata.power_returned_present ? DSMRdata.power_returned.val() : 0.0f;
  live["net_power_kw"] = live["power_delivered_kw"].as<float>() - live["power_returned_kw"].as<float>();
  live["voltage_l1"] = DSMRdata.voltage_l1_present ? DSMRdata.voltage_l1.val() : 0.0f;
  live["voltage_l2"] = DSMRdata.voltage_l2_present ? DSMRdata.voltage_l2.val() : 0.0f;
  live["voltage_l3"] = DSMRdata.voltage_l3_present ? DSMRdata.voltage_l3.val() : 0.0f;
  live["current_l1"] = DSMRdata.current_l1_present ? DSMRdata.current_l1.val() : 0.0f;
  live["current_l2"] = DSMRdata.current_l2_present ? DSMRdata.current_l2.val() : 0.0f;
  live["current_l3"] = DSMRdata.current_l3_present ? DSMRdata.current_l3.val() : 0.0f;
  live["gas_m3"] = gasM3;
  live["water_m3"] = waterM3;
  live["quarter_peak_kw"] = DSMRdata.peak_pwr_last_q_present ? (DSMRdata.peak_pwr_last_q.int_val() / 1000.0f) : 0.0f;
  live["highest_peak_kw"] = DSMRdata.highest_peak_pwr_present ? (DSMRdata.highest_peak_pwr.int_val() / 1000.0f) : 0.0f;
  live["wifi_rssi"] = (netw_state == NW_WIFI && WiFi.isConnected()) ? WiFi.RSSI() : 0;

  JsonObject daily = doc["daily"].to<JsonObject>();
  daily["import_kwh"] = proxyDailyDelta((uint32_t)((importT1 + importT2) * 1000.0f), dataYesterday.t1 + dataYesterday.t2);
  daily["export_kwh"] = proxyDailyDelta((uint32_t)((exportT1 + exportT2) * 1000.0f), dataYesterday.t1r + dataYesterday.t2r);
  daily["gas_m3"] = proxyDailyDelta((uint32_t)(gasM3 * 1000.0f), dataYesterday.gas);
  daily["water_m3"] = proxyDailyDelta((uint32_t)(waterM3 * 1000.0f), dataYesterday.water);
  daily["import_t1_kwh"] = proxyDailyDelta((uint32_t)(importT1 * 1000.0f), dataYesterday.t1);
  daily["import_t2_kwh"] = proxyDailyDelta((uint32_t)(importT2 * 1000.0f), dataYesterday.t2);
  daily["export_t1_kwh"] = proxyDailyDelta((uint32_t)(exportT1 * 1000.0f), dataYesterday.t1r);
  daily["export_t2_kwh"] = proxyDailyDelta((uint32_t)(exportT2 * 1000.0f), dataYesterday.t2r);

  JsonObject auth = doc["auth"].to<JsonObject>();
  auth["secret"] = settingProxySecret;
  auth["token_set"] = strlen(settingProxyToken) > 0;

  String payload;
  serializeJson(doc, payload);
  return payload;
}

void proxyHandleSuccessPayload(const String& body) {
  if (body.length() == 0) return;

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, body);
  if (error) return;

  const char* token = doc["token"] | nullptr;
  if (token && token[0] != '\0' && strcmp(token, settingProxyToken) != 0) {
    strlcpy(settingProxyToken, token, sizeof(settingProxyToken));
    writeSettings();
  }

  if (doc["interval"].is<int>()) {
    settingProxyInterval = constrain(doc["interval"].as<int>(), 5, 3600);
  }
}

bool proxyPublishSnapshot() {
  if (!bProxyEnabled || skipNetwork || netw_state == NW_NONE) return false;
  if (telegramCount == 0) return false;

  proxyEnsureIdentity();
  proxyLastAttempt = proxyCurrentEpoch();

  HTTPClient http;
  const String url = proxyRemoteUrl();
  bool beginOk = false;

  if (bProxyUseTLS) {
    proxyTlsClient.setInsecure();
    proxyTlsClient.setTimeout(5000);
    beginOk = http.begin(proxyTlsClient, url);
  } else {
    proxyClient.setTimeout(5000);
    beginOk = http.begin(proxyClient, url);
  }

  if (!beginOk) {
    proxySetError("proxy begin failed");
    return false;
  }

  http.setConnectTimeout(4000);
  http.setTimeout(5000);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Id", settingProxyDeviceId);
  http.addHeader("X-Device-Secret", settingProxySecret);
  if (strlen(settingProxyToken) > 0) http.addHeader("Authorization", "Bearer " + String(settingProxyToken));

  const int httpCode = http.POST(proxyBuildPayload());
  proxyLastHttpCode = httpCode > 0 ? (uint16_t)httpCode : 0;

  if (httpCode >= 200 && httpCode < 300) {
    proxyConnected = true;
    proxyLastSuccess = proxyCurrentEpoch();
    proxySentCount++;
    proxyLastError[0] = '\0';
    proxyHandleSuccessPayload(http.getString());
    http.end();
    return true;
  }

  String err = "proxy http ";
  err += httpCode;
  proxySetError(err.c_str());
  http.end();
  return false;
}
#endif
} // namespace

void ProxyRemoteSetup() {
#if REMOTE_PROXY
  proxyEnsureIdentity();
  proxyNextAttemptMs = millis() + 5000UL;
  proxyConnected = false;
  proxyLastHttpCode = 0;
  proxyLastError[0] = '\0';
#endif
}

void ProxyRemoteNotifyConfigChanged() {
#if REMOTE_PROXY
  proxyEnsureIdentity();
  proxyConnected = false;
  proxyLastHttpCode = 0;
  proxyLastError[0] = '\0';
  proxyNextAttemptMs = 0;
#endif
}

void ProxyRemoteLoop() {
#if REMOTE_PROXY
  if (!bProxyEnabled || skipNetwork || netw_state == NW_NONE) {
    proxyConnected = false;
    return;
  }

  const uint32_t nowMs = millis();
  if ((int32_t)(nowMs - proxyNextAttemptMs) < 0) return;

  if (proxyPublishSnapshot()) {
    proxyNextAttemptMs = nowMs + (settingProxyInterval * 1000UL);
  } else {
    proxyNextAttemptMs = nowMs + PROXY_REMOTE_RETRY_MIN_MS;
  }
#endif
}

String proxyStatusJson() {
  JsonDocument doc;
#if REMOTE_PROXY
  doc["compiled"] = true;
  doc["enabled"] = bProxyEnabled;
  doc["connected"] = proxyConnected;
  doc["tls"] = bProxyUseTLS;
  doc["host"] = settingProxyHost;
  doc["port"] = settingProxyPort;
  doc["path"] = settingProxyPath;
  doc["interval"] = settingProxyInterval;
  doc["device_id"] = settingProxyDeviceId;
  doc["secret"] = settingProxySecret;
  doc["token_set"] = strlen(settingProxyToken) > 0;
  doc["last_success"] = proxyLastSuccess;
  doc["last_attempt"] = proxyLastAttempt;
  doc["last_http_code"] = proxyLastHttpCode;
  doc["sent_count"] = proxySentCount;
  doc["error_count"] = proxyErrorCount;
  doc["last_error"] = proxyLastError;
  doc["url"] = proxyRemoteUrl();
#else
  doc["compiled"] = false;
  doc["enabled"] = false;
  doc["message"] = "Remote proxy support is disabled at compile time";
#endif

  String out;
  serializeJson(doc, out);
  return out;
}
