struct AccuPwrSystems {
  bool      Available;
  String    Url;
  String    unit;
  String    status;
  float     currentPower;
  uint8_t   chargeLevel;
  uint32_t  Interval;
  time_t    LastRefresh;
  char    file_name[20];
};

AccuPwrSystems SolarEdgeAccu = { false, "", "", "", 0.0, 0, 300, 0, "/se_accu.json"  };
 
void ReadAccuConfig(){
  if ( skipNetwork ) return;
  AccuPwrSystems* solarSystem = &SolarEdgeAccu;
  
  if ( !FSmounted || !LittleFS.exists( solarSystem->file_name ) ) return;

  DebugT("Read Accu Config: ");Debugln(solarSystem->file_name);
  
  JsonDocument doc; 
  File SolarFile = LittleFS.open( solarSystem->file_name, "r");
  if ( !SolarFile ) {
      DebugTln("Error reading SolarFile");
      return;
   }
  
  DeserializationError error = deserializeJson(doc, SolarFile);
  if (error) {
    Debugln("Error deserialize Accu config");
    SolarFile.close();
    return;
  }
  
  solarSystem->Available = true;
  solarSystem->Url       = doc["gateway-url"].as<String>();
  // if ( doc["refresh-interval"].as<int>() > (src==ENPHASE?60:300) ) 
  solarSystem->Interval = doc["refresh-interval"].as<int>();

#ifdef DEBUG
  Debug("url      > "); Debugln(solarSystem->Url);
  Debug("interval > "); Debugln(solarSystem->Interval);
  
#endif
 
  SolarFile.close();

  GetAccuData( true );
}

void GetAccuDataN() {
  if ( skipNetwork ) return;
  GetAccuData( false );
}

void GetAccuData( bool forceUpdate ){
  
  AccuPwrSystems* solarSystem = &SolarEdgeAccu;
  if ( !solarSystem->Available ) return; 
  if ( !forceUpdate && ((uptime() - solarSystem->LastRefresh) < solarSystem->Interval) )  return;
  
  solarSystem->LastRefresh = uptime();
  
  HTTPClient http;

  String urlcheck = solarSystem->Url;
  
  http.begin( solarSystem->Url.c_str() );
  http.addHeader("Accept", "application/json");
  // http.addHeader("X-API-Key", solarSystem->Token);
  
  int httpResponseCode = http.GET();
  DebugT(F("HTTP Response code: "));Debugln(httpResponseCode);
  
  if ( httpResponseCode <= 0 ) { 
    return; //leave on error
  }
  
  String payload = http.getString();
  Debugln(payload);
  http.end();

//  payload = "{\"g_sn\":1902457183,\"g_ver\":\"ME-121001-V1.0.6(201704012008)\",\"time\":1723404273,\"g_time\":9168,\"g_err\":0,\"mac\":\"bc-54-f9-f5-73-bc\",\"auto_ip\":1,\"ip\":\"192.168.0.38\",\"sub\":\"255.255.255.0\",\"gate\":\"192.168.0.1\",\"auto_dns\":1,\"dns\":\"208.67.222.222\",\"dns_bak\":\"208.67.222.222\",\"i_status\":16,\"i_type\":1283,\"i_num\":1,\"i_sn\":\"110E301842200080\",\"i_ver_m\":\"\",\"i_ver_s\":\"\",\"i_modle\":\"\",\"i_pow\":\"\",\"i_pow_n\":88,\"i_eday\":\"8.451\",\"i_eall\":\"8012.0\",\"i_alarm\":\"F22F23\",\"i_last_t\":0,\"l_a_agr\":2,\"l_a_ip\":\"8.211.53.112\",\"l_a_port\":10000}";
    
  // Parse JSON object in response
  JsonDocument solarDoc;
  DeserializationError error = deserializeJson(solarDoc, payload);
  if (error) {
    Debugln("Error deserialisation solarDoc");
    return;
  }
  solarSystem->unit = solarDoc["siteCurrentPowerFlow"]["unit"].as<const char*>();
  solarSystem->status = solarDoc["siteCurrentPowerFlow"]["STORAGE"]["status"].as<const char*>();
  solarSystem->currentPower = solarDoc["siteCurrentPowerFlow"]["STORAGE"]["currentPower"].as<float>();
  solarSystem->chargeLevel = solarDoc["siteCurrentPowerFlow"]["STORAGE"]["chargeLevel"].as<int>();

#ifdef DEBUG
  Debug("status  > "); Debugln(solarSystem->status);
  Debug("currentPower > "); Debugln(solarSystem->currentPower); 
  Debug("chargeLevel > "); Debugln(solarSystem->chargeLevel);    
#endif  
}

void SendAccuJson(){
  
  if ( !SolarEdgeAccu.Available ) {
    httpServer.send(200, "application/json", "{\"active\":false}");
    return;
  }

  String Json = "{\"active\":true,\"status\":\"";
  Json += String(SolarEdgeAccu.status);
  Json += "\",\"unit\":\"";
  Json += String(SolarEdgeAccu.unit);
  Json += "\", \"currentPower\":";
  Json += String( SolarEdgeAccu.currentPower );
  Json += ", \"chargeLevel\":";
  Json += String( SolarEdgeAccu.chargeLevel );
  Json += "}";

#ifdef DEBUG
  DebugTln("SendAccuJson");
  DebugT("Accu Json: ");Debugln(Json);
#endif
   
  httpServer.send(200, "application/json", Json.c_str() );  
}
