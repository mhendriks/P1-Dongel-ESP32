/* 
***************************************************************************  
**  Program  : JsonCalls, part of DSMRloggerAPI
**  Version  : v4.0.0
**
**  Copyright (c) 2021 Martijn Hendriks
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/
#define ACTUALELEMENTS  19
#define INFOELEMENTS     4
#define FIELDELEMENTS    1

byte fieldsElements = 0;
char Onefield[25];
bool onlyIfPresent = false;

const static PROGMEM char infoArray[][25]   = { "identification","p1_version","equipment_id","electricity_tariff" };
const static PROGMEM char actualArray[][25] = { "timestamp","energy_delivered_tariff1","energy_delivered_tariff2","energy_returned_tariff1","energy_returned_tariff2","power_delivered","power_returned","voltage_l1","voltage_l2","voltage_l3","current_l1","current_l2","current_l3","power_delivered_l1","power_delivered_l2","power_delivered_l3","power_returned_l1","power_returned_l2","power_returned_l3" };

DynamicJsonDocument jsonDoc(4100);  // generic doc to return, clear() before use!

void JsonGas(){
  if (!gasDelivered) return;
  
  jsonDoc["gas_delivered"]["value"] =  gasDelivered;
  jsonDoc["gas_delivered"]["unit"]  = "m3";
}

void JsonWater(){
  if (!WtrMtr) return;
  
  jsonDoc["water"]["value"] =  (float)P1Status.wtr_m3 + P1Status.wtr_l/1000.0;
  jsonDoc["water"]["unit"]  = "m3";
}
//--------------------------
void JsonGasID(){
  switch (mbusGas) {
    case 1: if ( DSMRdata.mbus1_equipment_id_tc_present ) jsonDoc["gas_equipment_id"]["value"] =  DSMRdata.mbus1_equipment_id_tc;
            else if (DSMRdata.mbus1_equipment_id_ntc_present) jsonDoc["gas_equipment_id"]["value"] =  DSMRdata.mbus1_equipment_id_ntc;
            break;
    case 2: if ( DSMRdata.mbus2_equipment_id_tc_present ) jsonDoc["gas_equipment_id"]["value"] =  DSMRdata.mbus2_equipment_id_tc;
            else if (DSMRdata.mbus2_equipment_id_ntc_present) jsonDoc["gas_equipment_id"]["value"] =  DSMRdata.mbus2_equipment_id_ntc;
            break;
    case 3: if ( DSMRdata.mbus3_equipment_id_tc_present ) jsonDoc["gas_equipment_id"]["value"] =  DSMRdata.mbus3_equipment_id_tc;
            else if (DSMRdata.mbus3_equipment_id_ntc_present) jsonDoc["gas_equipment_id"]["value"] =  DSMRdata.mbus3_equipment_id_ntc;
            break;
    case 4: if ( DSMRdata.mbus4_equipment_id_tc_present ) jsonDoc["gas_equipment_id"]["value"] =  DSMRdata.mbus4_equipment_id_tc;
            else if (DSMRdata.mbus4_equipment_id_ntc_present) jsonDoc["gas_equipment_id"]["value"] =  DSMRdata.mbus4_equipment_id_ntc;
            break;
    default: break; //do nothing 
  }
}
//--------------------------

struct buildJson {
    
    template<typename Item>
    void apply(Item &i) {
      String Name = String(Item::name);
      if (isInFieldsArray(Name.c_str())) {
        if (i.present()) {          
          String Unit = Item::unit();
          jsonDoc[Name]["value"] = value_to_json(i.val());
          if (Unit.length() > 0) jsonDoc[Name]["unit"]  = Unit;
        }  else if (!onlyIfPresent) jsonDoc[Name]["value"] = "-";   
    } //infielsarrayname
  }
  
  template<typename Item>
  Item& value_to_json(Item& i) {
    return i;
  }

  String value_to_json(TimestampedFixedValue i) {
    return String(i);
  }
  
  float value_to_json(FixedValue i) {
    return i;
  }

}; // buildjson{} 
 
//=======================================================================
void processAPI() {
//  char fName[40] = "";
  char URI[50]   = "";
  String words[10];

  strncpy( URI, httpServer.uri().c_str(), sizeof(URI) );

  if (httpServer.method() == HTTP_GET)
        DebugTf("from[%s] URI[%s] method[GET] \r\n"
                                  , httpServer.client().remoteIP().toString().c_str()
                                        , URI); 
  else  DebugTf("from[%s] URI[%s] method[PUT] \r\n" 
                                  , httpServer.client().remoteIP().toString().c_str()
                                        , URI); 

if (bailout())
  {
      DebugTf("==> Bailout due to low heap (%d bytes))\r\n", ESP.getFreeHeap() );
      
    httpServer.send(500, "text/plain", "500: internal server error (low heap)\r\n"); 
    return;
  }

  int8_t wc = splitString(URI, '/', words, 10);
  
  if (Verbose2) 
  {
    DebugT(">>");
    for (int w=0; w<wc; w++)
    {
      Debugf("word[%d] => [%s], ", w, words[w].c_str());
    }
    Debugln(" ");
  }
  
  if (words[2] != "v2")
  {
    sendApiNotFound(URI);
    return;
  }

  if (words[3] == "dev")
  {
    handleDevApi(URI, words[4].c_str(), words[5].c_str(), words[6].c_str());
  }
  else if (words[3] == "hist")
  {
    handleHistApi(URI, words[4].c_str(), words[5].c_str(), words[6].c_str());
  }
  else if (words[3] == "sm")
  {
    handleSmApi(URI, words[4].c_str(), words[5].c_str(), words[6].c_str());
  }
  else sendApiNotFound(URI);
  
} // processAPI()

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
  
  if (Verbose1) serializeJsonPretty(doc,buffer); 
  else serializeJson(doc,buffer);
//  DebugT(F("Sending json: ")); Debugln(buffer);
//  DebugT("strsize:"); Debugln(strsize);
  sendJsonBuffer(buffer.c_str());
  DebugTln(F("sendJson: json sent .."));
    
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
  doc["fwversion"] = _VERSION;
  snprintf(cMsg, sizeof(cMsg), "%s %s", __DATE__, __TIME__);
  doc["compiled"] = cMsg;
  doc["smr_version"] = DSMR_NL?"NL":"BE";
  doc["hostname"] = settingHostname;
  doc["ipaddress"] = WiFi.localIP().toString();
  doc["indexfile"] = settingIndexPage;
  doc["macaddress"] = WiFi.macAddress();
  doc["freeheap"] ["value"] = ESP.getFreeHeap();
  doc["freeheap"]["unit"] = "bytes";
  doc["maxfreeblock"] ["value"] = ESP.getMaxAllocHeap(); 
  doc["maxfreeblock"]["unit"] = "bytes";
  
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  
  doc["chipid"] = WIFI_getChipId();
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
  doc["flashchipspeed"] ["value"] = formatFloat((ESP.getFlashChipSpeed() / 1000.0 / 1000.0), 0);
  doc["flashchipspeed"]["unit"] = "MHz";
 
  FlashMode_t ideMode = ESP.getFlashChipMode();
  doc["flashchipmode"] = flashMode[ideMode];
  doc["boardtype"] = ARDUINO_BOARD;
  doc["compileoptions"] = ALL_OPTIONS;
  doc["ssid"] = WiFi.SSID();

#ifdef SHOW_PASSWRDS
  doc["pskkey"] = WiFi.psk();
#endif
  doc["wifirssi"] = WiFi.RSSI();
  doc["uptime"] = upTime();
  doc["smhasfaseinfo"] = (int)settingSmHasFaseInfo;
  doc["telegraminterval"] = (int)settingTelegramInterval;
  doc["telegramcount"] = (int)telegramCount;
  doc["telegramerrors"] = (int)telegramErrors;

#ifdef USE_MQTT
  snprintf(cMsg, sizeof(cMsg), "%s:%04d", settingMQTTbroker, settingMQTTbrokerPort);
  doc["mqttbroker"] = cMsg;
  doc["mqttinterval"] = settingMQTTinterval;
  if (mqttIsConnected) doc["mqttbroker_connected"] = "yes";
  else  doc["mqttbroker_connected"] = "no";
#endif

  doc["reboots"] = (int)P1Status.reboots;
  doc["lastreset"] = lastReset;  

  sendJson(doc);
 
} // sendDeviceInfo()

//=======================================================================
void sendDeviceSettings() 
{
  DebugTln(F("sending device settings ...\r"));
  DynamicJsonDocument doc(1500);
  
  doc["hostname"]["value"] = settingHostname;
  doc["hostname"]["type"] = "s";
  doc["hostname"]["maxlen"] = sizeof(settingHostname) -1;
  
  doc["ed_tariff1"]["value"] = settingEDT1;
  doc["ed_tariff1"]["type"] = "f";
  doc["ed_tariff1"]["min"] = 0;
  doc["ed_tariff1"]["max"] = 10;
  
  doc["ed_tariff2"]["value"] = settingEDT2;
  doc["ed_tariff2"]["type"] = "f";
  doc["ed_tariff2"]["min"] = 0;
  doc["ed_tariff2"]["max"] = 10;
  
  doc["er_tariff1"]["value"] = settingERT1;
  doc["er_tariff1"]["type"] = "f";
  doc["er_tariff1"]["min"] = 0;
  doc["er_tariff1"]["max"] = 10;
  
  doc["er_tariff2"]["value"] = settingERT2;
  doc["er_tariff2"]["type"] = "f";
  doc["er_tariff2"]["min"] = 0;
  doc["er_tariff2"]["max"] = 10;
  
  doc["gd_tariff"]["value"] = settingGDT;
  doc["gd_tariff"]["type"] = "f";
  doc["gd_tariff"]["min"] = 0;
  doc["gd_tariff"]["max"] = 10;
  
  doc["electr_netw_costs"]["value"] = settingENBK;
  doc["electr_netw_costs"]["type"] = "f";
  doc["electr_netw_costs"]["min"] = 0;
  doc["electr_netw_costs"]["max"] = 100;
  
  doc["gas_netw_costs"]["value"] = settingGNBK;
  doc["gas_netw_costs"]["type"] = "f";
  doc["gas_netw_costs"]["min"] = 0;
  doc["gas_netw_costs"]["max"] = 100;
  
  doc["sm_has_fase_info"]["value"] = settingSmHasFaseInfo;
  doc["sm_has_fase_info"]["type"] = "i";
  doc["sm_has_fase_info"]["min"] = 0;
  doc["sm_has_fase_info"]["max"] = 1;
  
  doc["tlgrm_interval"]["value"] = settingTelegramInterval;
  doc["tlgrm_interval"]["type"] = "i";
  doc["tlgrm_interval"]["min"] = 2;
  doc["tlgrm_interval"]["max"] = 60;
  
  doc["IndexPage"]["value"] = settingIndexPage;
  doc["IndexPage"]["type"] = "s";
  doc["IndexPage"]["maxlen"] = sizeof(settingIndexPage) -1;
  
  doc["mqtt_broker"]["value"]  = settingMQTTbroker;
  doc["mqtt_broker"]["type"] = "s";
  doc["mqtt_broker"]["maxlen"] = sizeof(settingIndexPage) -1;
  
  doc["mqtt_broker_port"]["value"] = settingMQTTbrokerPort;
  doc["mqtt_broker_port"]["type"] = "i";
  doc["mqtt_broker_port"]["min"] = 1;
  doc["mqtt_broker_port"]["max"] = 9999;
  
  doc["mqtt_user"]["value"] = settingMQTTuser;
  doc["mqtt_user"]["type"] = "s";
  doc["mqtt_user"]["maxlen"] = sizeof(settingMQTTuser) -1;
  
  doc["mqtt_passwd"]["value"] = settingMQTTpasswd;
  doc["mqtt_passwd"]["type"] = "s";
  doc["mqtt_passwd"]["maxlen"] = sizeof(settingMQTTpasswd) -1;
  
  doc["mqtt_toptopic"]["value"] = settingMQTTtopTopic;
  doc["mqtt_toptopic"]["type"] = "s";
  doc["mqtt_toptopic"]["maxlen"] = sizeof(settingMQTTtopTopic) -1;
  
  doc["mqtt_interval"]["value"] = settingMQTTinterval;
  doc["mqtt_interval"]["type"] = "i";
  doc["mqtt_interval"]["min"] = 0;
  doc["mqtt_interval"]["max"] = 600;

  sendJson(doc);

} // sendDeviceSettings()

//====================================================
void sendApiNotFound(const char *URI)
{
  DebugTln(F("sending device settings ...\r"));
//  String output = "{\"error\":{\"url\":\"" + String(URI) + "\",\"message\":\"url not valid\"}}";
  
  byte len = 47 + strlen(URI);
  char buffer[len];
  sprintf_P(buffer,PSTR("{\"error\":{\"url\":\"%s\",\"message\":\"url not valid\"}}"), URI);
  
  httpServer.sendHeader("Access-Control-Allow-Origin", "*");
  httpServer.setContentLength(len);
  httpServer.send(404, "application/json", buffer); 
  
} // sendApiNotFound()

//====================================================

void handleSmApi(const char *URI, const char *word4, const char *word5, const char *word6)
{
  //DebugTf("word4[%s], word5[%s], word6[%s]\r\n", word4, word5, word6);
  switch (word4[0]) {
    
  case 'i': //info
    onlyIfPresent = false;
    fieldsElements = INFOELEMENTS;
    jsonDoc.clear();
    DSMRdata.applyEach(buildJson());
    JsonGasID();
    sendJson(jsonDoc);
  break;
  
  case 'a': //actual
    fieldsElements = ACTUALELEMENTS;
    onlyIfPresent = true;
    jsonDoc.clear();
    JsonGas();
    DSMRdata.applyEach(buildJson());
#ifdef USE_WATER_SENSOR    
    jsonDoc["water"]["value"] = P1Status.wtr_m3;
    jsonDoc["water"]["unit"]  = "m3";
#endif
    sendJson(jsonDoc);
  break;
  
  case 'f': //fields
    fieldsElements = 0;
    onlyIfPresent = false;
    if (strlen(word5) > 0)
    {
       strCopy(Onefield, 24, word5);
       fieldsElements = FIELDELEMENTS;
    }
    jsonDoc.clear();
    DSMRdata.applyEach(buildJson());
    if (strlen(word5) == 0) JsonGas();
    sendJson(jsonDoc);
    break;  
  case 't': //telegramm 
    { byte c = 0;
    while ((c < 19) && !slimmeMeter.available()){delay(100); c++;}
    String buff = slimmeMeter.raw();
    if (buff.length() > 50) sendJsonBuffer(&buff[0]);
    else httpServer.send(200, "application/plain", F("Empty telegram buffer, try again"));
    break; } 
  default:
    sendApiNotFound(URI);
  }
  
} // handleSmApi()
//====================================================

void handleDevApi(const char *URI, const char *word4, const char *word5, const char *word6)
{
  //DebugTf("word4[%s], word5[%s], word6[%s]\r\n", word4, word5, word6);
  if (strcmp(word4, "info") == 0)
  {
    sendDeviceInfo();
  }
  else if (strcmp(word4, "time") == 0)
  {
    sendDeviceTime();
  }
  else if (strcmp(word4, "settings") == 0)
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
  else if (strcmp(word4, "debug") == 0)
  {
    sendDeviceDebug(URI, word5);
  }
  else sendApiNotFound(URI);
  
} // handleDevApi()

//====================================================
void handleHistApi(const char *URI, const char *word4, const char *word5, const char *word6)
{

  //DebugTf("word4[%s], word5[%s], word6[%s]\r\n", word4, word5, word6);
  if (   strcmp(word4, "hours") == 0 )
  {
    RingFileTo(RINGHOURS, true);
    return;
  }
  else if (strcmp(word4, "days") == 0 )
  {
    RingFileTo(RINGDAYS, true);
    return;

  }
  else if (strcmp(word4, "months") == 0)
  {
    if (httpServer.method() == HTTP_PUT || httpServer.method() == HTTP_POST)
    {
      //------------------------------------------------------------ 
      // json string: {"recid":"29013023"
      //               ,"edt1":2601.146,"edt2":"9535.555"
      //               ,"ert1":378.074,"ert2":208.746
      //               ,"gdt":3314.404}
      //------------------------------------------------------------ 
    
      //--- update MONTHS
      writeRingFile(RINGMONTHS, httpServer.arg(0).c_str());
      return;
    }
    else 
    {
      RingFileTo(RINGMONTHS,true);
      return;
    }
  }
  else 
  {
    sendApiNotFound(URI);
    return;
  }

} // handleHistApi()

//=======================================================================
void sendDeviceDebug(const char *URI, String tail) 
{
  sendApiNotFound(URI);

} // sendDeviceDebug()

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
