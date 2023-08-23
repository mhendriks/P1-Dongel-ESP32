String eid_token;
DECLARE_TIMER_SEC(EID, 10);

void handleEnergyID(){  

#ifdef ENERGYID 
  if ( DUE(EID) ) {
    DebugTln(F("handleEnergyID: DUE"));
    if ( !eid_token.length() ) ReadEIDToken();
    else PostEnergyID();
  }
#endif  

}

#ifdef ENERGYID

#define EID_BASE_URL "https://hooks.energyid.eu/services/WebhookIn/"

void ReadEIDToken(){
  if (!FSmounted) return;
  File TokenFile = LittleFS.open("/eid-token.txt", "r"); // open for appending  
  if (!TokenFile) {
    DebugTln(F("open eid-token.txt FAILED!!!--> Bailout\r\n"));
    return;
  }
  eid_token = TokenFile.readStringUntil('\r');
  CHANGE_INTERVAL_SEC(EID, 300); //after succesfull read set interval on 300sec
  
  DebugT(F("token read:"));Debugln(eid_token);
}
  
//void GETEnergyID(){
//
//  HTTPClient http;
//  snprintf( cMsg, sizeof( cMsg ), "%s%s",EID_BASE_URL, eid_token );
//  http.begin( cMsg );
//  
//  int httpResponseCode = http.GET();
//  if ( httpResponseCode<=0 ) { 
//    Debug(F("Error code: "));Debugln(httpResponseCode);
//    return;
//  }
//  
//  Debug(F("HTTP Response code: "));Debugln(httpResponseCode);
//  String payload = http.getString();
//  Debugln(payload);
//  http.end();
//
//}

String IsoTS () {
  // convert to this format -> 2023-06-16T08:01+0200
  
  String tmpTS = String(actTimestamp);
  String DateTime = "";
  DateTime   = "20" + tmpTS.substring(0, 2);    // YY
  DateTime  += "-"  + tmpTS.substring(2, 4);    // MM
  DateTime  += "-"  + tmpTS.substring(4, 6);    // DD
  DateTime  += "T";
  DateTime  += tmpTS.substring(6, 8);    // HH
  DateTime  += ":"  + tmpTS.substring(8, 10);   // MM
  DateTime  += ":"  + tmpTS.substring(10, 12);  // SS
  DateTime  += tmpTS[12]=='S' ? "+0200" : "+0100";
  return DateTime;
}

String JsonEnergyID ( const char* id, const char* metric, double sm_value, const char* unit, const char* type ){
/*  
  {
    "remoteId": "p1-dongle-pro",
    "remoteName": "P1 Dongle Pro",
    "metric": "electricityExport",
    "readingType": "counter",
    "unit": "kWh",    
    "data": [
        ["2023-06-10T08:01+0200", 1.123]
    ]
}
*/  
  
  String Json;
  Json  = "{\"remoteId\": \"p1-dongle-pro-" + String(id) + "\"";
  Json += ",\"remoteName\": \"P1 " + String(id) + "\"";
  Json += ",\"metric\":\"" + String(metric) + "\"";
  Json += ",\"readingType\": \"counter\"";
  Json += ",\"unit\": \"" + String(unit) + "\"";
  Json += ",\"interval\": \"PT15M\"";  
  if ( strlen(type) > 0 ) Json += ",\"register\": \"" + String(type) + "\"";
  Json += ",\"data\":[";
  //data 
  Json +="[\"" + IsoTS() + "\"," + sm_value + "]";
  Json += "]}";

  Debug("JsonEnergyID : "); Debugln(Json);
  
  return Json;

}

void PostEnergyID(){
/*
1- afgenomen dag
2- afgenomen nacht
3- teruglevering dag
4- teruglevering nacht
5- gas
6- water
*/

  
  String httpRequestData;
  int httpResponseCode;
  HTTPClient http;
  snprintf( cMsg, sizeof( cMsg ), "%s%s",EID_BASE_URL, eid_token.c_str() );  
  http.begin( cMsg );
  http.addHeader("Content-Type", "application/json");

  DebugT(F("Post URL:"));Debugln(cMsg);

  if ( DSMRdata.energy_returned_tariff1 ) {
    httpRequestData = JsonEnergyID ("e-ex-1", "electricityExport", DSMRdata.energy_returned_tariff1, "kWh","low");           
    httpResponseCode = http.POST(httpRequestData);
    if ( httpResponseCode<=0 ) { 
      Debug(F("Error code: "));Debugln(httpResponseCode);
      return;
    }
  }
  
  if ( DSMRdata.energy_returned_tariff2 ) {
    httpRequestData = JsonEnergyID ("e-ex-2", "electricityExport", DSMRdata.energy_returned_tariff2, "kWh","high");           
    httpResponseCode = http.POST(httpRequestData);
    if ( httpResponseCode<=0 ) { 
      Debug(F("Error code: "));Debugln(httpResponseCode);
      return;
    }
  }

  if ( DSMRdata.energy_delivered_tariff1 ) {
    httpRequestData = JsonEnergyID ("e-in-1","electricityImport", DSMRdata.energy_delivered_tariff1, "kWh","low");           
    httpResponseCode = http.POST(httpRequestData);
    if ( httpResponseCode<=0 ) { 
      Debug(F("Error code: "));Debugln(httpResponseCode);
      return;
    }
  }

  if ( DSMRdata.energy_delivered_tariff2 ) {
    httpRequestData = JsonEnergyID ("e-in-2","electricityImport", DSMRdata.energy_delivered_tariff2, "kWh","high");           
    httpResponseCode = http.POST(httpRequestData);
    if ( httpResponseCode<=0 ) { 
      Debug(F("Error code: "));Debugln(httpResponseCode);
      return;
    }
  }
  
  if ( gasDelivered ){
    httpRequestData = JsonEnergyID ("gas", "naturalGasImport", gasDelivered, "m³", "");           
    httpResponseCode = http.POST(httpRequestData);
    if ( httpResponseCode<=0 ) { 
      Debug(F("Error code: "));Debugln(httpResponseCode);
      return;
    }
  }
  
  http.end();
}

#endif
