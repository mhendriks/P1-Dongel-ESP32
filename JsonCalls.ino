/* 
***************************************************************************  
**  Program  : JsonCalls, part of DSMRloggerAPI
**
**  Copyright (c) 2023 Martijn Hendriks
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/
#define ACTUALELEMENTS  22
#define INFOELEMENTS     3
#define FIELDELEMENTS    1

byte fieldsElements = 0;
char Onefield[25];
bool onlyIfPresent = false;

const static PROGMEM char infoArray[][25]   = { "identification","p1_version","equipment_id" }; //waardes dient redelijk statisch zijn niet elke keer versturen
#ifndef SE_VERSION
  const static PROGMEM char actualArray[][25] = { "timestamp","electricity_tariff","energy_delivered_tariff1","energy_delivered_tariff2","energy_returned_tariff1","energy_returned_tariff2","power_delivered","power_returned","voltage_l1","voltage_l2","voltage_l3","current_l1","current_l2","current_l3","power_delivered_l1","power_delivered_l2","power_delivered_l3","power_returned_l1","power_returned_l2","power_returned_l3","peak_pwr_last_q", "highest_peak_pwr"};
#else
  const static PROGMEM char actualArray[][25] = { "timestamp","electricity_tariff","energy_delivered_total","energy_delivered_tariff2","energy_returned_total","energy_returned_tariff2","power_delivered","power_returned","voltage_l1","voltage_l2","voltage_l3","current_l1","current_l2","current_l3","power_delivered_l1","power_delivered_l2","power_delivered_l3","power_returned_l1","power_returned_l2","power_returned_l3" };
#endif

DynamicJsonDocument jsonDoc(4100);  // generic doc to return, clear() before use!

void JsonGas(){
  if (!gasDelivered) return;
  jsonDoc["gas_delivered"]["value"] =  (int)(gasDelivered*1000)/1000.0;
  jsonDoc["gas_delivered"]["unit"]  = "m3";
  jsonDoc["gas_delivered_timestamp"]["value"] = gasDeliveredTimestamp;
}

void JsonWater(){

  if ( !WtrMtr && !mbusWater ) return;  
  if ( mbusWater) {
    jsonDoc["water"]["value"] =  (int)(waterDelivered*1000)/1000.0;
    jsonDoc["water_delivered_ts"]["value"] = waterDeliveredTimestamp;
  } else {
    jsonDoc["water"]["value"] =  (float)P1Status.wtr_m3 + (P1Status.wtr_l?P1Status.wtr_l/1000.0:0);
  } 
  jsonDoc["water"]["unit"]  = "m3";
}

//--------------------------

struct buildJson {
    
    template<typename Item>
    void apply(Item &i) {
     char Name[25];
     strncpy(Name,String(Item::name).c_str(),25);

      if (isInFieldsArray(Name)) {
        
        #ifdef SE_VERSION     
        if (strcmp(Name, "energy_delivered_total") == 0) strcpy(Name,"energy_delivered_tariff1");
        else if (strcmp(Name, "energy_returned_total") == 0) strcpy(Name,"energy_returned_tariff1");
        #endif
        
        if (i.present()) {          
          jsonDoc[Name]["value"] = value_to_json(i.val());
          if (String(Item::unit()).length() > 0) jsonDoc[Name]["unit"]  = Item::unit();
        }  else if (!onlyIfPresent) jsonDoc[Name]["value"] = "-";   
    } //infielsarrayname
  }
  
  template<typename Item>
  Item& value_to_json(Item& i) {
    return i;
  }

  double value_to_json(TimestampedFixedValue i) {
    return i.int_val()/1000.0;
  }
  
  double value_to_json(FixedValue i) {
    return i.int_val()/1000.0;
  }

}; // buildjson{} 
 
template <typename TSource>
void sendJson(const TSource &doc) 
{  
  //const size_t strsize = measureJson(doc)+1;
  if (doc.isNull()) {
//    DebugT(F("sendjson isNull"));
    sendJsonBuffer("{}");
    return;
  }
//  char buffer[strsize];
  String buffer;
  
//  if (Verbose1) serializeJsonPretty(doc,buffer); 
//  else 
  serializeJson(doc,buffer);
//  DebugT(F("Sending json: ")); Debugln(buffer);
//  DebugT("strsize:"); Debugln(strsize);
  sendJsonBuffer(buffer.c_str());
//  if (Verbose1) DebugTln(F("sendJson: json sent .."));
    
}

void sendJsonBuffer(const char* buffer){
  httpServer.sendHeader("Access-Control-Allow-Origin", "*");
  httpServer.setContentLength(strlen(buffer));
  httpServer.send(200, "application/json", buffer);
} //sendJsonBuffer

void sendDeviceTime() 
{
   char buffer[80];
  //  {"timestamp":"201201074021S","time":"2020-12-01 07:40:21","epoch":1606808619}
  sprintf_P(buffer,PSTR("{\"timestamp\":\"%s\",\"time\":\"%s\",\"epoch\":%d}"), actTimestamp, buildDateTimeString(actTimestamp, sizeof(actTimestamp)).c_str() , now() );
  sendJsonBuffer(buffer);
  
} // sendDeviceTime()

void sendDeviceInfo() 
{
  DynamicJsonDocument doc(1500);
  doc["fwversion"] = Firmware.Version;
//  snprintf(cMsg, sizeof(cMsg), "%s %s", __DATE__, __TIME__);
  doc["compiled"] = __DATE__ " " __TIME__;

//#ifndef HEATLINK
  if ( ! bWarmteLink ) { // IF NOT HEATLINK
    doc["smr_version"] = DSMR_NL?"NL":"BE";
  }
//#endif
  
  doc["hostname"] = settingHostname;
  doc["ipaddress"] = IP_Address();
  doc["indexfile"] = settingIndexPage;
  doc["macaddress"] = macStr;
  doc["freeheap"] ["value"] = ESP.getFreeHeap();
  doc["freeheap"]["unit"] = "bytes";
//  doc["maxfreeblock"] ["value"] = ESP.getMaxAllocHeap(); 
//  doc["maxfreeblock"]["unit"] = "bytes";
  
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);

  doc["chipid"] = _getChipId();
  snprintf(cMsg, sizeof(cMsg), "model %x rev: %x cores: %x", chip_info.model, chip_info.revision, chip_info.cores);
  doc["coreversion"] = cMsg;
  doc["sdkversion"] = String( ESP.getSdkVersion() );
  doc["cpufreq"] ["value"] = ESP.getCpuFreqMHz();
  doc["cpufreq"]["unit"] = "MHz";
  doc["sketchsize"] ["value"] = formatFloat( (ESP.getSketchSize() / 1024.0), 3);
  doc["sketchsize"]["unit"] = "kB";
  doc["freesketchspace"] ["value"] = formatFloat( (ESP.getFreeSketchSpace() / 1024.0), 3);
  doc["freesketchspace"]["unit"] = "kB";
  doc["flashchipsize"] ["value"] = formatFloat((ESP.getFlashChipSize() / 1024.0 / 1024.0), 3);
  doc["flashchipsize"]["unit"] = "MB";
  doc["FSsize"] ["value"] = formatFloat( (LittleFS.totalBytes() / (1024.0 * 1024.0)), 0);
  doc["FSsize"]["unit"] = "MB";
//  doc["flashchipspeed"] ["value"] = formatFloat((ESP.getFlashChipSpeed() / 1000.0 / 1000.0), 0);
//  doc["flashchipspeed"]["unit"] = "MHz";
 
//  FlashMode_t ideMode = ESP.getFlashChipMode();
//  doc["flashchipmode"] = flashMode[ideMode];
//  doc["boardtype"] = ARDUINO_BOARD;
  doc["compileoptions"] = ALL_OPTIONS;

#ifndef ETHERNET
  doc["ssid"] = WiFi.SSID();
  // doc["pskkey"] = WiFi.psk();
  doc["wifirssi"] = WiFi.RSSI();
#endif
  doc["uptime"] = upTime();

  if ( !bWarmteLink ) { // IF NO HEATLINK
      doc["smhasfaseinfo"] = (int)settingSmHasFaseInfo;
  }
  
//  doc["telegraminterval"] = (int)settingTelegramInterval;
  if ( DSMRdata.p1_version == "50" || !DSMR_NL ) doc["telegraminterval"] = 1; 
  else doc["telegraminterval"] = 10; 

  doc["telegramcount"] = (int)telegramCount;
  doc["telegramerrors"] = (int)telegramErrors;

#ifndef MQTT_DISABLE
  snprintf(cMsg, sizeof(cMsg), "%s:%04d", settingMQTTbroker, settingMQTTbrokerPort);
  doc["mqttbroker"] = cMsg;
  doc["mqttinterval"] = settingMQTTinterval;
  if (MQTTclient.connected()) doc["mqttbroker_connected"] = "yes";
  else  doc["mqttbroker_connected"] = "no";
#endif

  doc["reboots"] = (int)P1Status.reboots;
  doc["lastreset"] = lastReset;  

  sendJson(doc);
 
} // sendDeviceInfo()

//=======================================================================
void sendDeviceSettings() {
  DebugTln(F("sending device settings ...\r"));
  DynamicJsonDocument doc(2100);

  // Helper macro to add a setting to the JSON document
#define ADD_SETTING(name, type, min, max, value) \
  doc[name]["value"] = value;                     \
  doc[name]["type"] = type;                       \
  doc[name]["min"] = min;                         \
  doc[name]["max"] = max

  ADD_SETTING("hostname", "s", 0, sizeof(settingHostname) - 1, settingHostname);
  if ( !bWarmteLink ) { // IF NO HEATLINK
  ADD_SETTING("ed_tariff1", "f", 0, 10, settingEDT1);
  ADD_SETTING("ed_tariff2", "f", 0, 10, settingEDT2);
  ADD_SETTING("er_tariff1", "f", 0, 10, settingERT1);
  ADD_SETTING("er_tariff2", "f", 0, 10, settingERT2);
  ADD_SETTING("w_tariff", "f", 0, 10, settingWDT);
  }
  ADD_SETTING("gd_tariff", "f", 0, 10, settingGDT);
  if ( ! bWarmteLink ) { // IF NO HEATLINK
    ADD_SETTING("electr_netw_costs", "f", 0, 100, settingENBK);
    ADD_SETTING("water_netw_costs", "f", 0, 100, settingWNBK);
  }
  ADD_SETTING("gas_netw_costs", "f", 0, 100, settingGNBK);
  if ( ! bWarmteLink ) { // IF NO HEATLINK
    ADD_SETTING("sm_has_fase_info", "i", 0, 1, settingSmHasFaseInfo);
  }
  ADD_SETTING("IndexPage", "s", 0, sizeof(settingIndexPage) - 1, settingIndexPage);

#ifndef MQTT_DISABLE
#ifndef MQTTKB
  ADD_SETTING("mqtt_broker", "s", 0, sizeof(settingMQTTbroker) - 1, settingMQTTbroker);
  ADD_SETTING("mqtt_broker_port", "i", 1, 9999, settingMQTTbrokerPort);
  ADD_SETTING("mqtt_user", "s", 0, sizeof(settingMQTTuser) - 1, settingMQTTuser);
  ADD_SETTING("mqtt_passwd", "s", 0, sizeof(settingMQTTpasswd) - 1, settingMQTTpasswd);
  doc["mqtt_tls"] = bMQTToverTLS;
#endif
  ADD_SETTING("mqtt_toptopic", "s", 0, sizeof(settingMQTTtopTopic) - 1, settingMQTTtopTopic);
  ADD_SETTING("mqtt_interval", "i", 0, 600, settingMQTTinterval);
#endif

  if (WtrMtr) {
    ADD_SETTING("water_m3", "i", 0, 99999, P1Status.wtr_m3);
    ADD_SETTING("water_l", "i", 0, 999, P1Status.wtr_l);
    ADD_SETTING("water_fact", "f", 0, 10, WtrFactor);
  }

  if (strncmp(BaseOTAurl, "http://", 7) == 0) {
    char ota_url[sizeof(BaseOTAurl)];
    strncpy(ota_url, BaseOTAurl + 7, strlen(BaseOTAurl));
    ADD_SETTING("ota_url", "s", 0, sizeof(ota_url) - 1, ota_url);
  } else {
    ADD_SETTING("ota_url", "s", 0, sizeof(BaseOTAurl) - 1, BaseOTAurl);
  }

  ADD_SETTING("b_auth_user", "s", 0, sizeof(bAuthUser) - 1, bAuthUser);
  ADD_SETTING("b_auth_pw", "s", 0, sizeof(bAuthPW) - 1, bAuthPW);

  //booleans
  doc["hist"] = EnableHistory;
  doc["water_enabl"] = WtrMtr;
  doc["led"] = LEDenabled;
  doc["raw-port"] = bRawPort;
  doc["eid-enabled"] = bEID_enabled;
#ifdef DEV_PAIRING
  doc["dev-pairing"] = true;
#endif

  doc["ha_disc_enabl"] = EnableHAdiscovery;
if ( P1Status.dev_type == PRO_BRIDGE ) doc["led-prt"] = bLED_PRT;

  if ( bWarmteLink ) { // IF HEATLINK
    doc["conf"] = "p1-q";
  } else {
    doc["pre40"] = bPre40;
    doc["conf"] = "p1-p";
  }

  // doc["auto_update"] = bAutoUpdate;  TO DO ...

  sendJson(doc);
}

//====================================================
void sendApiNotFound() {
  
  httpServer.send(404, "application/json", "{\"error\":{\"url\":\"" + httpServer.uri() + "\",\"message\":\"url not valid\"}}");  

} // sendApiNotFound()


//====================================================
void handleSmApiField(){
    onlyIfPresent = false;
    strCopy(Onefield, 24, httpServer.pathArg(0).c_str());
    fieldsElements = FIELDELEMENTS;
    jsonDoc.clear();
    DSMRdata.applyEach(buildJson());
    sendJson(jsonDoc);
}

void handleSmApi()
{
  switch ( httpServer.pathArg(0)[0]) {
    
  case 'i': //info
    onlyIfPresent = false;
    fieldsElements = INFOELEMENTS;
    break;
  
  case 'a': //actual
    fieldsElements = ACTUALELEMENTS;
    onlyIfPresent = true;
    break;
  
  case 'f': //fields
    fieldsElements = 0;
    onlyIfPresent = false;
    break;  
    
  case 't': //telegram
    if ( CapTelegram.length() ) {
        sendJsonBuffer( CapTelegram.c_str() );
    }
    else sendJsonBuffer( "no telegram available" );
    return;
    
  default:
    sendApiNotFound();
    return;
    
  } //switch

    jsonDoc.clear();
    DSMRdata.applyEach(buildJson());
    JsonGas();
    JsonWater();
    sendJson(jsonDoc);
    
} // handleSmApi()
//====================================================

void handleDevApi()
{
   if ( httpServer.pathArg(0) == "info" )
  {
    sendDeviceInfo();
  }
  else if ( httpServer.pathArg(0) == "time")
  {
    sendDeviceTime();
  }
  else if (httpServer.pathArg(0) == "settings")
  {
    if (httpServer.method() == HTTP_PUT || httpServer.method() == HTTP_POST)
    {
      //------------------------------------------------------------ 
      // json string: {"name":"settingInterval","value":9}  
      // json string: {"name":"settingTelegramInterval","value":123.45}  
      // json string: {"name":"settingTelegramInterval","value":"abc"}  
      //------------------------------------------------------------ 
      // so, why not use ArduinoJSON library?
      // I say: try it yourself ;-) It won't be easy
      String wOut[5];
      String wPair[5];
      String jsonIn  = httpServer.arg(0).c_str();
      DebugT("json in :");Debugln(jsonIn);
      char field[25] = "";
      char newValue[101]="";
      jsonIn.replace("{", "");
      jsonIn.replace("}", "");
      jsonIn.replace("\"", "");
      int8_t wp = splitString(jsonIn.c_str(), ',',  wPair, 5) ;
      for (int i=0; i<wp; i++)
      {
        //DebugTf("[%d] -> pair[%s]\r\n", i, wPair[i].c_str());
        int8_t wc = splitString(wPair[i].c_str(), ':',  wOut, 5) ;
        //DebugTf("==> [%s] -> field[%s]->val[%s]\r\n", wPair[i].c_str(), wOut[0].c_str(), wOut[1].c_str());
        if (wOut[0].equalsIgnoreCase("name"))  strCopy(field, sizeof(field), wOut[1].c_str());
        if (wOut[0].equalsIgnoreCase("value")) strCopy(newValue, sizeof(newValue), wOut[1].c_str());
      }
      //DebugTf("--> field[%s] => newValue[%s]\r\n", field, newValue);
      updateSetting(field, newValue);
      httpServer.send(200, "application/json", httpServer.arg(0));
    }
    else
    {
      sendDeviceSettings();
    }
  }
  else sendApiNotFound();
  
} // handleDevApi()

bool isInFieldsArray(const char* lookUp)
{                        
//    DebugTf("Elemts[%2d] | LookUp [%s] | LookUpType[%2d]\r\n", elemts, lookUp, LookUpType);
  if (fieldsElements == 0) return true;  
  for (int i=0; i<fieldsElements; i++)
  {
    //if (Verbose2) DebugTf("[%2d] Looking for [%s] in array[%s]\r\n", i, lookUp, fieldsArray[i]); 
    if (fieldsElements == ACTUALELEMENTS ) { if (strcmp_P(lookUp, actualArray[i]) == 0 ) return true; }
    else if (fieldsElements == INFOELEMENTS ) { if (strcmp_P(lookUp, infoArray[i]) == 0 ) return true; }
    else if (fieldsElements == FIELDELEMENTS ) { if ( (strcmp(lookUp, Onefield) == 0) || (strcmp(lookUp, "timestamp") == 0)  ) return true; }
  }

  return false; 
} // isInFieldsArray()

/***************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the
* following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
* 
***************************************************************************/
