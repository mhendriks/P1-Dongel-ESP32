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
  if ( ! bNewTelegramPostPower || PostErrors > 10 ) return;
  bNewTelegramPostPower = false;
  HTTPClient http;  
  http.setReuse(true); 

  if (http.begin(tslPower, URL_POWERCH)) {
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Connection", "keep-alive");
  
    int httpResponseCode = http.POST( JsonPowerCH() );
    DebugT(F("HTTP Response code: "));Debugln(httpResponseCode);
    if ( httpResponseCode == 200 ) PostErrors = 0; else PostErrors++;
    http.end();  
  } else {
    PostErrors++;
    DebugTln(F("HTTP begin failed"));
  }  

#ifdef DEBUG
  Debugf("PowerCH process time: %d\n", millis()-tPower);
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

// void fillJson(JsonDocument& doc) {
  
//   // Meter info
//   doc["meterType"] = DSMRdata.identification_present ? DSMRdata.identification : "-";
//   doc["version"] = DSMRdata.p1_version_present ? DSMRdata.p1_version : "-";
//   doc["timestamp"] = DSMRdata.timestamp_present ? DSMRdata.timestamp : "-";
//   doc["equipmentId"] = DSMRdata.equipment_id_present ? DSMRdata.equipment_id : "-";

//   // Elektriciteit ontvangen
//   doc["electricity"]["received"]["tariff1"]["reading"] = DSMRdata.energy_delivered_tariff1_present ? DSMRdata.energy_delivered_tariff1.val() : 0;
//   doc["electricity"]["received"]["tariff2"]["reading"] = DSMRdata.energy_delivered_tariff2_present ? DSMRdata.energy_delivered_tariff2.val() : 0;
//   doc["electricity"]["received"]["actual"]["reading"] = DSMRdata.power_delivered_present ? DSMRdata.power_delivered.int_val() : 0;

//   // Elektriciteit geleverd
//   doc["electricity"]["delivered"]["tariff1"]["reading"] = DSMRdata.energy_returned_tariff1_present ? DSMRdata.energy_returned_tariff1.val() : 0;
//   doc["electricity"]["delivered"]["tariff2"]["reading"] = DSMRdata.energy_returned_tariff2_present ? DSMRdata.energy_returned_tariff2.val() : 0;
//   doc["electricity"]["delivered"]["actual"]["reading"] = DSMRdata.power_returned_present ? DSMRdata.power_returned.int_val() : 0;

//   // Tariefindicator
//   if (DSMRdata.electricity_tariff_present) {
//     doc["electricity"]["tariffIndicator"] = String(DSMRdata.electricity_tariff).endsWith("2") ? 2 : 1;
//   }

//   // Instantane stroom en vermogen
//   doc["electricity"]["instantaneous"]["current"]["L1"]["reading"] = DSMRdata.current_l1_present ? DSMRdata.current_l1.val() : 0;
//   doc["electricity"]["instantaneous"]["current"]["L2"]["reading"] = DSMRdata.current_l2_present ? DSMRdata.current_l2.val() : 0;
//   doc["electricity"]["instantaneous"]["current"]["L3"]["reading"] = DSMRdata.current_l3_present ? DSMRdata.current_l3.val() : 0;

//   doc["electricity"]["instantaneous"]["power"]["positive"]["L1"]["reading"] = DSMRdata.power_delivered_l1_present ? DSMRdata.power_delivered_l1.val() : 0;
//   doc["electricity"]["instantaneous"]["power"]["positive"]["L2"]["reading"] = DSMRdata.power_delivered_l2_present ? DSMRdata.power_delivered_l2.val() : 0;
//   doc["electricity"]["instantaneous"]["power"]["positive"]["L3"]["reading"] = DSMRdata.power_delivered_l3_present ? DSMRdata.power_delivered_l3.val() : 0;

//   doc["electricity"]["instantaneous"]["power"]["negative"]["L1"]["reading"] = DSMRdata.power_returned_l1_present ? DSMRdata.power_returned_l1.val() : 0;
//   doc["electricity"]["instantaneous"]["power"]["negative"]["L2"]["reading"] = DSMRdata.power_returned_l2_present ? DSMRdata.power_returned_l2.val() : 0;
//   doc["electricity"]["instantaneous"]["power"]["negative"]["L3"]["reading"] = DSMRdata.power_returned_l3_present ? DSMRdata.power_returned_l3.val() : 0;

//   // Gas (mbus1)
//   if (DSMRdata.mbus1_device_type == 3 && DSMRdata.mbus1_delivered_present) {
//     JsonObject gas = doc["gas"];
//     gas["deviceType"] = "003";
//     gas["equipmentId"] = DSMRdata.mbus1_equipment_id_tc_present ? DSMRdata.mbus1_equipment_id_tc : "";
//     gas["timestamp"] = String(DSMRdata.mbus1_delivered.timestamp);
//     gas["reading"] = DSMRdata.mbus1_delivered.val();
//     gas["valvePosition"] = DSMRdata.mbus1_valve_position;
//   }

//   // Water (bv. mbus2 als type 2)
//   if (DSMRdata.mbus2_device_type == 2 && DSMRdata.mbus2_delivered_present) {
//     JsonObject water = doc["water"];
//     water["deviceType"] = "002";
//     water["equipmentId"] = DSMRdata.mbus2_equipment_id_tc_present ? DSMRdata.mbus2_equipment_id_tc : "";
//     water["timestamp"] = String(DSMRdata.mbus2_delivered.timestamp);
//     water["reading"] = DSMRdata.mbus2_delivered.val();
//     water["valvePosition"] = DSMRdata.mbus2_valve_position;
//   }
// }

// void PostHomey(){
//   JsonDocument HomeyDoc;
//   String PostData;
//   fillJson(HomeyDoc);
//   serializeJsonPretty(HomeyDoc, PostData);

//   HTTPClient http;
//   http.begin("https://homey/api/app/com.p1/update");
  
//   http.addHeader("Content-Type", "application/json");
//   http.addHeader("Accept", "*/*");
  
//   int httpResponseCode = http.POST( PostData );
//   DebugT(F("HTTP Response code: "));Debugln(httpResponseCode);
//   // if ( httpResponseCode == 200 ) Debugln(PostData);
//   http.end();  


// }