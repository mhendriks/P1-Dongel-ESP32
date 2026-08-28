#if __has_include("./../../_secrets/posts.h")
  #include "./../../_secrets/posts.h"
#endif

#ifndef URL_POWERCH
  #define URL_POWERCH ""
#endif
#ifndef URL_MEENT
  #define URL_MEENT ""
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
static String meentAuthorizationHeader() {
  String token = settingMeentToken;
  token.trim();
  if (!token.length()) token = MEENT_AUTH_TOKEN;
  token.trim();
  if (!token.length()) return "";
  if (token.startsWith("Bearer ") || token.startsWith("bearer ")) return token;
  return "Bearer " + token;
}
#endif

void PostWebhook() {
  if (!bNewTelegramWebhook || netw_state == NW_NONE || webhookPostErrors > 100 || webhookPostPending) return;

#if defined(POST_MEENT) || defined(POST_KEMP)
#ifdef POST_MEENT
  const uint16_t effectiveMeentInterval =
#ifdef DEBUG
      10;
#else
      constrain(settingMeentInterval, (uint16_t)1, (uint16_t)3600);
#endif
  const uint32_t webhookIntervalMs = (uint32_t)effectiveMeentInterval * 1000UL;
#else
  const uint32_t webhookIntervalMs = 60UL * 1000UL;
#endif
  const uint32_t nowMs = millis();
  if (webhookLastPostMs != 0 && (uint32_t)(nowMs - webhookLastPostMs) < webhookIntervalMs) return;
#endif

  WorkerWebhookPayload payload = {};
  payload.id = _getChipId();
  payload.pFromGrid = outputPowerInt(DSMRdata.power_delivered.int_val());
  payload.pToGrid = outputPowerInt(DSMRdata.power_returned.int_val());
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
  webhookUrl = URL_MEENT;
#elif defined(POST_KEMP)
  webhookUrl = URL_KEMP;
#endif

  if (http.begin(webhookTlsClient, webhookUrl)) {
    http.addHeader("Content-Type", "application/json");
#ifdef POST_MEENT
    http.addHeader("API-Version", MEENT_API_VERSION);
    String authHeader = meentAuthorizationHeader();
    if (authHeader.length()) http.addHeader("Authorization", authHeader);
#endif

    int httpResponseCode = http.POST(JsonWebhook(payload));
    DebugT(F("HTTP Response code: ")); Debugln(httpResponseCode);

    if (httpResponseCode >= 200 && httpResponseCode < 300) {
      webhookPostErrors = 0;
#if defined(POST_MEENT) || defined(POST_KEMP)
      webhookLastPostMs = millis();
#endif
    } else {
      webhookPostErrors++;
      webhookTlsClient.stop(); // hard reset van de TLS-socket zodat volgende call schoon start
      delay(10);
    }
    http.end();                  // resources van HTTPClient vrijgeven
  } else {
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
}

void StartWebhook() {
  webhookTlsClient.setInsecure();
  webhookTlsClient.setTimeout(5000);
}
#else 
  void StartWebhook(){}
  void PostWebhook(){}
  void PostWebhookFromWorker(const WorkerWebhookPayload& payload){}
#endif
