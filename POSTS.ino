#if __has_include("./../../_secrets/posts.h")
  #include "./../../_secrets/posts.h"
#endif

// These labels retain the well-tested protocol helpers below. They are local
// to this translation unit and do not select a build profile; actual provider
// selection is `settingHttpPostProvider`, provisioned at runtime.
#define POST_KEMP
#define POST_MEENT

#ifndef URL_POWERCH
  #define URL_POWERCH ""
#endif
#ifndef URL_KEMP
  #define URL_KEMP ""
#endif
#ifndef KEMP_API_KEY
  #define KEMP_API_KEY ""
#endif
#ifndef OTAURL_PREFIX
  #define OTAURL_PREFIX ""
#endif
// MEENT provisioning is always compiled in; it only starts after the user
// selects MEENT in the settings screen.
#ifndef MEENT_API_BASE_URL
  #ifdef DEBUG
    #define MEENT_API_BASE_URL "https://meent.dev.muze.nl/api/"
  #else
    #define MEENT_API_BASE_URL "https://webhook.energiemeent.nl/api/"
  #endif
#endif
#ifndef MEENT_CLIENT_SECRET_HEADER
  #define MEENT_CLIENT_SECRET_HEADER "X-Client-Secret"
#endif

static WiFiClientSecure webhookTlsClient;
#ifdef DEBUG
static WiFiClient webhookPlainClient;
#endif
uint32_t webhookPostErrors = 0;
uint32_t webhookLastPostMs = 0;
bool webhookPostPending = false;
static int webhookLastHttpStatus = 0;
static time_t webhookLastSuccessfulPost = 0;

static constexpr const char* POST_NVS = "http_post";
static constexpr const char* POST_FILE = "/post.json";
struct HttpPostSettings {
  uint8_t version, provider, payload, auth;
  bool enabled, acceptInterval;
  uint16_t interval;
  char url[192], authKey[128], authName[48], extraHeaderName[48], extraHeaderValue[128];
};

static bool savePostSettings() {
  HttpPostSettings s = { 2, settingHttpPostProvider, settingHttpPostPayload,
      settingHttpPostAuth, bHttpPostEnabled, settingHttpPostAcceptInterval,
      settingHttpPostInterval };
  strlcpy(s.url, settingHttpPostUrl, sizeof(s.url));
  strlcpy(s.authKey, settingHttpPostAuthKey, sizeof(s.authKey));
  strlcpy(s.authName, settingHttpPostAuthName, sizeof(s.authName));
  strlcpy(s.extraHeaderName, settingHttpPostExtraHeaderName, sizeof(s.extraHeaderName));
  strlcpy(s.extraHeaderValue, settingHttpPostExtraHeaderValue, sizeof(s.extraHeaderValue));
  Preferences nvs;
  const bool saved = nvs.begin(POST_NVS, false) &&
      nvs.putBytes("settings", &s, sizeof(s)) == sizeof(s);
  nvs.end();
  return saved;
}

static bool loadPostSettings() {
  HttpPostSettings s = {};
  Preferences nvs;
  const bool loaded = nvs.begin(POST_NVS, true) &&
      nvs.getBytes("settings", &s, sizeof(s)) == sizeof(s) && s.version == 2;
  nvs.end();
  if (!loaded) return false;
  bHttpPostEnabled = s.enabled;
  settingHttpPostProvider = constrain(s.provider, HTTP_POST_GENERIC, HTTP_POST_MEENT);
  settingHttpPostInterval = constrain(s.interval, 1, 3600);
  settingHttpPostPayload = constrain(s.payload, 0, 3);
  settingHttpPostAuth = constrain(s.auth, 0, 3);
  settingHttpPostAcceptInterval = s.acceptInterval;
  strlcpy(settingHttpPostUrl, s.url, sizeof(settingHttpPostUrl));
  strlcpy(settingHttpPostAuthKey, s.authKey, sizeof(settingHttpPostAuthKey));
  strlcpy(settingHttpPostAuthName, s.authName, sizeof(settingHttpPostAuthName));
  strlcpy(settingHttpPostExtraHeaderName, s.extraHeaderName, sizeof(settingHttpPostExtraHeaderName));
  strlcpy(settingHttpPostExtraHeaderValue, s.extraHeaderValue, sizeof(settingHttpPostExtraHeaderValue));
  // The active runtime provider is authoritative. Do not revive an old
  // MEENT selection from settings after a KEMP or generic post.json was used.
  bMeentEnabled = bHttpPostEnabled && settingHttpPostProvider == HTTP_POST_MEENT;
  return true;
}

static bool validPostUrl(const char* url) {
  if (!url) return false;
#ifdef DEBUG
  return !strncmp(url, "https://", 8) || !strncmp(url, "http://", 7);
#else
  return !strncmp(url, "https://", 8);
#endif
}

// A local plain-HTTP mock is available only in a DEBUG build. Production
// endpoints are HTTPS-only and always use the TLS transport.
static bool webhookHttpBegin(HTTPClient& http, const char* url) {
  if (!validPostUrl(url)) return false;
#ifdef DEBUG
  if (!strncmp(url, "http://", 7)) return http.begin(webhookPlainClient, url);
#endif
  return http.begin(webhookTlsClient, url);
}

void ProcessPostProvisioning() {
  const bool loaded = loadPostSettings();
  if (!LittleFS.exists(POST_FILE)) {
    // MEENT is selected only by an active `provider: meent` post.json/NVS
    // configuration; legacy settings must not make its UI reappear.
    if (!loaded) bMeentEnabled = false;
    return;
  }
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
  const char* provider = doc["provider"] | "generic";
  uint8_t parsedProvider = HTTP_POST_GENERIC;
  if (!strcmp(provider, "kemp")) parsedProvider = HTTP_POST_KEMP;
  else if (!strcmp(provider, "meent")) parsedProvider = HTTP_POST_MEENT;
  else if (strcmp(provider, "generic")) { LogFile("HTTP POST: ongeldige provider", true); return; }
  bHttpPostEnabled = enabled;
  settingHttpPostProvider = parsedProvider;
  strlcpy(settingHttpPostUrl, url, sizeof(settingHttpPostUrl));
  settingHttpPostInterval = constrain(doc["interval"] | 300, 1, 3600);
  settingHttpPostPayload = constrain(doc["payload"] | 0, 0, 3);
  settingHttpPostAuth = constrain(doc["auth"] | 0, 0, 3);
  strlcpy(settingHttpPostAuthKey, doc["auth_key"] | "", sizeof(settingHttpPostAuthKey));
  strlcpy(settingHttpPostAuthName, doc["auth_name"] | "X-API-Key", sizeof(settingHttpPostAuthName));
  strlcpy(settingHttpPostExtraHeaderName, doc["extra_header_name"] | "", sizeof(settingHttpPostExtraHeaderName));
  strlcpy(settingHttpPostExtraHeaderValue, doc["extra_header_value"] | "", sizeof(settingHttpPostExtraHeaderValue));
  settingHttpPostAcceptInterval = doc["accept_interval"] | false;
  bMeentEnabled = parsedProvider == HTTP_POST_MEENT && enabled;
  if (!savePostSettings()) { LogFile("HTTP POST: NVS-opslag mislukt", true); return; }
  writeSettingsDirect(); // persist the MEENT selection shown in the UI
  LittleFS.remove(POST_FILE);
  LogFile("HTTP POST: post.json opgeslagen", true);
}

static bool httpPostConfigured() { return bHttpPostEnabled && strlen(settingHttpPostUrl); }

void AppendHttpPostStatus(JsonDocument& doc) {
  doc["http_post_enabled"] = bHttpPostEnabled;
  if (bHttpPostEnabled) doc["http_post_interval_s"] = settingHttpPostInterval;
  doc["http_post_provider"] = settingHttpPostProvider == HTTP_POST_KEMP ? "kemp" :
      (settingHttpPostProvider == HTTP_POST_MEENT ? "meent" : "generic");
  if (!bHttpPostEnabled) doc["http_post_status"] = "uitgeschakeld";
  else if (webhookLastHttpStatus >= 200 && webhookLastHttpStatus < 300) doc["http_post_status"] = "verbonden";
  else if (webhookLastHttpStatus < 0) doc["http_post_status"] = "verbindingsfout";
  else if (webhookLastHttpStatus) {
    char status[20];
    snprintf(status, sizeof(status), "HTTP %d", webhookLastHttpStatus);
    doc["http_post_status"] = status;
  } else doc["http_post_status"] = "wacht op verzending";
  if (webhookLastHttpStatus) doc["http_post_http_status"] = webhookLastHttpStatus;
  if (webhookLastSuccessfulPost) doc["http_post_last_success"] = webhookLastSuccessfulPost;
}

#ifdef POST_KEMP
// Keep the normal log useful during an outage: report the first failed push
// and the eventual recovery, rather than every 60-second retry.
static bool kempPushFailed = false;
static constexpr const char* KEMP_NVS_NAMESPACE = "kemp_post";
static constexpr uint16_t KEMP_DEFAULT_INTERVAL_S = 60;
static char kempApiKey[129] = "";
static char kempUrl[192] = "";
static uint16_t kempIntervalS = KEMP_DEFAULT_INTERVAL_S;
static bool kempStartupConfigPending = false;
static bool kempStartupConfigQueued = false;

static void kempLoadConnectionSettings() {
  strlcpy(kempApiKey, settingHttpPostAuthKey, sizeof(kempApiKey));
  strlcpy(kempUrl, settingHttpPostUrl, sizeof(kempUrl));
  kempIntervalS = settingHttpPostInterval;
}

static bool kempSaveConnectionSetting(const char* key, const char* value) {
  if (!strcmp(key, "api_key")) strlcpy(settingHttpPostAuthKey, value ? value : "", sizeof(settingHttpPostAuthKey));
  else if (!strcmp(key, "url")) strlcpy(settingHttpPostUrl, value ? value : "", sizeof(settingHttpPostUrl));
  else return false;
  return savePostSettings();
}

static bool kempIsValidUrl(const char* url) {
  return url && !strncmp(url, "https://", 8) && strlen(url) < sizeof(kempUrl);
}

static const char* kempHardwareName() {
  switch (HardwareType) {
    case P1P: return "P1P";
    case NRGD: return "NRGD";
    case P1E: return "P1E";
    case P1EP: return "P1EP";
    case P1UM: return "P1UM";
    case P1U: return "P1U";
    case NRGM: return "NRGM";
    case P1S: return "P1S";
    case P1UX2: return "P1UX2";
    case NRGDH: return "NRGDH";
    case D1MC: return "D1MC";
    case W1MC: return "W1MC";
    default: return "UNKNOWN";
  }
}

static String kempResetReason() {
  const int start = lastReset.indexOf("ESP_RST_");
  if (start < 0) return lastReset;
  const int end = lastReset.indexOf(" -", start);
  return end < 0 ? lastReset.substring(start) : lastReset.substring(start, end);
}

static void kempLogPushFailure(const char* detail) {
  if (kempPushFailed) return;

  char message[96];
  snprintf(message, sizeof(message), "KEMP: push failed (%s); retrying every 60 sec", detail);
  LogFile(message, true);
  kempPushFailed = true;
}

static void kempLogPushRecovered() {
  if (!kempPushFailed) return;

  LogFile("KEMP: push recovered", true);
  kempPushFailed = false;
}
#endif

String JsonWebhook(const WorkerWebhookPayload& payload) {
  
  JsonDocument doc;
  if (settingHttpPostProvider == HTTP_POST_KEMP) {
    doc["message_type"] = "sample";
    doc["api_key"] = kempApiKey;
  }
  if (settingHttpPostProvider == HTTP_POST_GENERIC && settingHttpPostAuth == 3 &&
      strlen(settingHttpPostAuthName) && strlen(settingHttpPostAuthKey)) {
    doc[settingHttpPostAuthName] = settingHttpPostAuthKey;
  }
  char idbuf[21];
  snprintf(idbuf, sizeof(idbuf), "%llu", (unsigned long long)payload.id);
  doc["id"] = idbuf;
  doc["p_from_grid"] = payload.pFromGrid;
  doc["p_to_grid"] = payload.pToGrid;
  if (settingHttpPostProvider == HTTP_POST_MEENT || settingHttpPostProvider == HTTP_POST_KEMP ||
      settingHttpPostPayload == 1 || settingHttpPostPayload == 3) {
    doc["t1"] = payload.t1; doc["t2"] = payload.t2;
    doc["t1r"] = payload.t1r; doc["t2r"] = payload.t2r;
  }
  if (settingHttpPostProvider == HTTP_POST_KEMP || settingHttpPostPayload == 2 || settingHttpPostPayload == 3) {
  const char* voltageKeys[] = { "v_l1", "v_l2", "v_l3" };
  const char* sagKeys[] = {
    "voltage_sag_l1_count", "voltage_sag_l2_count", "voltage_sag_l3_count"
  };
  const char* swellKeys[] = {
    "voltage_swell_l1_count", "voltage_swell_l2_count", "voltage_swell_l3_count"
  };
  for (uint8_t phase = 0; phase < 3; phase++) {
    if (payload.voltagePresentMask & (1U << phase)) doc[voltageKeys[phase]] = payload.voltage[phase];
    if (payload.sagSwellPresentMask & (1U << phase)) doc[sagKeys[phase]] = payload.voltageSags[phase];
    if (payload.sagSwellPresentMask & (1U << (phase + 3))) doc[swellKeys[phase]] = payload.voltageSwells[phase];
  }
  }
  doc["timestamp"] = payload.timestamp; // int of string, beide ok

  String output;
  serializeJson(doc, output);

#ifdef DEBUG
  Debugf("Webhook Json: %s\n", output.c_str());
#endif  

  return output;
}

#ifdef POST_KEMP
static String kempConfigJson(const char* ack = nullptr, const char* commandId = nullptr,
                             bool success = true, const char* msg = nullptr,
                             const char* requestedFirmwareVersion = nullptr) {
  JsonDocument doc;
  doc["message_type"] = "config";
  char idbuf[21];
  snprintf(idbuf, sizeof(idbuf), "%llu", (unsigned long long)_getChipId());
  doc["id"] = idbuf;
  doc["api_key"] = kempApiKey;
  if (ack && *ack) {
    doc["ack"] = ack;
    if (commandId && *commandId) doc["command_id"] = commandId;
    doc["success"] = success;
    if (msg && *msg) doc["msg"] = msg;
  }
  if (requestedFirmwareVersion && *requestedFirmwareVersion) {
    doc["requested_firmware_version"] = requestedFirmwareVersion;
  }
  doc["firmware_version"] = _VERSION_ONLY;
  doc["hardware"] = kempHardwareName();
  if (smID.length()) doc["smart_meter"] = smID;
  doc["uptime_s"] = millis() / 1000UL;
  doc["reboot_count"] = P1Status.reboots;
  doc["last_reset_reason"] = kempResetReason();
  doc["p1_count"] = telegramCount;
  doc["p1_error"] = telegramErrors;
  doc["interval_s"] = kempIntervalS;
  String output;
  serializeJson(doc, output);
  return output;
}

static bool kempPostJson(const char* url, const char* apiKey, const String& body, String* response = nullptr) {
  if (!url || !*url || !apiKey || !*apiKey) { webhookLastHttpStatus = -1; return false; }
  HTTPClient http;
  if (!webhookHttpBegin(http, url)) { webhookLastHttpStatus = -1; return false; }
  http.setTimeout(15000);
  http.addHeader("Content-Type", "application/json");
  const int status = http.POST(body);
  webhookLastHttpStatus = status;
  if (response) *response = http.getString();
  http.end();
  if (status >= 200 && status < 300) webhookLastSuccessfulPost = actT ? actT : time(nullptr);
  return status == HTTP_CODE_OK;
}

static bool kempSendConfig(const char* url, const char* apiKey, const char* ack = nullptr,
                           const char* commandId = nullptr, bool success = true,
                           const char* msg = nullptr, const char* requestedFirmwareVersion = nullptr) {
  const String currentKey(kempApiKey);
  // Config can validate a candidate API key. Do not alter the active key unless
  // the server confirms this very request with HTTP 200.
  if (apiKey && strcmp(apiKey, kempApiKey)) strlcpy(kempApiKey, apiKey, sizeof(kempApiKey));
  const String body = kempConfigJson(ack, commandId, success, msg, requestedFirmwareVersion);
  const bool ok = kempPostJson(url, apiKey ? apiKey : kempApiKey, body);
  strlcpy(kempApiKey, currentKey.c_str(), sizeof(kempApiKey));
  return ok;
}

static void kempApplyServerResponse(const String& response) {
  JsonDocument doc;
  if (deserializeJson(doc, response)) return;

  // `interval` is accepted during the server migration, but the agreed
  // contract sends `interval_s` on every successful sample response.
  const int interval = doc["interval_s"].is<int>()
      ? doc["interval_s"].as<int>()
      : (doc["interval"] | 0);
  if (interval >= 1 && interval <= 3600) kempIntervalS = (uint16_t)interval;

  const char* command = doc["command"] | "";
  const char* commandId = doc["command_id"] | "";
  if (!*command) return;

  if (!strcmp(command, "REQconfig")) {
    kempSendConfig(kempUrl, kempApiKey, command, commandId, true, "configuration sent");
  } else if (!strcmp(command, "APIupdate")) {
    const char* candidate = doc["api_key_new"] | "";
    if (strlen(candidate) && strlen(candidate) < sizeof(kempApiKey) &&
        kempSendConfig(kempUrl, candidate, command, commandId, true, "api-key verified")) {
      if (kempSaveConnectionSetting("api_key", candidate)) strlcpy(kempApiKey, candidate, sizeof(kempApiKey));
    }
  } else if (!strcmp(command, "URLupdate")) {
    const char* candidate = doc["url_new"] | "";
    if (kempIsValidUrl(candidate) &&
        kempSendConfig(candidate, kempApiKey, command, commandId, true, "upload-url verified")) {
      if (kempSaveConnectionSetting("url", candidate)) strlcpy(kempUrl, candidate, sizeof(kempUrl));
    }
  } else if (!strcmp(command, "OTAupdate")) {
    const char* version = doc["firmware_version"] | "";
    String error;
    if (!RemoteUpdateAvailable(version, &error)) {
      kempSendConfig(kempUrl, kempApiKey, command, commandId, false,
                     error.length() ? error.c_str() : "requested firmware version unavailable", version);
    } else if (kempSendConfig(kempUrl, kempApiKey, command, commandId, true, "ota update started", version)) {
      RemoteUpdateNow(version, true, &error);
    }
  } else if (!strcmp(command, "REBOOT")) {
    if (kempSendConfig(kempUrl, kempApiKey, command, commandId, true, "rebooting")) P1Reboot();
  } else {
    DebugTf("KEMP: unsupported server command [%s]\r\n", command);
  }
}

static void kempQueueStartupConfig() {
  if (!kempStartupConfigPending || kempStartupConfigQueued || netw_state == NW_NONE) return;
  if (WorkerEnqueueSimple(WORKER_JOB_KEMP_CONFIG, WORKER_PRIO_NORMAL)) kempStartupConfigQueued = true;
}

void KempStartupConfigFromWorker() {
  kempStartupConfigQueued = false;
  if (netw_state == NW_NONE || !kempStartupConfigPending) return;
  if (kempSendConfig(kempUrl, kempApiKey)) kempStartupConfigPending = false;
}
#else
void KempStartupConfigFromWorker() {}
#endif

time_t webhookStartMs;

#ifdef POST_MEENT
enum MeentStepState : uint8_t {
  MEENT_NOT_SET, MEENT_STORED, MEENT_IN_PROGRESS, MEENT_OK, MEENT_ERROR, MEENT_BLOCKED
};

static MeentStepState meentWebIdState = MEENT_NOT_SET;
static MeentStepState meentApiKeyState = MEENT_NOT_SET;
static MeentStepState meentDataState = MEENT_NOT_SET;
static int meentWebIdHttpStatus = 0;
static int meentApiKeyHttpStatus = 0;
static int meentDataHttpStatus = 0;
static time_t meentLastSuccessfulPost = 0;
static uint32_t meentNextProvisionAttemptMs = 0;
static bool meentProvisionPending = false;
static bool meentProvisionBlocked = false;
static constexpr const char* MEENT_CLIENT_SECRET_NVS_KEY = "meent_secret";
static constexpr size_t MEENT_CLIENT_SECRET_BYTES = 32;
static char meentClientSecret[MEENT_CLIENT_SECRET_BYTES * 2 + 1] = "";

static bool meentClientSecretIsValid(const char* value) {
  if (!value || strlen(value) != MEENT_CLIENT_SECRET_BYTES * 2) return false;
  for (const char* p = value; *p; ++p) {
    if (!isxdigit((unsigned char)*p)) return false;
  }
  return true;
}

// The secret is saved before the first provisioning request. It intentionally
// lives outside the LittleFS settings file, so a settings-file failure cannot
// orphan a pod or API key. Factory reset explicitly removes this credential.
static bool meentEnsureClientSecret() {
  if (meentClientSecretIsValid(meentClientSecret)) return true;

  preferences.getString(MEENT_CLIENT_SECRET_NVS_KEY, meentClientSecret, sizeof(meentClientSecret));
  if (meentClientSecretIsValid(meentClientSecret)) return true;

  static const char hex[] = "0123456789abcdef";
  for (size_t i = 0; i < MEENT_CLIENT_SECRET_BYTES; ++i) {
    const uint8_t value = (uint8_t)esp_random();
    meentClientSecret[i * 2] = hex[value >> 4];
    meentClientSecret[i * 2 + 1] = hex[value & 0x0F];
  }
  meentClientSecret[MEENT_CLIENT_SECRET_BYTES * 2] = '\0';

  if (preferences.putString(MEENT_CLIENT_SECRET_NVS_KEY, meentClientSecret) != MEENT_CLIENT_SECRET_BYTES * 2) {
    meentClientSecret[0] = '\0';
    DebugTln(F("MEENT: client-secret NVS write failed"));
    return false;
  }
  return true;
}

static String meentApiKey() {
  String key = settingMeentApiKey;
  key.trim();
  // Accept an older/manual "Bearer <key>" value, but store and send a key.
  if (key.startsWith("Bearer ") || key.startsWith("bearer ")) key.remove(0, 7);
  key.trim();
  return key;
}

static uint32_t meentIntervalMs() {
  return (uint32_t)constrain(settingMeentInterval, (uint16_t)1, (uint16_t)3600) * 1000UL;
}

static String meentStatusText(uint8_t state, int httpStatus) {
  switch (state) {
    case MEENT_NOT_SET: return "niet ingesteld";
    // The key/WebID is available locally. Keep this short so the last data
    // timestamp remains readable in the System information card.
    case MEENT_STORED: return "OK";
    case MEENT_IN_PROGRESS: return "bezig";
    case MEENT_OK: return "OK";
    case MEENT_BLOCKED: return "fout (HTTP " + String(httpStatus) + ", actie nodig)";
    case MEENT_ERROR:
      if (httpStatus == -1) return "fout (verbinding)";
      if (httpStatus == -2) return "fout (ongeldig antwoord)";
      if (httpStatus == -3) return "fout (client-secret opslag)";
      return "fout (HTTP " + String(httpStatus) + ")";
  }
  return "onbekend";
}

// A conflict means the current provider cannot recover this provisioning
// state. Other responses, including a transitional 401 while the provider is
// being updated, are retried at the configured interval.
static bool meentProvisionFailureNeedsAction(int httpStatus) {
  return httpStatus == HTTP_CODE_CONFLICT;
}

static void meentResetStatus() {
  meentWebIdState = strlen(settingMeentWebId) ? MEENT_STORED : MEENT_NOT_SET;
  meentApiKeyState = meentApiKey().length() ? MEENT_STORED : MEENT_NOT_SET;
  meentDataState = MEENT_NOT_SET;
  meentWebIdHttpStatus = meentApiKeyHttpStatus = meentDataHttpStatus = 0;
  meentLastSuccessfulPost = 0;
  meentNextProvisionAttemptMs = 0;
  meentProvisionPending = false;
  meentProvisionBlocked = false;
}

static bool meentPostText(const String& url, const String& request, String& response, int& status) {
  HTTPClient http;
  http.setTimeout(10000); // Keep failed internet routes from delaying provisioning too long.
  if (!http.begin(webhookTlsClient, url)) {
    status = -1;
    return false;
  }
  http.addHeader("Content-Type", "text/plain");
  // Recovery credential: expose only in explicitly enabled Verbose 2 logs.
  DebugTraceTf("MEENT client-secret: %s\r\n", meentClientSecret);
  http.addHeader(MEENT_CLIENT_SECRET_HEADER, meentClientSecret);
  status = http.POST(request);
  response = http.getString();
  http.end();
  DebugTf("MEENT provisioning response: %d\r\n", status);
  // A fresh provisioning response is normally 201; an idempotent recovery
  // response from the provider may correctly be 200 instead.
  return status >= 200 && status < 300;
}

static bool meentProvision() {
  String response;
  // Reserve the next slot before doing any network I/O. A failed request must
  // never cause another provisioning request for every incoming telegram.
  meentNextProvisionAttemptMs = millis() + meentIntervalMs();
  if (!meentEnsureClientSecret()) {
    if (!strlen(settingMeentWebId)) {
      meentWebIdState = MEENT_ERROR;
      meentWebIdHttpStatus = -3;
    } else {
      meentApiKeyState = MEENT_ERROR;
      meentApiKeyHttpStatus = -3;
    }
    return false;
  }
  if (!strlen(settingMeentWebId)) {
    meentWebIdState = MEENT_IN_PROGRESS;
    if (!meentPostText(String(MEENT_API_BASE_URL) + "pod/", macID, response, meentWebIdHttpStatus)) {
      meentWebIdState = meentProvisionFailureNeedsAction(meentWebIdHttpStatus) ? MEENT_BLOCKED : MEENT_ERROR;
      meentProvisionBlocked = meentWebIdState == MEENT_BLOCKED;
      DebugTln(F("MEENT: WebID creation failed"));
      return false;
    }
    JsonDocument doc;
    if (deserializeJson(doc, response) || !doc["data"]["webid"].is<const char*>()) {
      meentWebIdHttpStatus = -2;
      meentWebIdState = MEENT_ERROR;
      DebugTln(F("MEENT: WebID missing from response"));
      return false;
    }
    strCopy(settingMeentWebId, sizeof(settingMeentWebId), doc["data"]["webid"].as<const char*>());
    writeSettingsDirect(); // Persist each provisioning step immediately.
    meentWebIdState = MEENT_OK;
    DebugTln(F("MEENT: WebID saved"));
  }

  if (!meentApiKey().length()) {
    meentApiKeyState = MEENT_IN_PROGRESS;
    if (!meentPostText(String(MEENT_API_BASE_URL) + "register/", settingMeentWebId, response, meentApiKeyHttpStatus)) {
      meentApiKeyState = meentProvisionFailureNeedsAction(meentApiKeyHttpStatus) ? MEENT_BLOCKED : MEENT_ERROR;
      meentProvisionBlocked = meentApiKeyState == MEENT_BLOCKED;
      DebugTln(F("MEENT: API-key registration failed"));
      return false;
    }
    JsonDocument doc;
    if (deserializeJson(doc, response) || !doc["data"]["api_key"].is<const char*>()) {
      meentApiKeyHttpStatus = -2;
      meentApiKeyState = MEENT_ERROR;
      DebugTln(F("MEENT: API key missing from response"));
      return false;
    }
    strCopy(settingMeentApiKey, sizeof(settingMeentApiKey), doc["data"]["api_key"].as<const char*>());
    writeSettingsDirect(); // Never lose a key that cannot be requested again.
    meentApiKeyState = MEENT_OK;
    DebugTln(F("MEENT: API key saved"));
  }
  // MEENT uses the same generic connection store as every other provider.
  // Its provider-specific part ends with obtaining the WebID/API key.
  bHttpPostEnabled = true;
  settingHttpPostProvider = HTTP_POST_MEENT;
  const String dataUrl = String(MEENT_API_BASE_URL) + "data/";
  strlcpy(settingHttpPostUrl, dataUrl.c_str(), sizeof(settingHttpPostUrl));
  settingHttpPostInterval = settingMeentInterval;
  settingHttpPostPayload = 1;
  settingHttpPostAuth = 1;
  strlcpy(settingHttpPostAuthKey, meentApiKey().c_str(), sizeof(settingHttpPostAuthKey));
  strlcpy(settingHttpPostAuthName, "Authorization", sizeof(settingHttpPostAuthName));
  strlcpy(settingHttpPostExtraHeaderName, "X-MAC-Address", sizeof(settingHttpPostExtraHeaderName));
  strlcpy(settingHttpPostExtraHeaderValue, "{mac}", sizeof(settingHttpPostExtraHeaderValue));
  settingHttpPostAcceptInterval = true;
  if (!savePostSettings()) DebugTln(F("MEENT: HTTP POST settings not saved"));
  return true;
}

static void meentApplyServerInterval(const String& response) {
  JsonDocument doc;
  if (deserializeJson(doc, response) || !doc["interval"].is<int>()) return;

  const int requestedInterval = doc["interval"].as<int>();
  if (requestedInterval < 1 || requestedInterval > 3600) {
    DebugTf("MEENT: ignoring invalid server interval %d\r\n", requestedInterval);
    return;
  }
  const uint16_t serverInterval = (uint16_t)requestedInterval;
  if (serverInterval == settingMeentInterval) return;

  settingMeentInterval = serverInterval;
  settingHttpPostInterval = serverInterval;
  savePostSettings();
  writeSettingsDirect(); // This runs in the worker, so persist the server policy directly.
  DebugTf("MEENT: interval updated by server to %u seconds\r\n", serverInterval);
}
#endif

void PostWebhook() {
  if (netw_state == NW_NONE) return;
  if (settingHttpPostProvider == HTTP_POST_KEMP && httpPostConfigured()) kempQueueStartupConfig();
  if (!bNewTelegramWebhook || webhookPostPending || (!httpPostConfigured() && !bMeentEnabled)) return;
  const uint32_t webhookIntervalMs = settingHttpPostProvider == HTTP_POST_KEMP
      ? (uint32_t)kempIntervalS * 1000UL
      : (settingHttpPostProvider == HTTP_POST_MEENT ? meentIntervalMs()
                                                    : (uint32_t)settingHttpPostInterval * 1000UL);
  const uint32_t nowMs = millis();
  if (settingHttpPostProvider == HTTP_POST_MEENT && !meentApiKey().length()) {
    if (meentProvisionPending || meentProvisionBlocked) return;
    if (meentNextProvisionAttemptMs != 0 && (int32_t)(nowMs - meentNextProvisionAttemptMs) < 0) return;
  } else if (webhookLastPostMs != 0 && (uint32_t)(nowMs - webhookLastPostMs) < webhookIntervalMs) return;

  WorkerWebhookPayload payload = {};
  payload.id = _getChipId();
  payload.pFromGrid = outputPowerInt(DSMRdata.power_delivered.int_val());
  payload.pToGrid = outputPowerInt(DSMRdata.power_returned.int_val());
  payload.t1 = DSMRdata.energy_delivered_tariff1_present ? outputEnergyUint64(DSMRdata.energy_delivered_tariff1.int_val()) : 0;
  payload.t2 = DSMRdata.energy_delivered_tariff2_present ? outputEnergyUint64(DSMRdata.energy_delivered_tariff2.int_val()) : 0;
  payload.t1r = DSMRdata.energy_returned_tariff1_present ? outputEnergyUint64(DSMRdata.energy_returned_tariff1.int_val()) : 0;
  payload.t2r = DSMRdata.energy_returned_tariff2_present ? outputEnergyUint64(DSMRdata.energy_returned_tariff2.int_val()) : 0;
  payload.timestamp = actT;
  if (DSMRdata.voltage_l1_present) {
    payload.voltage[0] = (uint32_t)lroundf(outputVoltage(DSMRdata.voltage_l1.val()) * 10.0f);
    payload.voltagePresentMask |= 1U << 0;
  }
  if (DSMRdata.voltage_l2_present) {
    payload.voltage[1] = (uint32_t)lroundf(outputVoltage(DSMRdata.voltage_l2.val()) * 10.0f);
    payload.voltagePresentMask |= 1U << 1;
  }
  if (DSMRdata.voltage_l3_present) {
    payload.voltage[2] = (uint32_t)lroundf(outputVoltage(DSMRdata.voltage_l3.val()) * 10.0f);
    payload.voltagePresentMask |= 1U << 2;
  }

  if (DSMRdata.electricity_sags_l1_present) {
    payload.voltageSags[0] = DSMRdata.electricity_sags_l1;
    payload.sagSwellPresentMask |= 1U << 0;
  }
  if (DSMRdata.electricity_sags_l2_present) {
    payload.voltageSags[1] = DSMRdata.electricity_sags_l2;
    payload.sagSwellPresentMask |= 1U << 1;
  }
  if (DSMRdata.electricity_sags_l3_present) {
    payload.voltageSags[2] = DSMRdata.electricity_sags_l3;
    payload.sagSwellPresentMask |= 1U << 2;
  }
  if (DSMRdata.electricity_swells_l1_present) {
    payload.voltageSwells[0] = DSMRdata.electricity_swells_l1;
    payload.sagSwellPresentMask |= 1U << 3;
  }
  if (DSMRdata.electricity_swells_l2_present) {
    payload.voltageSwells[1] = DSMRdata.electricity_swells_l2;
    payload.sagSwellPresentMask |= 1U << 4;
  }
  if (DSMRdata.electricity_swells_l3_present) {
    payload.voltageSwells[2] = DSMRdata.electricity_swells_l3;
    payload.sagSwellPresentMask |= 1U << 5;
  }

  if (!WorkerEnqueueWebhookPost(payload)) return;

  bNewTelegramWebhook = false;
  webhookPostPending = true;
}

void PostWebhookFromWorker(const WorkerWebhookPayload& payload) {
#ifdef DEBUG
  webhookStartMs = millis();
#endif
  if (netw_state == NW_NONE) {
    webhookPostPending = false;
    return;
  }

  // KEMP keeps its agreed sample/config command contract, while the generic
  // connector uses the credentials and payload declared in post.json.
  if (settingHttpPostProvider == HTTP_POST_KEMP || settingHttpPostProvider == HTTP_POST_GENERIC ||
      (settingHttpPostProvider == HTTP_POST_MEENT && meentApiKey().length())) {
    HTTPClient http;
    const bool isKemp = settingHttpPostProvider == HTTP_POST_KEMP;
    const bool isMeent = settingHttpPostProvider == HTTP_POST_MEENT;
    const char* url = isKemp ? kempUrl : settingHttpPostUrl;
    int status = -1;
    String response;
    if (webhookHttpBegin(http, url)) {
      http.setTimeout(15000);
      http.addHeader("Content-Type", "application/json");
      if (!isKemp && settingHttpPostAuth == 1 && strlen(settingHttpPostAuthKey))
        http.addHeader("Authorization", "Bearer " + String(settingHttpPostAuthKey));
      else if (!isKemp && settingHttpPostAuth == 2 && strlen(settingHttpPostAuthName) && strlen(settingHttpPostAuthKey))
        http.addHeader(settingHttpPostAuthName, settingHttpPostAuthKey);
      if (!isKemp && strlen(settingHttpPostExtraHeaderName) && strlen(settingHttpPostExtraHeaderValue)) {
        String v = settingHttpPostExtraHeaderValue; v.replace("{mac}", macID);
        http.addHeader(settingHttpPostExtraHeaderName, v);
      }
      status = http.POST(JsonWebhook(payload));
      response = http.getString();
      http.end();
    }
    const bool ok = status >= 200 && status < 300;
    webhookLastHttpStatus = status;
    if (ok) {
      webhookPostErrors = 0;
      webhookLastSuccessfulPost = payload.timestamp;
      if (isKemp && status == HTTP_CODE_OK) kempApplyServerResponse(response);
      if (isMeent) {
        meentDataState = MEENT_OK; meentDataHttpStatus = status;
        meentLastSuccessfulPost = payload.timestamp;
        meentApiKeyState = MEENT_OK; meentApiKeyHttpStatus = status;
        meentApplyServerInterval(response);
      }
      if (!isKemp && settingHttpPostAcceptInterval) {
        JsonDocument doc;
        const int interval = deserializeJson(doc, response) ? 0 :
            (doc["interval_s"].is<int>() ? doc["interval_s"].as<int>() : (doc["interval"] | 0));
        if (interval >= 1 && interval <= 3600 && interval != settingHttpPostInterval) {
          settingHttpPostInterval = interval; savePostSettings();
        }
      }
      if (isKemp) kempLogPushRecovered();
    } else {
      webhookPostErrors++; webhookTlsClient.stop();
      if (isMeent) {
        meentDataState = MEENT_ERROR; meentDataHttpStatus = status;
        if (status == HTTP_CODE_UNAUTHORIZED || status == HTTP_CODE_FORBIDDEN) {
          meentApiKeyState = MEENT_ERROR; meentApiKeyHttpStatus = status;
        }
      }
      if (isKemp) { char detail[24]; snprintf(detail, sizeof(detail), "HTTP %d", status); kempLogPushFailure(detail); }
    }
    webhookLastPostMs = millis();
    webhookPostPending = false;
    return;
  }

  HTTPClient http;
  String webhookUrl;
#ifdef POST_KEMP
  String kempResponseBody;
  bool kempResponseAccepted = false;
#endif
#ifdef POST_POWERCH
  webhookUrl = URL_POWERCH;
#elif defined(POST_MEENT)
  meentProvisionPending = !meentApiKey().length();
  if (!meentProvision()) {
    webhookPostErrors++;
    meentProvisionPending = false;
    webhookPostPending = false;
    return;
  }
  webhookUrl = String(MEENT_API_BASE_URL) + "data/";
#elif defined(POST_KEMP)
  webhookUrl = kempUrl;
#endif

  if (http.begin(webhookTlsClient, webhookUrl)) {
    http.addHeader("Content-Type", "application/json");
#ifdef POST_MEENT
    http.addHeader("Authorization", "Bearer " + meentApiKey());
    http.addHeader("X-MAC-Address", macID);
#endif

    int httpResponseCode = http.POST(JsonWebhook(payload));
#ifdef POST_MEENT
    const String responseBody = http.getString();
#endif
#ifdef POST_KEMP
    kempResponseBody = http.getString();
#endif
    DebugT(F("HTTP Response code: ")); Debugln(httpResponseCode);

    if (httpResponseCode >= 200 && httpResponseCode < 300) {
#ifdef POST_MEENT
      meentDataState = MEENT_OK;
      meentDataHttpStatus = httpResponseCode;
      meentLastSuccessfulPost = payload.timestamp;
      meentApiKeyState = MEENT_OK; // The data endpoint has authenticated this key.
      meentApiKeyHttpStatus = httpResponseCode;
      meentApplyServerInterval(responseBody);
#endif
#ifdef POST_KEMP
      // Commands are only valid on the agreed 200 response. A 401 is logged
      // server-side and deliberately does not trigger a device-side action.
      kempResponseAccepted = httpResponseCode == HTTP_CODE_OK;
#endif
      webhookPostErrors = 0;
#ifdef POST_KEMP
      kempLogPushRecovered();
#endif
    } else {
#ifdef POST_MEENT
      meentDataState = MEENT_ERROR;
      meentDataHttpStatus = httpResponseCode;
      if (httpResponseCode == HTTP_CODE_UNAUTHORIZED || httpResponseCode == HTTP_CODE_FORBIDDEN) {
        meentApiKeyState = MEENT_ERROR;
        meentApiKeyHttpStatus = httpResponseCode;
      }
#endif
      webhookPostErrors++;
#ifdef POST_KEMP
      char errorDetail[24];
      snprintf(errorDetail, sizeof(errorDetail), "HTTP %d", httpResponseCode);
      kempLogPushFailure(errorDetail);
#endif
      webhookTlsClient.stop(); // hard reset van de TLS-socket zodat volgende call schoon start
      delay(10);
    }
    http.end();                  // resources van HTTPClient vrijgeven
#ifdef POST_KEMP
    if (kempResponseAccepted) kempApplyServerResponse(kempResponseBody);
#endif
  } else {
#ifdef POST_MEENT
    meentDataState = MEENT_ERROR;
    meentDataHttpStatus = -1;
#endif
    webhookPostErrors++;
    DebugTln(F("HTTP begin failed"));
#ifdef POST_KEMP
    kempLogPushFailure("HTTP begin");
#endif
    // begin() faalt? zorg ook hier dat de client schoon is
    webhookTlsClient.stop();
    delay(10);
  }

#if defined(POST_MEENT) || defined(POST_KEMP)
  // The interval limits attempts, not only successful deliveries. Otherwise a
  // 401/connection failure would produce a request for every P1 telegram.
  webhookLastPostMs = millis();
#endif

#ifdef DEBUG
  Debugf("Webhook process time: %d\n", millis() - webhookStartMs);
#endif
  webhookPostPending = false;
#ifdef POST_MEENT
  meentProvisionPending = false;
#endif
}

void MeentProvisionFromWorker() {
#ifdef POST_MEENT
  if (!bMeentEnabled || settingHttpPostProvider != HTTP_POST_MEENT) return;
  if (meentApiKey().length()) {
    meentProvisionPending = false;
    return;
  }
  meentProvisionPending = true;
  DebugTln(F("MEENT: start one-time provisioning"));
  if (meentProvision()) webhookPostErrors = 0;
  else webhookPostErrors++;
  meentProvisionPending = false;
#endif
}

void MeentConfigChanged() {
#ifdef POST_MEENT
  meentResetStatus();
  if (!bMeentEnabled) {
    if (settingHttpPostProvider == HTTP_POST_MEENT) {
      bHttpPostEnabled = false;
      savePostSettings();
    }
    return;
  }
  settingHttpPostProvider = HTTP_POST_MEENT;
  bHttpPostEnabled = meentApiKey().length();
  if (bHttpPostEnabled) {
    const String dataUrl = String(MEENT_API_BASE_URL) + "data/";
    strlcpy(settingHttpPostUrl, dataUrl.c_str(), sizeof(settingHttpPostUrl));
    settingHttpPostInterval = settingMeentInterval;
    settingHttpPostPayload = 1;
    settingHttpPostAuth = 1;
    strlcpy(settingHttpPostAuthKey, meentApiKey().c_str(), sizeof(settingHttpPostAuthKey));
    strlcpy(settingHttpPostAuthName, "Authorization", sizeof(settingHttpPostAuthName));
    strlcpy(settingHttpPostExtraHeaderName, "X-MAC-Address", sizeof(settingHttpPostExtraHeaderName));
    strlcpy(settingHttpPostExtraHeaderValue, "{mac}", sizeof(settingHttpPostExtraHeaderValue));
    settingHttpPostAcceptInterval = true;
    savePostSettings();
  } else if (netw_state != NW_NONE && meentEnsureClientSecret()) {
    meentProvisionPending = true;
    WorkerEnqueueSimple(WORKER_JOB_MEENT_PROVISION, WORKER_PRIO_NORMAL);
  }
#endif
}

void MeentClearClientSecret() {
#ifdef POST_MEENT
  preferences.remove(MEENT_CLIENT_SECRET_NVS_KEY);
  meentClientSecret[0] = '\0';
  meentResetStatus();
#endif
}

void AppendMeentStatus(JsonDocument& doc) {
#ifdef POST_MEENT
  if (!bMeentEnabled || !bHttpPostEnabled || settingHttpPostProvider != HTTP_POST_MEENT) return;
  doc["meent_webid_status"] = meentStatusText(meentWebIdState, meentWebIdHttpStatus);
  doc["meent_api_key_status"] = meentStatusText(meentApiKeyState, meentApiKeyHttpStatus);
  doc["meent_data_status"] = meentStatusText(meentDataState, meentDataHttpStatus);
  if (meentLastSuccessfulPost) doc["meent_last_success"] = meentLastSuccessfulPost;
#endif
}

void StartWebhook() {
  webhookTlsClient.setInsecure();
  webhookTlsClient.setTimeout(10000);
#ifdef POST_MEENT
  // Queue provisioning after network, filesystem and the worker are ready.
  // It is non-blocking and only runs on a fresh/cleared API-key setting.
  meentResetStatus();
  if (bMeentEnabled && settingHttpPostProvider == HTTP_POST_MEENT && !meentApiKey().length()) {
    if (meentEnsureClientSecret()) {
      meentProvisionPending = true;
      WorkerEnqueueSimple(WORKER_JOB_MEENT_PROVISION, WORKER_PRIO_NORMAL);
    } else {
      meentWebIdState = MEENT_ERROR;
      meentWebIdHttpStatus = -3;
    }
  }
#endif
  if (settingHttpPostProvider == HTTP_POST_KEMP && httpPostConfigured()) {
    kempLoadConnectionSettings();
    kempStartupConfigPending = true;
    kempQueueStartupConfig();
  }
}
