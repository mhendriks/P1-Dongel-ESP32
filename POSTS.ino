#if __has_include("./../../_secrets/posts.h")
  #include "./../../_secrets/posts.h"
#else
  // Optional local overrides (webhook URLs, OTAURL_PREFIX, etc.)
  #ifndef URL_POWERCH
    #define URL_POWERCH ""
  #endif
  #ifndef URL_MEENT
    #define URL_MEENT ""
  #endif
  #ifndef OTAURL_PREFIX
    #define OTAURL_PREFIX ""
  #endif
#endif

#if defined(POST_POWERCH) && defined(POST_MEENT)
  #error "POST_POWERCH and POST_MEENT cannot be enabled at the same time"
#endif

#if defined(POST_POWERCH) || defined(POST_MEENT)

static WiFiClientSecure webhookTlsClient;
uint8_t webhookPostErrors = 0;
uint32_t webhookLastPostMs = 0;
bool webhookPostPending = false;

String JsonWebhook(const WorkerWebhookPayload& payload) {
  
  JsonDocument doc;
  char idbuf[21];
  snprintf(idbuf, sizeof(idbuf), "%llu", (unsigned long long)payload.id);
  doc["id"] = idbuf;
  doc["p_from_grid"] = payload.pFromGrid;
  doc["p_to_grid"] = payload.pToGrid;
  doc["timestamp"] = payload.timestamp; // int of string, beide ok

  String output;
  serializeJson(doc, output);

#ifdef DEBUG
  Debugf("Webhook Json: %s\n", output.c_str());
#endif  

  return output;
}

time_t webhookStartMs;

void PostWebhook() {
  if (!bNewTelegramWebhook || netw_state == NW_NONE || webhookPostErrors > 100 || webhookPostPending) return;

#ifdef POST_MEENT
  const uint32_t meentIntervalMs = (uint32_t)constrain(settingMeentInterval, (uint16_t)1, (uint16_t)3600) * 1000UL;
  const uint32_t nowMs = millis();
  if (webhookLastPostMs != 0 && (uint32_t)(nowMs - webhookLastPostMs) < meentIntervalMs) return;
#endif

  WorkerWebhookPayload payload = {};
  payload.id = _getChipId();
  payload.pFromGrid = DSMRdata.power_delivered.int_val();
  payload.pToGrid = DSMRdata.power_returned.int_val();
  payload.timestamp = actT;

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
#endif

  if (http.begin(webhookTlsClient, webhookUrl)) {
    http.addHeader("Content-Type", "application/json");
#ifdef POST_MEENT
    http.addHeader("API-Version", MEENT_API_VERSION);
    http.addHeader("Authorization", MEENT_AUTH_TOKEN);
#endif

    int httpResponseCode = http.POST(JsonWebhook(payload));
    DebugT(F("HTTP Response code: ")); Debugln(httpResponseCode);

    if (httpResponseCode >= 200 && httpResponseCode < 300) {
      webhookPostErrors = 0;
#ifdef POST_MEENT
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
