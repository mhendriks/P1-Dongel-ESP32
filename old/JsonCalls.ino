/* 
***************************************************************************  
**  Program  : JsonCalls, part of DSMRloggerAPI
**  Version  : v3.0.0
**
**  Copyright (c) 2021 Martijn Hendriks
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

// ******* Global Vars *******
#define ACTUALELEMENTS  20
#define INFOELEMENTS  6
int  fieldsElements = 0;
char fieldsArray[2][25] = {"timestamp", "word5"}; // to lookup fields 
char OneRecord[2300]= "";

bool onlyIfPresent  = false;

const static PROGMEM char infoArray[][25]   = { "identification","p1_version","equipment_id","electricity_tariff","gas_device_type","gas_equipment_id" };
const static PROGMEM char actualArray[][25] = { "timestamp","energy_delivered_tariff1","energy_delivered_tariff2","energy_returned_tariff1","energy_returned_tariff2","power_delivered","power_returned","voltage_l1","voltage_l2","voltage_l3","current_l1","current_l2","current_l3","power_delivered_l1","power_delivered_l2","power_delivered_l3","power_returned_l1","power_returned_l2","power_returned_l3","gas_delivered"};
 
//=======================================================================
void processAPI() {
  char fName[40] = "";
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

#ifdef USE_SYSLOGGER
  if (ESP.getFreeHeap() < 5000) // to prevent firmware from crashing!
#else
  if (ESP.getFreeHeap() < 8500) // to prevent firmware from crashing!
#endif
  {
      DebugTf("==> Bailout due to low heap (%d bytes))\r\n", ESP.getFreeHeap() );
      writeToSysLog("from[%s][%s] Bailout low heap (%d bytes)"
                                    , httpServer.client().remoteIP().toString().c_str()
                                    , URI
                                    , ESP.getFreeHeap() );
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
  const size_t strsize = measureJson(doc)+1;
  char buffer[strsize];
  
  if (Verbose1) serializeJsonPretty(doc,buffer,strsize); 
  else serializeJson(doc,buffer,strsize);
//  DebugT(F("Sending json: ")); Debugln(buffer);
//  DebugT("strsize:"); Debugln(strsize);
  sendJsonBuffer(buffer);
  DebugTln(F("sendJson: json sent .."));
    
}
void sendJsonBuffer(char* buffer){
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
  doc["fwversion"] = _FW_VERSION;
  snprintf(cMsg, sizeof(cMsg), "%s %s", __DATE__, __TIME__);
  doc["compiled"] = cMsg;
  doc["hostname"] = settingHostname;
  doc["ipaddress"] = WiFi.localIP().toString();
  doc["indexfile"] = settingIndexPage;
  doc["macaddress"] = WiFi.macAddress();
  doc["freeheap"] ["value"] = ESP.getFreeHeap();
  doc["freeheap"]["unit"] = "bytes";
  doc["maxfreeblock"] ["value"] = ESP.getMaxAllocHeap(); //esp32 aanpassing
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
  doc["LITTLEFSsize"] ["value"] = formatFloat( (LITTLEFS.totalBytes() / (1024.0 * 1024.0)), 0);
  doc["LITTLEFSsize"]["unit"] = "MB";
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
  if (mqttIsConnected)
        doc["mqttbroker_connected"] = "yes";
  else  doc["mqttbroker_connected"] = "no";
#endif

  doc["reboots"] = (int)nrReboots;
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
//---------------------------------------------------------------
void FillJsonRec(const char *cName, String sValue)
{
  char noUnit[] = {'\0'};
 
  if (sValue.length() > (JSON_BUFF_MAX - 60) )
  {
    FillJsonRec(cName, sValue.substring(0,(JSON_BUFF_MAX - (strlen(cName) + 30))), noUnit);
  }
  else
  {
    FillJsonRec(cName, sValue, noUnit);
  }
  
} // FillJsonRec(*char, String)

//=======================================================================
void FillJsonRec(const char *cName, const char *cValue, const char *cUnit)
{
    //DebugTln("const char *cName, const char *cValue, const char *cUnit");

  if (strlen(cUnit) == 0)
  {
    snprintf(OneRecord, sizeof(OneRecord)-1, "%s\"%s\":{\"value\":\"%s\"},"
                                      ,OneRecord, cName, cValue);
  }
  else
  {
    snprintf(OneRecord, sizeof(OneRecord)-1, "%s\"%s\":{\"value\":\"%s\",\"unit\":\"%s\"},"
                                      ,OneRecord, cName, cValue, cUnit);
  }

} // FillJsonRec(*char, *char, *char)

//---------------------------------------------------------------
void FillJsonRec(const char *cName, const char *cValue)
{
    //DebugTln(F("const char *cName, const char *cValue"));
    char noUnit[] = {'\0'};

  FillJsonRec(cName, cValue, noUnit);
  
} // FillJsonRec(*char, *char)


//=======================================================================
void FillJsonRec(const char *cName, String sValue, const char *cUnit)
{
  //DebugTln(F("const char *cName, String sValue, const char *cUnit "));

  if (sValue.length() > (JSON_BUFF_MAX - 65) )
  {
    DebugTf("[2] sValue.length() [%d]\r\n", sValue.length());
  }
  
  if (strlen(cUnit) == 0)
  {
    snprintf(OneRecord, sizeof(OneRecord)-1, "%s\"%s\":{\"value\":\"%s\"},"
                                      ,OneRecord, cName, sValue.c_str());
  }
  else
  {
    snprintf(OneRecord, sizeof(OneRecord)-1, "%s\"%s\":{\"value\":\"%s\",\"unit\":\"%s\"},"
                                      ,OneRecord, cName, sValue.c_str(), cUnit);
  }

} // FillJsonRec(*char, String, *char)

//---------------------------------------------------------------
void FillJsonRec(const char *cName, float fValue)
{
        //DebugTln(F("const char *cName, float fValue"));

  char noUnit[] = {'\0'};

  FillJsonRec(cName, fValue, noUnit);
  
} // FillJsonRec(*char, float)

//=======================================================================
void FillJsonRec(const char *cName, float fValue, const char *cUnit)
{  
      //DebugTln(F("const char *cName, float fValue, const char *cUnit"));

  if (strlen(cUnit) == 0)
  {
    snprintf(OneRecord, sizeof(OneRecord)-1, "%s\"%s\":{\"value\":%.3f},"
                                      ,OneRecord, cName, fValue);
  }
  else
  {
    snprintf(OneRecord, sizeof(OneRecord)-1, "%s \"%s\":{\"value\":%.3f,\"unit\":\"%s\"},"
                                      ,OneRecord, cName, fValue, cUnit);
  }

} // FillJsonRec(*char, float, *char)

//=======================================================================

struct buildJsonApi {
    bool  skip = false;

    template<typename Item>
    void apply(Item &i) {
      skip = false;
      char Name[25];sprintf(Name,"%s",Item::name);
//      String Name; strcpy(Name, Item::name);
  
      if (!isInFieldsArray(Name))
      {
        skip = true;
      }
      if (!skip)
      {
        if (i.present()) 
        {
      char Unit[10];sprintf(Unit,"%s",Item::unit());
//          String Unit;strcpy(Unit, Item::unit());
          if (strlen(Unit) > 0) FillJsonRec(Name, typecastValue(i.val()), Unit);
          else  FillJsonRec(Name, typecastValue(i.val()));
        }
        else if (!onlyIfPresent) FillJsonRec(Name, "-");
        //printf("OneRecord: %s \n\r", OneRecord);
      }
  }

};  // buildJsonApi()

void ArrayToJson(){
  //send json output
  
  if (strlen(OneRecord) < 4 ) { //some relevant data should be available
    OneRecord[1] = '}';
    OneRecord[2] = '\0';
  } else OneRecord[strlen(OneRecord)-1] = '}'; //replace "," with json terminating char
  
  DebugTln(F("Json sent."));
  sendJsonBuffer(OneRecord); 
}    

//====================================================
void handleSmApi(const char *URI, const char *word4, const char *word5, const char *word6)
{
  char    tlgrm[1200] = "";
  bool    stopParsingTelegram = false;
  memset(OneRecord,0,sizeof(OneRecord));//clear onerecord
  OneRecord[0] = '{'; //start Json
  
  //DebugTf("word4[%s], word5[%s], word6[%s]\r\n", word4, word5, word6);
  switch (word4[0]) {
    
  case 'i': //info
    onlyIfPresent = false;
    fieldsElements = INFOELEMENTS;
    DSMRdata.applyEach(buildJsonApi());
    ArrayToJson() ;
  break;
  
  case 'a': //actual
    fieldsElements = ACTUALELEMENTS;
    onlyIfPresent = true;
    DSMRdata.applyEach(buildJsonApi());
    ArrayToJson();
  break;
  
  case 'f': //fields
    fieldsElements = 0;
    onlyIfPresent = false;
    if (strlen(word5) > 0)
    {
       strCopy(fieldsArray[1], 24, word5);
       fieldsElements = 2;
    }
    DSMRdata.applyEach(buildJsonApi());
    ArrayToJson();
  break;  
  case 't': //telegramm 
  {
    showRaw = true;
    slimmeMeter.enable(true);
    Serial.setTimeout(5000);  // 5 seconds must be enough ..
    memset(tlgrm, 0, sizeof(tlgrm));
    int l = 0;
    // The terminator character is discarded from the serial buffer.
    l = Serial.readBytesUntil('/', tlgrm, sizeof(tlgrm));
    // now read from '/' to '!'
    // The terminator character is discarded from the serial buffer.
    l = Serial.readBytesUntil('!', tlgrm, sizeof(tlgrm));
    Serial.setTimeout(1000);  // seems to be the default ..
    DebugTf("read [%d] bytes\r\n", l);
    if (l == 0) 
    {
      httpServer.send(200, "application/plain", "no telegram received");
      showRaw = false;
      return;
    }

    tlgrm[l++] = '!';
    tlgrm[l++]    = '\r';
    tlgrm[l++]    = '\n';
    tlgrm[(l +1)] = '\0';
    // shift telegram 1 char to the right (make room at pos [0] for '/')
    for (int i=strlen(tlgrm); i>=0; i--) { tlgrm[i+1] = tlgrm[i]; yield(); }
    tlgrm[0] = '/'; 
    showRaw = false;
    if (Verbose1) Debugf("Telegram (%d chars):\r\n/%s", strlen(tlgrm), tlgrm);
    //httpServer.send(200, "application/plain", tlgrm);
    sendJsonBuffer(tlgrm);
    break; 
    } 
  default:
    sendApiNotFound(URI);
    break;
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
      writeToSysLog("DSMReditor: Field[%s] changed to [%s]", field, newValue);
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
#ifdef USE_SYSLOGGER
  String lLine = "";
  int lineNr = 0;
  int tailLines = tail.toInt();

  DebugTf("list [%d] debug lines\r\n", tailLines);
  sysLog.status();
  sysLog.setDebugLvl(0);
  httpServer.sendHeader("Access-Control-Allow-Origin", "*");
  httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  if (tailLines > 0)
        sysLog.startReading((tailLines * -1));  
  else  sysLog.startReading(0, 0);  
  while( (lLine = sysLog.readNextLine()) && !(lLine == "EOF")) 
  {
    lineNr++;
    snprintf(cMsg, sizeof(cMsg), "%s\r\n", lLine.c_str());
    httpServer.sendContent(cMsg);

  }
  sysLog.setDebugLvl(1);

#else
  sendApiNotFound(URI);
#endif

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
    else if (fieldsElements == 2 ) { if (strcmp(lookUp, fieldsArray[i]) == 0) return true; }
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
