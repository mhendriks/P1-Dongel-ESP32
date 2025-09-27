#include "./../../_secrets/posts.h"

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

#ifdef POST_POWERCH

static WiFiClientSecure tslPower;
uint8_t PostErrors = 0;

String JsonPowerCH() {
  
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
  Debugf("JsonPowerCH: %s\n", output.c_str());
#endif  

  return output;
}

time_t tPower;

void PostPowerCh() {
#ifdef DEBUG
  tPower = millis();
#endif
  if (!bNewTelegramPostPower || netw_state == NW_NONE || PostErrors > 100 ) return;
  bNewTelegramPostPower = false;

  HTTPClient http;
  if (http.begin(tslPower, URL_POWERCH)) {
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.POST(JsonPowerCH());
    DebugT(F("HTTP Response code: ")); Debugln(httpResponseCode);

    if (httpResponseCode == 200) PostErrors = 0;
    else {
      PostErrors++;
      
      tslPower.stop(); // hard reset van de TLS-socket zodat volgende call schoon start
      delay(10);
    }
    http.end();                  // resources van HTTPClient vrijgeven
  } else {
    PostErrors++;
    DebugTln(F("HTTP begin failed"));
    // begin() faalt? zorg ook hier dat de client schoon is
    tslPower.stop();
    delay(10);
  }

#ifdef DEBUG
  Debugf("PowerCH process time: %d\n", millis() - tPower);
#endif
}

void StartPowerCH() {
  tslPower.setInsecure();
  tslPower.setTimeout(5000);
}
#else 
  void StartPowerCH(){}
  void PostPowerCh(){}
#endif
