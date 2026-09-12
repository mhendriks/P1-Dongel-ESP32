#if __has_include("./../../_secrets/posts.h")
  #include "./../../_secrets/posts.h"
#endif

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
#ifdef POST_MEENT
  // Staging is intentionally used in DEBUG builds; release firmware writes to
  // the production webhook. A private posts.h may override this when needed.
  #ifndef MEENT_API_BASE_URL
    #ifdef DEBUG
      #define MEENT_API_BASE_URL "https://meent.dev.muze.nl/api/"
    #else
      #define MEENT_API_BASE_URL "https://webhook.energiemeent.nl/api/"
    #endif
  #endif
#endif

#if (defined(POST_POWERCH) + defined(POST_MEENT) + defined(POST_KEMP)) > 1
  #error "POST_POWERCH, POST_MEENT and POST_KEMP are mutually exclusive"
#endif

#if defined(POST_POWERCH) || defined(POST_MEENT) || defined(POST_KEMP)

static WiFiClientSecure webhookTlsClient;
uint8_t webhookPostErrors = 0;
uint32_t webhookLastPostMs = 0;
bool webhookPostPending = false;

String JsonWebhook(const WorkerWebhookPayload& payload) {
  
  JsonDocument doc;
#ifdef POST_KEMP
  doc["api_key"] = KEMP_API_KEY;
#endif
  char idbuf[21];
  snprintf(idbuf, sizeof(idbuf), "%llu", (unsigned long long)payload.id);
  doc["id"] = idbuf;
  doc["p_from_grid"] = payload.pFromGrid;
  doc["p_to_grid"] = payload.pToGrid;
#ifdef POST_MEENT
  doc["t1"] = payload.t1;
  doc["t2"] = payload.t2;
  doc["t1r"] = payload.t1r;
  doc["t2r"] = payload.t2r;
#endif
#ifdef POST_KEMP
  doc["v_l1"] = payload.voltage[0];
  doc["v_l2"] = payload.voltage[1];
  doc["v_l3"] = payload.voltage[2];

  const char* sagKeys[] = {
    "voltage_sag_l1_count", "voltage_sag_l2_count", "voltage_sag_l3_count"
  };
  const char* swellKeys[] = {
    "voltage_swell_l1_count", "voltage_swell_l2_count", "voltage_swell_l3_count"
  };
  for (uint8_t phase = 0; phase < 3; phase++) {
    if (payload.sagSwellPresentMask & (1U << phase)) doc[sagKeys[phase]] = payload.voltageSags[phase];
    if (payload.sagSwellPresentMask & (1U << (phase + 3))) doc[swellKeys[phase]] = payload.voltageSwells[phase];
  }
#endif
  doc["timestamp"] = payload.timestamp; // int of string, beide ok

  String output;
  serializeJson(doc, output);

#ifdef DEBUG
  Debugf("Webhook Json: %s\n", output.c_str());
#endif  

  return output;
}

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
static uint32_t meentLastProvisionAttemptMs = 0;
static bool meentProvisionPending = false;
static bool meentProvisionBlocked = false;

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
    case MEENT_BLOCKED: return "fout (HTTP 409, actie nodig)";
    case MEENT_ERROR:
      if (httpStatus == -1) return "fout (verbinding)";
      if (httpStatus == -2) return "fout (ongeldig antwoord)";
      return "fout (HTTP " + String(httpStatus) + ")";
  }
  return "onbekend";
}

static void meentResetStatus() {
  meentWebIdState = strlen(settingMeentWebId) ? MEENT_STORED : MEENT_NOT_SET;
  meentApiKeyState = meentApiKey().length() ? MEENT_STORED : MEENT_NOT_SET;
  meentDataState = MEENT_NOT_SET;
  meentWebIdHttpStatus = meentApiKeyHttpStatus = meentDataHttpStatus = 0;
  meentLastSuccessfulPost = 0;
  meentLastProvisionAttemptMs = 0;
  meentProvisionPending = false;
  meentProvisionBlocked = false;
}

static bool meentPostText(const String& url, const String& request, String& response, int& status) {
  HTTPClient http;
  http.setTimeout(15000); // Pod creation can take 5–10 seconds.
  if (!http.begin(webhookTlsClient, url)) {
    status = -1;
    return false;
  }
  http.addHeader("Content-Type", "text/plain");
  status = http.POST(request);
  response = http.getString();
  http.end();
  DebugTf("MEENT provisioning response: %d\r\n", status);
  return status == HTTP_CODE_CREATED;
}

static bool meentProvision() {
  String response;
  meentLastProvisionAttemptMs = millis();
  if (!strlen(settingMeentWebId)) {
    meentWebIdState = MEENT_IN_PROGRESS;
    if (!meentPostText(String(MEENT_API_BASE_URL) + "pod/", macID, response, meentWebIdHttpStatus)) {
      meentWebIdState = meentWebIdHttpStatus == HTTP_CODE_CONFLICT ? MEENT_BLOCKED : MEENT_ERROR;
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
      meentApiKeyState = meentApiKeyHttpStatus == HTTP_CODE_CONFLICT ? MEENT_BLOCKED : MEENT_ERROR;
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
  writeSettingsDirect(); // This runs in the worker, so persist the server policy directly.
  DebugTf("MEENT: interval updated by server to %u seconds\r\n", serverInterval);
}
#endif

void PostWebhook() {
  if (!bNewTelegramWebhook || netw_state == NW_NONE || webhookPostPending) return;
#ifdef POST_MEENT
  // Provisioning retries are already interval-limited below. Do not turn a
  // temporary outage into a permanent failure after 100 attempts.
  if (meentApiKey().length() && webhookPostErrors > 100) return;
#else
  if (webhookPostErrors > 100) return;
#endif

#if defined(POST_MEENT) || defined(POST_KEMP)
#ifdef POST_MEENT
  const uint32_t webhookIntervalMs = meentIntervalMs();
#else
  const uint32_t webhookIntervalMs = 60UL * 1000UL;
#endif
  const uint32_t nowMs = millis();
#ifdef POST_MEENT
  if (!meentApiKey().length()) {
    if (meentProvisionPending || meentProvisionBlocked) return;
    if (meentLastProvisionAttemptMs != 0 && (uint32_t)(nowMs - meentLastProvisionAttemptMs) < webhookIntervalMs) return;
  } else if (webhookLastPostMs != 0 && (uint32_t)(nowMs - webhookLastPostMs) < webhookIntervalMs) return;
#else
  if (webhookLastPostMs != 0 && (uint32_t)(nowMs - webhookLastPostMs) < webhookIntervalMs) return;
#endif
#endif

  WorkerWebhookPayload payload = {};
  payload.id = _getChipId();
  payload.pFromGrid = outputPowerInt(DSMRdata.power_delivered.int_val());
  payload.pToGrid = outputPowerInt(DSMRdata.power_returned.int_val());
#ifdef POST_MEENT
  payload.t1 = DSMRdata.energy_delivered_tariff1_present ? outputEnergyUint64(DSMRdata.energy_delivered_tariff1.int_val()) : 0;
  payload.t2 = DSMRdata.energy_delivered_tariff2_present ? outputEnergyUint64(DSMRdata.energy_delivered_tariff2.int_val()) : 0;
  payload.t1r = DSMRdata.energy_returned_tariff1_present ? outputEnergyUint64(DSMRdata.energy_returned_tariff1.int_val()) : 0;
  payload.t2r = DSMRdata.energy_returned_tariff2_present ? outputEnergyUint64(DSMRdata.energy_returned_tariff2.int_val()) : 0;
#endif
  payload.timestamp = actT;
#ifdef POST_KEMP
  payload.voltage[0] = DSMRdata.voltage_l1_present
      ? (uint32_t)lroundf(outputVoltage(DSMRdata.voltage_l1.val()) * 10.0f) : 0;
  payload.voltage[1] = DSMRdata.voltage_l2_present
      ? (uint32_t)lroundf(outputVoltage(DSMRdata.voltage_l2.val()) * 10.0f) : 0;
  payload.voltage[2] = DSMRdata.voltage_l3_present
      ? (uint32_t)lroundf(outputVoltage(DSMRdata.voltage_l3.val()) * 10.0f) : 0;

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
#endif

  if (!WorkerEnqueueWebhookPost(payload)) return;

  bNewTelegramWebhook = false;
  webhookPostPending = true;
}

void PostWebhookFromWorker(const WorkerWebhookPayload& payload) {
#ifdef DEBUG
  webhookStartMs = millis();
#endif
  if (netw_state == NW_NONE || webhookPostErrors > 100 ) {
    webhookPostPending = false;
    return;
  }

  HTTPClient http;
  String webhookUrl;
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
  webhookUrl = URL_KEMP;
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
    DebugT(F("HTTP Response code: ")); Debugln(httpResponseCode);

    if (httpResponseCode >= 200 && httpResponseCode < 300) {
      meentDataState = MEENT_OK;
      meentDataHttpStatus = httpResponseCode;
      meentLastSuccessfulPost = payload.timestamp;
      meentApplyServerInterval(responseBody);
      webhookPostErrors = 0;
#if defined(POST_MEENT) || defined(POST_KEMP)
      webhookLastPostMs = millis();
#endif
    } else {
      meentDataState = MEENT_ERROR;
      meentDataHttpStatus = httpResponseCode;
      webhookPostErrors++;
      webhookTlsClient.stop(); // hard reset van de TLS-socket zodat volgende call schoon start
      delay(10);
    }
    http.end();                  // resources van HTTPClient vrijgeven
  } else {
#ifdef POST_MEENT
    meentDataState = MEENT_ERROR;
    meentDataHttpStatus = -1;
#endif
    webhookPostErrors++;
    DebugTln(F("HTTP begin failed"));
    // begin() faalt? zorg ook hier dat de client schoon is
    webhookTlsClient.stop();
    delay(10);
  }

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
#endif
}

void AppendMeentStatus(JsonDocument& doc) {
#ifdef POST_MEENT
  doc["meent_webid_status"] = meentStatusText(meentWebIdState, meentWebIdHttpStatus);
  doc["meent_api_key_status"] = meentStatusText(meentApiKeyState, meentApiKeyHttpStatus);
  doc["meent_data_status"] = meentStatusText(meentDataState, meentDataHttpStatus);
  if (meentLastSuccessfulPost) doc["meent_last_success"] = meentLastSuccessfulPost;
#endif
}

void StartWebhook() {
  webhookTlsClient.setInsecure();
  webhookTlsClient.setTimeout(15000);
#ifdef POST_MEENT
  // Queue provisioning after network, filesystem and the worker are ready.
  // It is non-blocking and only runs on a fresh/cleared API-key setting.
  meentResetStatus();
  if (!meentApiKey().length()) {
    meentProvisionPending = true;
    WorkerEnqueueSimple(WORKER_JOB_MEENT_PROVISION, WORKER_PRIO_NORMAL);
  }
#endif
}
#else 
  void StartWebhook(){}
  void PostWebhook(){}
  void PostWebhookFromWorker(const WorkerWebhookPayload& payload){}
  void MeentProvisionFromWorker(){}
  void MeentConfigChanged(){}
  void AppendMeentStatus(JsonDocument&){}
#endif
