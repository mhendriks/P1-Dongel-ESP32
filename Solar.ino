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

  String urlcheck = solarSystem->Url;
  bool bSolis = false;
  if(urlcheck.indexOf("CMD=inv_query") > 0) {
    DebugTln("Solis Omvormer");
    bSolis = true;
  }

  http.begin( solarSystem->Url.c_str() );
  http.addHeader("Accept", "application/json");
  if (src == ENPHASE) {
    if ( !bSolis ) http.addHeader("Authorization", "Bearer " + solarSystem->Token);
  } else {
    http.addHeader("X-API-Key", solarSystem->Token);
  }
  
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
  DynamicJsonDocument solarDoc(1024);
  DeserializationError error = deserializeJson(solarDoc, payload);
  if (error) {
    Debugln("Error deserialisation solarDoc");
    return;
  }
  if ( bSolis ) {
    solarSystem->Actual = solarDoc["i_pow_n"].as<int>();
    solarSystem->Daily  = (int) ((float)solarDoc["i_eday"] * 1000);
  } else {
    solarSystem->Actual = (src == ENPHASE) ? solarDoc["wattsNow"].as<int>()       : solarDoc["overview"]["currentPower"]["power"].as<int>();
    solarSystem->Daily  = (src == ENPHASE) ? solarDoc["wattHoursToday"].as<int>() : solarDoc["overview"]["lastDayData"]["energy"].as<int>();  
  }
  
#ifdef DEBUG
  Debug("Daily  > "); Debugln(solarSystem->Daily);
  Debug("Actual > "); Debugln(solarSystem->Actual);    
#endif  
}


String SolarProdActual(){
  return String(Enphase.Actual + SolarEdge.Actual);
}

String SolarProdToday(){
  return String(Enphase.Daily + SolarEdge.Daily);
}

uint32_t SolarWpTotal(){
  return Enphase.Wp + SolarEdge.Wp;
}

void SendSolarJson(){
 
  
  if ( !Enphase.Available && !SolarEdge.Available ) {
    httpServer.send(200, "application/json", "{\"active\":false}");
    return;
  }

/*
SCR = Eigengebruik = ( Productie - Teruglevering ) / Productie
SEUE = Zelf Voorzienendheid = ( Productie - Teruglevering ) / ( Afname + Productie - Teruglevering ) 
Prestatie = kWh/kWp
 */

  String Json = "{\"active\":true,\"total\":{\"daily\":";
  Json += SolarProdToday();
  Json += ",\"actual\":";
  Json += SolarProdActual();
  Json += "}, \"Wp\":";
  Json += String( Enphase.Wp + SolarEdge.Wp  );
  Json += ", \"perc\":{\"seue\":";
  Json += String( 0 /* self consumed solar engery  : totale energy consumptie */ );
  Json += ",\"scr\":";
  Json += String(  0 /* self consumed solar engery  : opwekking */);
  Json += ",\"pres\":";
  Json += String(  0 /* kWh/kWp */);
  Json += "}";
  Json += "}";

#ifdef DEBUG
  DebugTln("SendSolarJson");
  DebugT("Solar Json: ");Debugln(Json);
#endif
   
  httpServer.send(200, "application/json", Json.c_str() );  
}
