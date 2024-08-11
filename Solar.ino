struct SolarPwrSystems {
  bool      Available;
  String    Url;
  String    Token;
  uint64_t  TokenExpire;
  uint32_t  SiteID;
  uint32_t  Wp;
  uint32_t  Actual;
  uint32_t  Daily;
  uint32_t  Interval;
  time_t    LastRefresh;
  char    file_name[20];
};

SolarPwrSystems Enphase   = { false, "https://envoy/api/v1/production", "", 0, 0, 0, 0, 0,  60, 0, "/enphase.json"  }, SolarEdge = { false, "", "", 0, 0, 0, 0, 0, 300, 0, "/solaredge.json"  };

void ReadSolarConfig( SolarSource src ){
  SolarPwrSystems* solarSystem = (src == ENPHASE ? &Enphase : &SolarEdge);
  
  if ( !FSmounted || !LittleFS.exists( solarSystem->file_name ) ) return;

  DebugT("ReadSolarConfig: ");Debugln(solarSystem->file_name);
  
  StaticJsonDocument<800> doc; 
  File SolarFile = LittleFS.open( solarSystem->file_name, "r");
  if ( !SolarFile ) {
      DebugTln("Error reading SolarFile");
      return;
   }
  
  DeserializationError error = deserializeJson(doc, SolarFile);
  if (error) {
    Debugln("Error deserialize SolarFile");
    SolarFile.close();
    return;
  }
  
  solarSystem->Available = true;
  solarSystem->Url       = doc["gateway-url"].as<String>();
  solarSystem->Token     = doc["token"].as<String>();
  solarSystem->Wp        = doc["wp"].as<int>();
  solarSystem->SiteID    = doc["siteid"].as<int>();
  if ( doc["refresh-interval"].as<int>() > (src==ENPHASE?60:300) ) solarSystem->Interval = doc["refresh-interval"].as<int>();

#ifdef DEBUG
  Debug("url      > "); Debugln(solarSystem->Url);
  Debug("token    > "); Debugln(solarSystem->Token);
  Debug("wp       > "); Debugln(solarSystem->Wp);
  Debug("interval > "); Debugln(solarSystem->Interval);
  Debug("siteid   > "); Debugln(solarSystem->SiteID);
  
#endif
 
  SolarFile.close();

  GetSolarData( src, true );
}

void ReadSolarConfigs(){
  ReadSolarConfig( ENPHASE );
  ReadSolarConfig( SOLAR_EDGE );
}

void GetSolarDataN(){
  GetSolarData( ENPHASE, false );
  GetSolarData( SOLAR_EDGE, false );
}

void GetSolarData( SolarSource src, bool forceUpdate ){
  
  SolarPwrSystems* solarSystem = (src == ENPHASE ? &Enphase : &SolarEdge);
  
  if ( !solarSystem->Available ) return; 
  if ( !forceUpdate && ((uptime() - solarSystem->LastRefresh) < solarSystem->Interval) )  return;
  
  solarSystem->LastRefresh = uptime();
  
  HTTPClient http;
  if (src == SOLAR_EDGE) {
    solarSystem->Url = "https://monitoringapi.solaredge.com/site/" + String(SolarEdge.SiteID) + "/overview";
    Debug("url      > "); Debugln(solarSystem->Url);
  }
  
  http.begin( solarSystem->Url.c_str() );
  http.addHeader("Accept", "application/json");
  if (src == ENPHASE) {
    http.addHeader("Authorization", "Bearer " + solarSystem->Token);
  } else {
    http.addHeader("X-API-Key", solarSystem->Token);
  }
  
  int httpResponseCode = http.GET();
  Debug(F("HTTP Response code: "));Debugln(httpResponseCode);
  
  if ( httpResponseCode <= 0 ) { 
    return; //leave on error
  }
  
  String payload = http.getString();
  Debugln(payload);
  http.end();
    
  // Parse JSON object in response
  DynamicJsonDocument solarDoc(512);
  DeserializationError error = deserializeJson(solarDoc, payload);
  if (error) {
    Debugln("Error deserialisation solarDoc");
    return;
  }

  solarSystem->Actual = (src == ENPHASE) ? solarDoc["wattsNow"].as<int>()       : solarDoc["overview"]["currentPower"]["power"].as<int>();
  solarSystem->Daily  = (src == ENPHASE) ? solarDoc["wattHoursToday"].as<int>() : solarDoc["overview"]["lastDayData"]["energy"].as<int>();

#ifdef DEBUG
  Debug("Daily  > "); Debugln(solarSystem->Daily);
  Debug("Actual > "); Debugln(solarSystem->Actual);    
#endif  
}

void SendSolarJson(){
  DebugTln("SendSolarJson");
  if ( !Enphase.Available && !SolarEdge.Available ) {
    httpServer.send(200, "application/json", "{\"active\":false}");
    return;
  }
  httpServer.send(200, "application/json", "{\"active\":true,\"total\":{\"daily\":" + String(Enphase.Daily + SolarEdge.Daily) + ",\"actual\":" + String(Enphase.Actual + SolarEdge.Actual) + "}, \"Wp\":" + String(Enphase.Wp + SolarEdge.Wp  ) + "}");  
}
