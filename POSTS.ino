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

void PostTelegram() {
#ifdef POST_TELEGRAM
  if ( !strlen(pt_end_point) ) return;
  if ( ( (esp_log_timestamp()/1000) - TelegramLastPost) < pt_interval ) return;
  
  TelegramLastPost = esp_log_timestamp()/1000;
  
  HTTPClient http;
  http.begin(wifiClient, pt_end_point, pt_port);
  
  http.addHeader("Content-Type", "text/plain");
  http.addHeader("Accept", "*/*");
  
  int httpResponseCode = http.POST( CapTelegram );
  DebugT(F("HTTP Response code: "));Debugln(httpResponseCode);
  
  http.end();  
#endif
  
}

#if defined(POST_POWERCH) || defined(POST_MEENT)

static WiFiClientSecure webhookTlsClient;
uint8_t webhookPostErrors = 0;
uint32_t webhookLastPostMs = 0;

String JsonWebhook() {
  
  JsonDocument doc;
  // char idbuf[21]; //64b value   
  // snprintf(idbuf, sizeof idbuf, "%llu", (uint64_t) _getChipId() );
  doc["id"] = String(_getChipId());  
  doc["p_from_grid"] = DSMRdata.power_delivered.int_val();
  doc["p_to_grid"] = DSMRdata.power_returned.int_val();
  doc["timestamp"] = actT; // int of string, beide ok

  String output;
  serializeJson(doc, output);

#ifdef DEBUG
  Debugf("Webhook Json: %s\n", output.c_str());
#endif  

  return output;
}

time_t webhookStartMs;

void PostWebhook() {
#ifdef DEBUG
  webhookStartMs = millis();
#endif
  if (!bNewTelegramWebhook || netw_state == NW_NONE || webhookPostErrors > 100 ) return;

#ifdef POST_MEENT
  const uint32_t meentIntervalMs = (uint32_t)constrain(settingMeentInterval, (uint16_t)1, (uint16_t)3600) * 1000UL;
  const uint32_t nowMs = millis();
  if (webhookLastPostMs != 0 && (uint32_t)(nowMs - webhookLastPostMs) < meentIntervalMs) return;
#endif

  bNewTelegramWebhook = false;

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

    int httpResponseCode = http.POST(JsonWebhook());
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
}

void StartWebhook() {
  webhookTlsClient.setInsecure();
  webhookTlsClient.setTimeout(5000);
}
#else 
  void StartWebhook(){}
  void PostWebhook(){}
#endif
