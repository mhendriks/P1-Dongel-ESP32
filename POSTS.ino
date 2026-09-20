/* Generic HTTP push connector; provisioned from post.json into NVS. */

static constexpr const char* POST_NVS = "http_post";
static constexpr const char* POST_FILE = "/post.json";

struct HttpPostSettings {
  uint8_t version, payload, auth;
  bool enabled, acceptInterval;
  uint16_t interval;
  char url[192], authKey[128], authName[48], extraHeaderName[48], extraHeaderValue[128];
};

static WiFiClientSecure webhookTlsClient;
static uint8_t webhookPostErrors = 0;
static uint32_t webhookLastPostMs = 0;
static bool webhookPostPending = false, webhookFailureLogged = false;
static int webhookLastHttpStatus = 0;
static time_t webhookLastSuccessfulPost = 0;

static bool savePostSettings() {
  HttpPostSettings s = { 1, settingHttpPostPayload, settingHttpPostAuth,
      bHttpPostEnabled, settingHttpPostAcceptInterval, settingHttpPostInterval };
  strlcpy(s.url, settingHttpPostUrl, sizeof(s.url));
  strlcpy(s.authKey, settingHttpPostAuthKey, sizeof(s.authKey));
  strlcpy(s.authName, settingHttpPostAuthName, sizeof(s.authName));
  strlcpy(s.extraHeaderName, settingHttpPostExtraHeaderName, sizeof(s.extraHeaderName));
  strlcpy(s.extraHeaderValue, settingHttpPostExtraHeaderValue, sizeof(s.extraHeaderValue));
  Preferences nvs;
  const bool saved = nvs.begin(POST_NVS, false) && nvs.putBytes("settings", &s, sizeof(s)) == sizeof(s);
  nvs.end();
  return saved;
}

static bool loadPostSettings() {
  HttpPostSettings s = {};
  Preferences nvs;
  const bool loaded = nvs.begin(POST_NVS, true) && nvs.getBytes("settings", &s, sizeof(s)) == sizeof(s) && s.version == 1;
  nvs.end();
  if (!loaded) return false;
  bHttpPostEnabled = s.enabled;
  settingHttpPostInterval = constrain(s.interval, 1, 3600);
  settingHttpPostPayload = constrain(s.payload, 0, 3);
  settingHttpPostAuth = constrain(s.auth, 0, 3);
  settingHttpPostAcceptInterval = s.acceptInterval;
  strlcpy(settingHttpPostUrl, s.url, sizeof(settingHttpPostUrl));
  strlcpy(settingHttpPostAuthKey, s.authKey, sizeof(settingHttpPostAuthKey));
  strlcpy(settingHttpPostAuthName, s.authName, sizeof(settingHttpPostAuthName));
  strlcpy(settingHttpPostExtraHeaderName, s.extraHeaderName, sizeof(settingHttpPostExtraHeaderName));
  strlcpy(settingHttpPostExtraHeaderValue, s.extraHeaderValue, sizeof(settingHttpPostExtraHeaderValue));
  return true;
}

static bool validPostUrl(const char* url) {
  return url && (!strncmp(url, "https://", 8) || !strncmp(url, "http://", 7));
}

void ProcessPostProvisioning() {
  loadPostSettings();
  if (!LittleFS.exists(POST_FILE)) return;
  File file = LittleFS.open(POST_FILE, "r");
  JsonDocument doc;
  if (!file || deserializeJson(doc, file) || !doc["enabled"].is<bool>()) {
    if (file) file.close();
    LogFile("HTTP POST: ongeldige post.json", true);
    return;
  }
  file.close();
  const bool enabled = doc["enabled"];
  const char* url = doc["url"] | "";
  if ((enabled && !validPostUrl(url)) || strlen(url) >= sizeof(settingHttpPostUrl)) {
    LogFile("HTTP POST: ongeldige URL", true);
    return;
  }
  bHttpPostEnabled = enabled;
  strlcpy(settingHttpPostUrl, url, sizeof(settingHttpPostUrl));
  settingHttpPostInterval = constrain(doc["interval"] | 300, 1, 3600);
  settingHttpPostPayload = constrain(doc["payload"] | 0, 0, 3);
  settingHttpPostAuth = constrain(doc["auth"] | 0, 0, 3);
  strlcpy(settingHttpPostAuthKey, doc["auth_key"] | "", sizeof(settingHttpPostAuthKey));
  strlcpy(settingHttpPostAuthName, doc["auth_name"] | "X-API-Key", sizeof(settingHttpPostAuthName));
  strlcpy(settingHttpPostExtraHeaderName, doc["extra_header_name"] | "", sizeof(settingHttpPostExtraHeaderName));
  strlcpy(settingHttpPostExtraHeaderValue, doc["extra_header_value"] | "", sizeof(settingHttpPostExtraHeaderValue));
  settingHttpPostAcceptInterval = doc["accept_interval"] | false;
  if (!savePostSettings()) {
    LogFile("HTTP POST: NVS-opslag mislukt", true);
    return;
  }
  LittleFS.remove(POST_FILE);
  LogFile("HTTP POST: post.json opgeslagen", true);
}

bool ImportLegacyMeentConfiguration(const char* webId, const char* apiKey, uint16_t interval) {
  if (!webId || !webId[0] || !apiKey || !apiKey[0] || loadPostSettings()) return false;
  bHttpPostEnabled = true;
  strlcpy(settingHttpPostUrl, "https://webhook.energiemeent.nl/api/data/", sizeof(settingHttpPostUrl));
  settingHttpPostInterval = constrain(interval, (uint16_t)1, (uint16_t)3600);
  settingHttpPostPayload = 1;
  settingHttpPostAuth = 1;
  strlcpy(settingHttpPostAuthKey, apiKey, sizeof(settingHttpPostAuthKey));
  strlcpy(settingHttpPostAuthName, "Authorization", sizeof(settingHttpPostAuthName));
  strlcpy(settingHttpPostExtraHeaderName, "X-MAC-Address", sizeof(settingHttpPostExtraHeaderName));
  strlcpy(settingHttpPostExtraHeaderValue, "{mac}", sizeof(settingHttpPostExtraHeaderValue));
  settingHttpPostAcceptInterval = true;
  return savePostSettings();
}

static bool httpPostConfigured() { return bHttpPostEnabled && strlen(settingHttpPostUrl) > 0; }
static uint32_t httpPostIntervalMs() { return (uint32_t)constrain(settingHttpPostInterval, (uint16_t)1, (uint16_t)3600) * 1000UL; }

static const char* httpPostStatusText() {
  if (!bHttpPostEnabled) return "uitgeschakeld";
  if (!strlen(settingHttpPostUrl)) return "onvolledig ingesteld";
  if (webhookLastHttpStatus >= 200 && webhookLastHttpStatus < 300) return "connected";
  if (webhookLastHttpStatus) return "fout";
  return "wacht op verzending";
}

static void logHttpPostResult(bool ok, int status) {
  if (ok) {
    if (webhookFailureLogged) LogFile("HTTP POST hersteld: verzending gelukt", true);
    webhookFailureLogged = false;
    return;
  }
  if (webhookFailureLogged) return;
  char message[64];
  if (status < 0) snprintf(message, sizeof(message), "HTTP POST fout: verbinding mislukt");
  else snprintf(message, sizeof(message), "HTTP POST fout: HTTP %d", status);
  LogFile(message, true);
  webhookFailureLogged = true;
}

void AppendHttpPostStatus(JsonDocument& doc) {
  doc["http_post_status"] = httpPostStatusText();
  doc["http_post_enabled"] = bHttpPostEnabled;
  if (webhookLastSuccessfulPost) doc["http_post_last_success"] = webhookLastSuccessfulPost;
  if (webhookLastHttpStatus) doc["http_post_http_status"] = webhookLastHttpStatus;
}

static String jsonWebhook(const WorkerWebhookPayload& p) {
  JsonDocument doc;
  char id[21]; snprintf(id, sizeof(id), "%llu", (unsigned long long)p.id);
  doc["id"] = id; doc["p_from_grid"] = p.pFromGrid; doc["p_to_grid"] = p.pToGrid; doc["timestamp"] = p.timestamp;
  if (settingHttpPostPayload == 1 || settingHttpPostPayload == 3) {
    doc["t1"] = p.t1; doc["t2"] = p.t2; doc["t1r"] = p.t1r; doc["t2r"] = p.t2r;
  }
  if (settingHttpPostPayload == 2 || settingHttpPostPayload == 3) {
    const char* v[] = { "v_l1", "v_l2", "v_l3" };
    const char* sag[] = { "voltage_sag_l1_count", "voltage_sag_l2_count", "voltage_sag_l3_count" };
    const char* swell[] = { "voltage_swell_l1_count", "voltage_swell_l2_count", "voltage_swell_l3_count" };
    for (uint8_t i = 0; i < 3; i++) {
      if (p.voltagePresentMask & (1U << i)) doc[v[i]] = p.voltage[i];
      if (p.sagSwellPresentMask & (1U << i)) doc[sag[i]] = p.voltageSags[i];
      if (p.sagSwellPresentMask & (1U << (i + 3))) doc[swell[i]] = p.voltageSwells[i];
    }
  }
  if (settingHttpPostAuth == 3 && strlen(settingHttpPostAuthName) && strlen(settingHttpPostAuthKey)) doc[settingHttpPostAuthName] = settingHttpPostAuthKey;
  String output; serializeJson(doc, output); return output;
}

static void applyServerInterval(const String& response) {
  if (!settingHttpPostAcceptInterval) return;
  JsonDocument doc;
  const int interval = deserializeJson(doc, response) ? 0 : doc["interval"] | 0;
  if (interval >= 1 && interval <= 3600 && interval != settingHttpPostInterval) {
    settingHttpPostInterval = interval;
    savePostSettings();
  }
}

void PostWebhook() {
  if (!bNewTelegramWebhook || netw_state == NW_NONE || webhookPostPending || !httpPostConfigured()) return;
  if (webhookLastPostMs && (uint32_t)(millis() - webhookLastPostMs) < httpPostIntervalMs()) return;
  WorkerWebhookPayload p = {};
  p.id = _getChipId(); p.pFromGrid = outputPowerInt(DSMRdata.power_delivered.int_val()); p.pToGrid = outputPowerInt(DSMRdata.power_returned.int_val()); p.timestamp = actT;
  p.t1 = DSMRdata.energy_delivered_tariff1_present ? outputEnergyUint64(DSMRdata.energy_delivered_tariff1.int_val()) : 0;
  p.t2 = DSMRdata.energy_delivered_tariff2_present ? outputEnergyUint64(DSMRdata.energy_delivered_tariff2.int_val()) : 0;
  p.t1r = DSMRdata.energy_returned_tariff1_present ? outputEnergyUint64(DSMRdata.energy_returned_tariff1.int_val()) : 0;
  p.t2r = DSMRdata.energy_returned_tariff2_present ? outputEnergyUint64(DSMRdata.energy_returned_tariff2.int_val()) : 0;
  if (DSMRdata.voltage_l1_present) { p.voltage[0] = (uint32_t)lroundf(outputVoltage(DSMRdata.voltage_l1.val()) * 10); p.voltagePresentMask |= 1; }
  if (DSMRdata.voltage_l2_present) { p.voltage[1] = (uint32_t)lroundf(outputVoltage(DSMRdata.voltage_l2.val()) * 10); p.voltagePresentMask |= 2; }
  if (DSMRdata.voltage_l3_present) { p.voltage[2] = (uint32_t)lroundf(outputVoltage(DSMRdata.voltage_l3.val()) * 10); p.voltagePresentMask |= 4; }
  if (DSMRdata.electricity_sags_l1_present) { p.voltageSags[0] = DSMRdata.electricity_sags_l1; p.sagSwellPresentMask |= 1; }
  if (DSMRdata.electricity_sags_l2_present) { p.voltageSags[1] = DSMRdata.electricity_sags_l2; p.sagSwellPresentMask |= 2; }
  if (DSMRdata.electricity_sags_l3_present) { p.voltageSags[2] = DSMRdata.electricity_sags_l3; p.sagSwellPresentMask |= 4; }
  if (DSMRdata.electricity_swells_l1_present) { p.voltageSwells[0] = DSMRdata.electricity_swells_l1; p.sagSwellPresentMask |= 8; }
  if (DSMRdata.electricity_swells_l2_present) { p.voltageSwells[1] = DSMRdata.electricity_swells_l2; p.sagSwellPresentMask |= 16; }
  if (DSMRdata.electricity_swells_l3_present) { p.voltageSwells[2] = DSMRdata.electricity_swells_l3; p.sagSwellPresentMask |= 32; }
  if (!WorkerEnqueueWebhookPost(p)) return;
  bNewTelegramWebhook = false; webhookPostPending = true;
}

void PostWebhookFromWorker(const WorkerWebhookPayload& p) {
  if (netw_state == NW_NONE || !httpPostConfigured()) { webhookPostPending = false; return; }
  HTTPClient http;
  if (!http.begin(webhookTlsClient, settingHttpPostUrl)) webhookLastHttpStatus = -1;
  else {
    http.setTimeout(15000); http.addHeader("Content-Type", "application/json");
    if (settingHttpPostAuth == 1 && strlen(settingHttpPostAuthKey)) http.addHeader("Authorization", "Bearer " + String(settingHttpPostAuthKey));
    else if (settingHttpPostAuth == 2 && strlen(settingHttpPostAuthName) && strlen(settingHttpPostAuthKey)) http.addHeader(settingHttpPostAuthName, settingHttpPostAuthKey);
    if (strlen(settingHttpPostExtraHeaderName) && strlen(settingHttpPostExtraHeaderValue)) { String v = settingHttpPostExtraHeaderValue; v.replace("{mac}", macID); http.addHeader(settingHttpPostExtraHeaderName, v); }
    webhookLastHttpStatus = http.POST(jsonWebhook(p));
    const String response = settingHttpPostAcceptInterval ? http.getString() : "";
    http.end();
    if (webhookLastHttpStatus >= 200 && webhookLastHttpStatus < 300) applyServerInterval(response);
  }
  webhookLastPostMs = millis();
  const bool ok = webhookLastHttpStatus >= 200 && webhookLastHttpStatus < 300;
  if (ok) { webhookPostErrors = 0; webhookLastSuccessfulPost = p.timestamp; }
  else { webhookPostErrors++; webhookTlsClient.stop(); }
  logHttpPostResult(ok, webhookLastHttpStatus);
  webhookPostPending = false;
}

void StartWebhook() { webhookTlsClient.setInsecure(); webhookTlsClient.setTimeout(15000); }
