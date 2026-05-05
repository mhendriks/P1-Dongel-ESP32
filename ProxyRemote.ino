/*
***************************************************************************
**  Program  : ProxyRemote, optional outbound proxy websocket client
**
**  Copyright (c) 2026 Smartstuff
**
**  TERMS OF USE: MIT License.
***************************************************************************
*/

namespace {
#if REMOTE_PROXY
enum ProxyMsgType : uint8_t {
  PROXY_MSG_AUTH = 1,
  PROXY_MSG_AUTH_ACK = 2,
  PROXY_MSG_MODE = 3,
  PROXY_MSG_HEARTBEAT = 4,
  PROXY_MSG_LIVE = 5,
  PROXY_MSG_DAILY = 6,
  PROXY_MSG_ERROR = 7,
};

enum ProxyStatusId : uint8_t {
  PROXY_STATUS_OK = 1,
  PROXY_STATUS_ERROR = 2,
};

WebSocketsClient proxyWsClient;
bool proxyWsStarted = false;
uint32_t proxyNextHeartbeatMs = 0;
uint32_t proxyNextLiveMs = 0;
uint32_t proxyLastLiveTelegramCount = 0;
uint32_t proxyLastDailyEpochDay = 0;
bool proxyNeedDailyPush = false;
bool proxyForceLivePush = false;

char proxyReadableCodeChar(size_t index, uint32_t seed) {
  static const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  const size_t alphabetLen = sizeof(alphabet) - 1;
  const uint32_t mixed = seed ^ (0x9E3779B9UL * (uint32_t)(index + 1));
  return alphabet[mixed % alphabetLen];
}

void proxyBuildReadableCode(char* out, size_t outSize, uint32_t seed) {
  if (!out || outSize < 16) return;

  const uint8_t groups[] = {3, 4, 3, 4};
  size_t pos = 0;
  size_t charIndex = 0;
  for (size_t group = 0; group < 4; group++) {
    for (uint8_t i = 0; i < groups[group] && pos + 1 < outSize; i++) {
      out[pos++] = proxyReadableCodeChar(charIndex++, seed + (group * 131UL));
    }
    if (group < 3 && pos + 1 < outSize) out[pos++] = '-';
  }
  out[pos] = '\0';
}

uint32_t proxyCurrentEpoch() {
  if (actT > 1000) return (uint32_t)actT;
  if (newT > 1000) return (uint32_t)newT;
  return (uint32_t)now();
}

const char* proxyModeName() {
  return proxyMode == PROXY_LIVE ? "live" : "idle";
}

void proxySetError(const char* errorText) {
  proxyConnected = false;
  proxyAuthenticated = false;
  proxyErrorCount++;
  proxyLastHttpCode = 0;
  strlcpy(proxyLastError, errorText ? errorText : "", sizeof(proxyLastError));
}

void proxyClearError() {
  proxyLastError[0] = '\0';
}

void proxyEnsureIdentity() {
  bool changed = false;
  const uint32_t chipId = (uint32_t)_getChipId();

  if (strlen(settingProxyDeviceId) == 0) {
    snprintf(settingProxyDeviceId, sizeof(settingProxyDeviceId), "P1-%s", macID);
    changed = true;
  }

  if (strlen(settingProxySecret) == 0) {
    proxyBuildReadableCode(settingProxySecret, sizeof(settingProxySecret), chipId ^ 0x53454352UL);
    changed = true;
  }

  if (strlen(settingProxyToken) == 0) {
    proxyBuildReadableCode(settingProxyToken, sizeof(settingProxyToken), chipId ^ 0x544F4B4EUL);
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
  proxyHeartbeatInterval = settingProxyInterval;

  if (changed && FSmounted) writeSettings();
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

String proxyBuildAuthPayload() {
  JsonDocument doc;
  doc["type_id"] = PROXY_MSG_AUTH;
  doc["device_id"] = settingProxyDeviceId;
  doc["token"] = settingProxyToken;
  doc["secret"] = settingProxySecret;
  doc["hostname"] = settingHostname;
  doc["fw_version"] = _VERSION_ONLY;
  doc["hw_version"] = HWTypeNames[HardwareType];
  doc["mac_address"] = macStr;
  doc["ip"] = IP_Address();

  String payload;
  serializeJson(doc, payload);
  return payload;
}

String proxyBuildHeartbeatPayload() {
  JsonDocument doc;
  doc["type_id"] = PROXY_MSG_HEARTBEAT;
  doc["device_id"] = settingProxyDeviceId;
  doc["ts"] = buildDateTimeString(actTimestamp, sizeof(actTimestamp));
  doc["hostname"] = settingHostname;
  doc["fw_version"] = _VERSION_ONLY;
  doc["hw_version"] = HWTypeNames[HardwareType];
  doc["mac_address"] = macStr;
  doc["ip"] = IP_Address();
  doc["wifi_rssi"] = (netw_state == NW_WIFI && WiFi.isConnected()) ? WiFi.RSSI() : 0;
  doc["uptime_sec"] = millis() / 1000UL;

  String payload;
  serializeJson(doc, payload);
  return payload;
}

String proxyBuildLivePayload() {
  JsonDocument doc;
  doc["type_id"] = PROXY_MSG_LIVE;
  doc["device_id"] = settingProxyDeviceId;
  doc["ts"] = buildDateTimeString(actTimestamp, sizeof(actTimestamp));
  doc["meter_ts"] = actTimestamp;
  doc["hostname"] = settingHostname;
  doc["fw_version"] = _VERSION_ONLY;
  doc["hardware"] = HWTypeNames[HardwareType];
  doc["mac_address"] = macStr;
  doc["ip"] = IP_Address();
  doc["electricity_tariff"] = DSMRdata.electricity_tariff_present ? DSMRdata.electricity_tariff.toInt() : 1;
  doc["power_delivered_kw"] = DSMRdata.power_delivered_present ? DSMRdata.power_delivered.val() : 0.0f;
  doc["power_returned_kw"] = DSMRdata.power_returned_present ? DSMRdata.power_returned.val() : 0.0f;
  doc["energy_delivered_t1_kwh"] = DSMRdata.energy_delivered_tariff1_present ? DSMRdata.energy_delivered_tariff1.val() : 0.0f;
  doc["energy_delivered_t2_kwh"] = DSMRdata.energy_delivered_tariff2_present ? DSMRdata.energy_delivered_tariff2.val() : 0.0f;
  doc["energy_returned_t1_kwh"] = DSMRdata.energy_returned_tariff1_present ? DSMRdata.energy_returned_tariff1.val() : 0.0f;
  doc["energy_returned_t2_kwh"] = DSMRdata.energy_returned_tariff2_present ? DSMRdata.energy_returned_tariff2.val() : 0.0f;
  doc["voltage_l1_v"] = DSMRdata.voltage_l1_present ? DSMRdata.voltage_l1.val() : 0.0f;
  doc["voltage_l2_v"] = DSMRdata.voltage_l2_present ? DSMRdata.voltage_l2.val() : 0.0f;
  doc["voltage_l3_v"] = DSMRdata.voltage_l3_present ? DSMRdata.voltage_l3.val() : 0.0f;
  doc["current_l1_a"] = DSMRdata.current_l1_present ? DSMRdata.current_l1.val() : 0.0f;
  doc["current_l2_a"] = DSMRdata.current_l2_present ? DSMRdata.current_l2.val() : 0.0f;
  doc["current_l3_a"] = DSMRdata.current_l3_present ? DSMRdata.current_l3.val() : 0.0f;
  doc["peak_pwr_last_q_kw"] = DSMRdata.peak_pwr_last_q_present ? (DSMRdata.peak_pwr_last_q.int_val() / 1000.0f) : 0.0f;
  doc["highest_peak_pwr_kw"] = DSMRdata.highest_peak_pwr_present ? (DSMRdata.highest_peak_pwr.int_val() / 1000.0f) : 0.0f;
  doc["gas_delivered_m3"] = mbusGas ? gasDelivered : 0.0f;
  doc["gas_ts"] = gasDeliveredTimestamp;
  doc["water_m3"] = proxyCurrentWaterM3();
  doc["wifi_rssi"] = (netw_state == NW_WIFI && WiFi.isConnected()) ? WiFi.RSSI() : 0;

  String payload;
  serializeJson(doc, payload);
  return payload;
}

String proxyBuildDailyPayload() {
  JsonDocument doc;
  const float importT1 = DSMRdata.energy_delivered_tariff1_present ? DSMRdata.energy_delivered_tariff1.val() : 0.0f;
  const float importT2 = DSMRdata.energy_delivered_tariff2_present ? DSMRdata.energy_delivered_tariff2.val() : 0.0f;
  const float exportT1 = DSMRdata.energy_returned_tariff1_present ? DSMRdata.energy_returned_tariff1.val() : 0.0f;
  const float exportT2 = DSMRdata.energy_returned_tariff2_present ? DSMRdata.energy_returned_tariff2.val() : 0.0f;
  const float gasM3 = mbusGas ? gasDelivered : 0.0f;
  const float waterM3 = proxyCurrentWaterM3();
  const String isoTs = buildDateTimeString(actTimestamp, sizeof(actTimestamp));

  doc["type_id"] = PROXY_MSG_DAILY;
  doc["device_id"] = settingProxyDeviceId;
  doc["ts"] = isoTs;
  doc["day"] = isoTs.substring(0, 10);
  doc["import_kwh"] = proxyDailyDelta((uint32_t)((importT1 + importT2) * 1000.0f), dataYesterday.t1 + dataYesterday.t2);
  doc["export_kwh"] = proxyDailyDelta((uint32_t)((exportT1 + exportT2) * 1000.0f), dataYesterday.t1r + dataYesterday.t2r);
  doc["gas_m3"] = proxyDailyDelta((uint32_t)(gasM3 * 1000.0f), dataYesterday.gas);
  doc["water_m3"] = proxyDailyDelta((uint32_t)(waterM3 * 1000.0f), dataYesterday.water);
  doc["import_t1_kwh"] = proxyDailyDelta((uint32_t)(importT1 * 1000.0f), dataYesterday.t1);
  doc["import_t2_kwh"] = proxyDailyDelta((uint32_t)(importT2 * 1000.0f), dataYesterday.t2);
  doc["export_t1_kwh"] = proxyDailyDelta((uint32_t)(exportT1 * 1000.0f), dataYesterday.t1r);
  doc["export_t2_kwh"] = proxyDailyDelta((uint32_t)(exportT2 * 1000.0f), dataYesterday.t2r);

  String payload;
  serializeJson(doc, payload);
  return payload;
}

void proxySendJson(const String& payload) {
  if (!proxyWsStarted || !proxyConnected) return;
  String mutablePayload(payload);
  proxyWsClient.sendTXT(mutablePayload);
  proxySentCount++;
}

void proxySendAuth() {
  proxySendJson(proxyBuildAuthPayload());
}

void proxySendHeartbeat() {
  proxyLastAttempt = proxyCurrentEpoch();
  proxySendJson(proxyBuildHeartbeatPayload());
}

void proxySendLive() {
  proxyLastAttempt = proxyCurrentEpoch();
  proxySendJson(proxyBuildLivePayload());
}

void proxySendDaily() {
  proxyLastAttempt = proxyCurrentEpoch();
  proxySendJson(proxyBuildDailyPayload());
  proxyLastDailyEpochDay = proxyCurrentEpoch() / (3600UL * 24UL);
  proxyNeedDailyPush = false;
}

void proxyApplyControlMessage(JsonDocument& doc) {
  const String token = doc["token"] | "";
  if (token.length() > 0 && strcmp(token.c_str(), settingProxyToken) != 0) {
    strlcpy(settingProxyToken, token.c_str(), sizeof(settingProxyToken));
    writeSettings();
  }

  if (doc["heartbeat_interval"].is<int>()) {
    proxyHeartbeatInterval = constrain(doc["heartbeat_interval"].as<int>(), 5, 3600);
  }

  if (doc["live_interval"].is<int>()) {
    proxyLiveIntervalMs = constrain((uint32_t)doc["live_interval"].as<int>() * 1000UL, 1000UL, 60000UL);
  } else if (doc["live_interval_ms"].is<int>()) {
    proxyLiveIntervalMs = constrain((uint32_t)doc["live_interval_ms"].as<int>(), 1000UL, 60000UL);
  }

  const int modeId = doc["mode_id"] | -1;
  if (modeId == PROXY_LIVE) {
    if (proxyMode != PROXY_LIVE) {
      proxyNeedDailyPush = true;
      proxyForceLivePush = true;
      proxyNextLiveMs = millis();
    }
    proxyMode = PROXY_LIVE;
  } else if (modeId == PROXY_IDLE) {
    proxyMode = PROXY_IDLE;
    proxyForceLivePush = false;
  }
}

void proxyHandleTextMessage(const char* payload, size_t length) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    proxySetError("proxy invalid json");
    return;
  }

  const int typeId = doc["type_id"] | -1;
  if (typeId == PROXY_MSG_AUTH_ACK) {
    const int statusId = doc["status_id"] | -1;
    if (statusId == PROXY_STATUS_OK) {
      proxyAuthenticated = true;
      proxyConnected = true;
      proxyLastSuccess = proxyCurrentEpoch();
      proxyLastHttpCode = 101;
      proxyClearError();
      proxyApplyControlMessage(doc);
      proxyNeedDailyPush = true;
      proxyForceLivePush = (proxyMode == PROXY_LIVE);
      proxyNextHeartbeatMs = millis() + (proxyHeartbeatInterval * 1000UL);
      proxyNextLiveMs = millis() + proxyLiveIntervalMs;
    } else {
      const String reason = doc["reason"] | "proxy auth failed";
      proxySetError(reason.c_str());
    }
    return;
  }

  if (typeId == PROXY_MSG_MODE) {
    proxyApplyControlMessage(doc);
    proxyLastSuccess = proxyCurrentEpoch();
    proxyClearError();
    return;
  }
}

void proxyWebSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      proxyConnected = false;
      proxyAuthenticated = false;
      proxyLastHttpCode = 0;
      break;

    case WStype_CONNECTED:
      proxyConnected = true;
      proxyAuthenticated = false;
      proxyLastHttpCode = 101;
      proxySendAuth();
      break;

    case WStype_TEXT:
      if (payload && length > 0) proxyHandleTextMessage((const char*)payload, length);
      break;

    default:
      break;
  }
}

void proxyStartClient() {
  proxyEnsureIdentity();
  proxyWsClient.setReconnectInterval(PROXY_REMOTE_RETRY_MIN_MS);
  proxyWsClient.onEvent(proxyWebSocketEvent);

  if (bProxyUseTLS) proxyWsClient.beginSSL(settingProxyHost, settingProxyPort, settingProxyPath);
  else proxyWsClient.begin(settingProxyHost, settingProxyPort, settingProxyPath);

  proxyWsStarted = true;
  proxyConnected = false;
  proxyAuthenticated = false;
  proxyMode = PROXY_IDLE;
  proxyNeedDailyPush = false;
  proxyForceLivePush = false;
  proxyLastHttpCode = 0;
  proxyClearError();
}
#endif
}  // namespace

void ProxyRemoteSetup() {
#if REMOTE_PROXY
  proxyEnsureIdentity();
  proxyConnected = false;
  proxyAuthenticated = false;
  proxyLastHttpCode = 0;
  proxyLiveIntervalMs = PROXY_REMOTE_DEFAULT_LIVE_MS;
  proxyHeartbeatInterval = settingProxyInterval;
  proxyMode = PROXY_IDLE;
  proxyClearError();
  proxyForceLivePush = false;
#endif
}

void ProxyRemoteNotifyConfigChanged() {
#if REMOTE_PROXY
  proxyEnsureIdentity();
  if (proxyWsStarted) proxyWsClient.disconnect();
  proxyWsStarted = false;
  proxyConnected = false;
  proxyAuthenticated = false;
  proxyMode = PROXY_IDLE;
  proxyLastHttpCode = 0;
  proxyNeedDailyPush = false;
  proxyForceLivePush = false;
  proxyClearError();
#endif
}

void ProxyRemoteLoop() {
#if REMOTE_PROXY
  if (!bProxyEnabled || skipNetwork || netw_state == NW_NONE) {
    if (proxyWsStarted) proxyWsClient.disconnect();
    proxyWsStarted = false;
    proxyConnected = false;
    proxyAuthenticated = false;
    proxyMode = PROXY_IDLE;
    return;
  }

  if (!proxyWsStarted) proxyStartClient();
  proxyWsClient.loop();

  if (!proxyAuthenticated) return;

  const uint32_t nowMs = millis();
  if ((int32_t)(nowMs - proxyNextHeartbeatMs) >= 0) {
    proxySendHeartbeat();
    proxyNextHeartbeatMs = nowMs + (proxyHeartbeatInterval * 1000UL);
  }

  if (proxyMode == PROXY_LIVE) {
    if (proxyForceLivePush || (int32_t)(nowMs - proxyNextLiveMs) >= 0) {
      proxySendLive();
      proxyLastLiveTelegramCount = telegramCount;
      proxyForceLivePush = false;
      proxyNextLiveMs = nowMs + proxyLiveIntervalMs;
    }

    const uint32_t epochDay = proxyCurrentEpoch() / (3600UL * 24UL);
    if (proxyNeedDailyPush || proxyLastDailyEpochDay != epochDay) {
      proxySendDaily();
    }
  }
#endif
}

String proxyStatusJson() {
  JsonDocument doc;
#if REMOTE_PROXY
  doc["compiled"] = true;
  doc["enabled"] = bProxyEnabled;
  doc["connected"] = proxyConnected;
  doc["authenticated"] = proxyAuthenticated;
  doc["tls"] = bProxyUseTLS;
  doc["host"] = settingProxyHost;
  doc["port"] = settingProxyPort;
  doc["path"] = settingProxyPath;
  doc["interval"] = settingProxyInterval;
  doc["device_id"] = settingProxyDeviceId;
  doc["secret"] = settingProxySecret;
  doc["token_set"] = strlen(settingProxyToken) > 0;
  doc["mode"] = proxyModeName();
  doc["heartbeat_interval"] = proxyHeartbeatInterval;
  doc["live_interval_ms"] = proxyLiveIntervalMs;
  doc["last_success"] = proxyLastSuccess;
  doc["last_attempt"] = proxyLastAttempt;
  doc["last_http_code"] = proxyLastHttpCode;
  doc["sent_count"] = proxySentCount;
  doc["error_count"] = proxyErrorCount;
  doc["last_error"] = proxyLastError;
#else
  doc["compiled"] = false;
#endif

  String json;
  serializeJson(doc, json);
  return json;
}
