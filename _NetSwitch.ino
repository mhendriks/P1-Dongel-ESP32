#ifdef NETSWITCH

StaticJsonDocument<256> docTriggers;
time_t ShellyStateTrue = 0;
time_t ShellyStateFalse = 0;
bool bShellySwitch = false;
bool lastToggleState;
bool bNetSwitchConfigRead = false;

void fNetSwitchProc( void * pvParameters ){
  DebugTln(F("Enable NetSwitch processing"));
  readtriggers();
  //set default value and force to switch
  lastToggleState = docTriggers["device"]["default"].as<int>();
  bShellySwitch = true;
  while(true) {
    handleNetSwitch();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
  DebugTln(F("!!! DISABLE NetSwitch processing"));
  vTaskDelete(NULL);
}

void SetupNetSwitch(){
  if( xTaskCreate( fNetSwitchProc, "netswitch", 4*1024, NULL, 3, NULL ) == pdPASS ) DebugTln(F("Netswitch task succesfully created"));
}

uint32_t uptimeSEC(){
    return (uint32_t)(esp_log_timestamp()/1000);
}

// uint32_t uptimeMIN(){
//   return (uint32_t) esp_timer_get_time()/60000000;
// }

bool loadNetSwitchConfig(const char *filename) {
  File file = LittleFS.open(filename, "r");
  if (!file) {
    Debugln("Config not found");
    return false;
  }
  docTriggers.clear();
  DeserializationError error = deserializeJson(docTriggers, file);
  if (error) {
    Debug("Error JSON-parse: ");Debugln(error.c_str());
    file.close();
    return false;
  }

  file.close();
  return true;
}

void readtriggers(){
  bNetSwitchConfigRead = loadNetSwitchConfig("/netswitch.json");
  if ( bNetSwitchConfigRead ) {
#ifdef DEBUG
    Debug("value.          : ");Debugln(docTriggers["value"].as<int>());
    Debug("switch_on.      : ");Debugln(docTriggers["switch_on"].as<bool>());
    Debug("time-true (sec) : ");Debugln(docTriggers["time_true"].as<int>());
    Debug("time-false (sec): ");Debugln(docTriggers["time_false"].as<int>());
    Debug("device name.    : ");Debugln(docTriggers["device"]["name"].as<const char*>());
    Debug("relay           : ");Debugln(docTriggers["device"]["relay"].as<int>());
    Debug("default state.  : ");Debugln(docTriggers["device"]["default"].as<int>());
#endif
  }
}

void NetSwitchStateMngr(){
  //every time new p1 values are available
  int32_t Phouse = DSMRdata.power_delivered.int_val() - DSMRdata.power_returned.int_val();
  Debugf("Phouse = %d\n",Phouse);

  if ( Phouse > docTriggers["value"].as<int>() ) {
    if ( !ShellyStateTrue ) {
      ShellyStateTrue = uptimeSEC();
      Debug("State True: ");Debugln(ShellyStateTrue);
      ShellyStateFalse = 0;
    }
  } else {
    if ( !ShellyStateFalse ) {
      ShellyStateTrue = 0;
      ShellyStateFalse = uptimeSEC();
      Debug("State False: ");Debugln(ShellyStateFalse);
    }
  }
  ShellyDevMngr();
}

void handleNetSwitch(){
  if ( bShellySwitch && bNetSwitchConfigRead ) {
    bShellySwitch = false;
    toggleNetSwitchSocket(lastToggleState);
  }
}

// / Functie om de Shelly Power Plug aan of uit te zetten
void toggleNetSwitchSocket(bool turnOn) {
  if ( netw_state != NW_NONE ) {
    HTTPClient http;
    String url = String("http://") + docTriggers["device"]["name"].as<const char*>() + String("/relay/") + docTriggers["device"]["relay"].as<const char*>() + String("?turn=") + (turnOn ? "on" : "off");
    Debug("url: ");Debugln(url);
    
    // API-aanroep
    http.begin(url);
    http.addHeader("Authorization", "Basic BASE64_ENCODED_CREDENTIALS");
    int httpResponseCode = http.GET();

    if (httpResponseCode == 200) {
      String payload = http.getString();
      Debug("response: ");Debugln(payload);
      Debug("Succes - Shelly Power Plug is ");
      Debugln(turnOn ? "on" : "off");
    } else {
      Debug("Error by API call, HTTP-code: ");
      Debugln(httpResponseCode);
    }
    http.end();
  } else {
    Debugln("WiFi not connected, unable to reach API.");
  }
}

void ShellyDevMngr(){
  bool newState = lastToggleState;
  if ( ShellyStateTrue && (uptimeSEC() - ShellyStateTrue) >= (docTriggers["time_true"].as<int>()) ) newState = docTriggers["switch_on"].as<bool>();
  else if ( ShellyStateFalse && (uptimeSEC() - ShellyStateFalse) >= (docTriggers["time_false"].as<int>()) ) newState = !docTriggers["switch_on"].as<bool>();
  
  //only action if changed
  if ( newState != lastToggleState ){
    Debugf("Shelly toggle to: %s\n",newState?"on":"off");
    // toggleShellySocket(newState);
    lastToggleState = newState;
    bShellySwitch = true;
  }
}

#else
  void SetupNetSwitch(){}
  void NetSwitchStateMngr(){}
#endif