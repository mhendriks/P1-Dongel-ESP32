/*
***************************************************************************  
**  Program  : settings_status_files, part of DSMRloggerAPI
**  Version  : v4.2.1
**
**  Copyright (c) 2023 Martijn Hendriks
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

void GetFile(String filename){
  HTTPClient http;
  if(wifiClient.connect(HOST_DATA_FILES, 443)) {
      http.begin(PATH_DATA_FILES + filename);
      int httpResponseCode = http.GET();
//      Serial.print(F("HTTP Response code: "));Serial.println(httpResponseCode);
      if (httpResponseCode == 200 ){
        String payload = http.getString();
  //      Serial.println(payload);
        File file = LittleFS.open(filename, "w"); // open for reading and writing
        if (!file) DebugTln(F("open file FAILED!!!\r\n"));
        else file.print(payload); 
        file.close();
      }
      http.end(); 
      wifiClient.stop(); //end client connection to server  
  } else {
    DebugTln(F("connection to server failed"));
  }
}

template <typename TSource>
void writeToJsonFile(const TSource &doc, File &_file) 
{
  if (!serializeJson(doc, _file) ||!FSmounted) {
      DebugTln(F("write(): Failed to write to json file"));
      return;  
  } 
  else {
    DebugTln(F("write(): json file writen"));
    //verbose write output
    if (Verbose1) 
    {
      DebugTln(F("Save to json file:"));
      serializeJson(doc, TelnetStream); //print settingsfile to telnet output
      serializeJson(doc, SerialOut); //print settingsfile to serial output    
    } // Verbose1  
  }
    
  _file.flush();
  _file.close(); 
}

//=======================================================================
void writeSettings() 
{
  StaticJsonDocument<1200> doc; 
  if (!FSmounted) return;

  DebugT(F("Writing to [")); Debug(SETTINGS_FILE); Debugln(F("] ..."));
  
  File SettingsFile = LittleFS.open(SETTINGS_FILE, "w"); // open for reading and writing
  
  if (!SettingsFile) 
  {
    DebugTf("open(%s, 'w') FAILED!!! --> Bailout\r\n", SETTINGS_FILE);
    return;
  }

  if (strlen(settingIndexPage) < 7) strCopy(settingIndexPage, (sizeof(settingIndexPage) -1), _DEFAULT_HOMEPAGE);
//  byte min_value = bPre40?MIN_T_INTV_PRE40:MIN_TELEGR_INTV;
//  if (settingTelegramInterval < min_value)  settingTelegramInterval = min_value;
  if (settingMQTTbrokerPort < 1)    settingMQTTbrokerPort = 1883;
    
  DebugTln(F("Start writing setting data to json settings file"));
  doc["Hostname"] = settingHostname;
  doc["EnergyDeliveredT1"] = settingEDT1;
  doc["EnergyDeliveredT2"] = settingEDT2;
  doc["EnergyReturnedT1"] = settingERT1;
  doc["EnergyReturnedT2"] = settingERT2;
  doc["GASDeliveredT"] = settingGDT;
  doc["EnergyVasteKosten"] = settingENBK;
  doc["GasVasteKosten"] = settingGNBK;
  doc["SmHasFaseInfo"] = settingSmHasFaseInfo;
//  doc["TelegramInterval"] = settingTelegramInterval;
  doc["IndexPage"] = settingIndexPage;
  yield();
  doc["MQTTbroker"] = settingMQTTbroker;
  doc["MQTTbrokerPort"] = settingMQTTbrokerPort;
  doc["MQTTUser"] = settingMQTTuser;
  doc["MQTTpasswd"] = settingMQTTpasswd;
  doc["MQTTinterval"] = settingMQTTinterval;
  doc["MQTTtopTopic"] = settingMQTTtopTopic;
  
  doc["LED"] = LEDenabled;
  doc["ota"] = BaseOTAurl;
  doc["enableHistory"] = EnableHistory;
  doc["watermeter"] = WtrMtr;
  doc["waterfactor"] = WtrFactor;
  doc["HAdiscovery"] = EnableHAdiscovery;
  doc["basic-auth"]["user"] = bAuthUser;
  doc["basic-auth"]["pass"] = bAuthPW;
  doc["auto-update"] = bAutoUpdate;
  doc["pre40"] = bPre40;
  doc["act-json-mqtt"] = bActJsonMQTT;
  doc["raw-port"] = bRawPort;
  doc["led-prt"] = bLED_PRT;
  writeToJsonFile(doc, SettingsFile);
  
} // writeSettings()


//=======================================================================
void readSettings(bool show) 
{
  StaticJsonDocument<1024> doc; 
  File SettingsFile;
  if (!FSmounted) return;

  DebugTf(" %s ..\r\n", SETTINGS_FILE);
 
   if (!LittleFS.exists(SETTINGS_FILE)) 
  {
    DebugTln(F(" .. DSMRsettings.json file not found! --> created file!"));
    writeSettings();
    return;
  }

  for (int T = 0; T < 2; T++) 
  {
    SettingsFile = LittleFS.open(SETTINGS_FILE, "r");
    if (!SettingsFile) 
    {
      if (T == 0) DebugTf(" .. something went wrong opening [%s]\r\n", SETTINGS_FILE);
      else        DebugT(T);
      delay(500);
    }
  } // try T times ..
  
  DebugT(F("Reading settings:.."));

  DeserializationError error = deserializeJson(doc, SettingsFile);
  if (error) {
    Debugln();
//    DebugTln(F("read():Failed to read DSMRsettings.json file"));
    LogFile("read():Failed to read DSMRsettings.json file",true);
    SettingsFile.close();
    writeSettings();
    return;
  }
  
  //strcpy(LittleFSTimestamp, doc["Timestamp"]);
  strcpy(settingHostname, doc["Hostname"] | _DEFAULT_HOSTNAME );
  strcpy(settingIndexPage, doc["IndexPage"] | _DEFAULT_HOMEPAGE);
  settingEDT1 = doc["EnergyDeliveredT1"];
  settingEDT2 = doc["EnergyDeliveredT2"];
  settingERT1 = doc["EnergyReturnedT1"];
  settingERT2 = doc["EnergyReturnedT2"];
  settingGDT = doc["GASDeliveredT"];
  settingENBK = doc["EnergyVasteKosten"];
  settingGNBK = doc["GasVasteKosten"];
  settingSmHasFaseInfo = doc["SmHasFaseInfo"];
  
//  settingTelegramInterval = doc["TelegramInterval"];
//  CHANGE_INTERVAL_SEC(nextTelegram, settingTelegramInterval);
// 
  //sprintf(settingMQTTbroker, "%s:%d", MQTTbroker, MQTTbrokerPort);
  strcpy(settingMQTTbroker, doc["MQTTbroker"]);
  settingMQTTbrokerPort = doc["MQTTbrokerPort"];
  strcpy(settingMQTTuser, doc["MQTTUser"]);
  strcpy(settingMQTTpasswd, doc["MQTTpasswd"]);
  settingMQTTinterval = doc["MQTTinterval"];
  strcpy(settingMQTTtopTopic, doc["MQTTtopTopic"]);
  if (settingMQTTtopTopic[strlen(settingMQTTtopTopic)-1] != '/') strcat(settingMQTTtopTopic,"/");
  
  CHANGE_INTERVAL_SEC(publishMQTTtimer, settingMQTTinterval);
  CHANGE_INTERVAL_MIN(reconnectMQTTtimer, 1);
  LEDenabled = doc["LED"];
  if (doc.containsKey("ota")) strcpy(BaseOTAurl, doc["ota"]);
  if (doc.containsKey("enableHistory")) EnableHistory = doc["enableHistory"];

  if (doc.containsKey("watermeter")) WtrMtr = doc["watermeter"];
  if (doc.containsKey("waterfactor")) WtrFactor = doc["waterfactor"];

  if (doc.containsKey("HAdiscovery")) EnableHAdiscovery = doc["HAdiscovery"];
  if (doc.containsKey("auto-update")) bAutoUpdate = doc["auto-update"];
  if (doc.containsKey("pre40")) bPre40 = doc["pre40"];
  if (doc.containsKey("raw-port")) bRawPort = doc["raw-port"];
  if (doc.containsKey("led-prt")) bLED_PRT = doc["led-prt"];
  
  if (doc.containsKey("act-json-mqtt")) bActJsonMQTT = doc["act-json-mqtt"];

  const char* temp = doc["basic-auth"]["user"];
  if (temp) strcpy(bAuthUser, temp);
  
  temp = doc["basic-auth"]["pass"];
  if (temp) strcpy(bAuthPW, temp);

  
  SettingsFile.close();
  //end json

  //--- this will take some time to settle in
  //--- probably need a reboot before that to happen :-(
//  MDNS.setHostname(settingHostname);    // start advertising with new(?) settingHostname
  //set hostname
    mdns_hostname_set(settingHostname);
    //set default instance
    mdns_instance_name_set(_DEFAULT_HOSTNAME);
if ( P1Status.dev_type == PRO_BRIDGE ) digitalWrite(PRT_LED, bLED_PRT);

  Debug(F(".. done\r"));


  if (strlen(settingIndexPage) < 7) strCopy(settingIndexPage, (sizeof(settingIndexPage) -1), "DSMRindexEDGE.html");
  
//  byte min_value = bPre40?MIN_T_INTV_PRE40:MIN_TELEGR_INTV;
//  if (settingTelegramInterval  < min_value) settingTelegramInterval = min_value;
  if (settingMQTTbrokerPort    < 1) settingMQTTbrokerPort   = 1883;

  if (!show) return;
  
  Debugln(F("\r\n==== Settings ===================================================\r"));
  Debugf("                    Hostname : %s\r\n",     settingHostname);
  Debugf("   Energy Delivered Tarief 1 : %9.7f\r\n",  settingEDT1);
  Debugf("   Energy Delivered Tarief 2 : %9.7f\r\n",  settingEDT2);
  Debugf("   Energy Delivered Tarief 1 : %9.7f\r\n",  settingERT1);
  Debugf("   Energy Delivered Tarief 2 : %9.7f\r\n",  settingERT2);
  Debugf("        Gas Delivered Tarief : %9.7f\r\n",  settingGDT);
  Debugf("     Energy Netbeheer Kosten : %9.2f\r\n",  settingENBK);
  Debugf("        Gas Netbeheer Kosten : %9.2f\r\n",  settingGNBK);
  Debugf("  SM Fase Info (0=No, 1=Yes) : %d\r\n",     settingSmHasFaseInfo);
//  Debugf("   Telegram Process Interval : %d\r\n",     settingTelegramInterval);
  
  Debugf("                  Index Page : %s\r\n",     settingIndexPage);

  Debugln(F("\r\n==== MQTT settings ==============================================\r"));
  Debugf("          MQTT broker URL/IP : %s:%d", settingMQTTbroker, settingMQTTbrokerPort);
  if (MQTTclient.connected()) Debugln(F(" (is Connected!)\r"));
  else                 Debugln(F(" (NOT Connected!)\r"));
  Debugf("                   MQTT user : %s\r\n", settingMQTTuser);
#ifdef SHOW_PASSWRDS
  Debugf("               MQTT password : %s\r\n", settingMQTTpasswd);
#else
  Debug( "               MQTT password : *************\r\n");
#endif
  Debugf("          MQTT send Interval : %d\r\n", settingMQTTinterval);
  Debugf("              MQTT top Topic : %s\r\n", settingMQTTtopTopic);
  Debugln(F("\r\n==== Other settings =============================================\r"));
  Debug(F("                 LED enabled : ")); Debugln(LEDenabled);
  Debug(F("                Base OTA url : ")); Debugln(BaseOTAurl);
  Debug(F("              History Enabled: ")); Debugln(EnableHistory);
  Debug(F("          Water Meter Enabled: ")); Debugln(WtrMtr);
  Debug(F("    HA Auto Discovery Enabled: ")); Debugln(EnableHAdiscovery);
  Debug(F("   Support 2.x and 3.x meters: ")); Debugln(bPre40);
  Debug(F("      Raw Telegram on port 82: ")); Debugln(bRawPort);
  Debug(F("  Enables actual json in mqtt: ")); Debugln(bActJsonMQTT);
    
  Debugln(F("-\r"));

} // readSettings()


//=======================================================================
void updateSetting(const char *field, const char *newValue)
{
  DebugTf("-> field[%s], newValue[%s]\r\n", field, newValue);

  if (!FSmounted) return;

  if (!stricmp(field, "Hostname")) {
    strCopy(settingHostname, 29, newValue); 
    if (strlen(settingHostname) < 1) strCopy(settingHostname, 29, _DEFAULT_HOSTNAME); 
    char *dotPntr = strchr(settingHostname, '.') ;
    if (dotPntr != NULL)
    {
      byte dotPos = (dotPntr-settingHostname);
      if (dotPos > 0)  settingHostname[dotPos] = '\0';
    }
    Debugln();
    DebugTf("Need reboot before new %s.local will be available!\r\n\n", settingHostname);
  }
  if (!stricmp(field, "ed_tariff1"))        settingEDT1         = String(newValue).toFloat();  
  if (!stricmp(field, "ed_tariff2"))        settingEDT2         = String(newValue).toFloat();  
  if (!stricmp(field, "er_tariff1"))        settingERT1         = String(newValue).toFloat();  
  if (!stricmp(field, "er_tariff2"))        settingERT2         = String(newValue).toFloat();  
  if (!stricmp(field, "electr_netw_costs")) settingENBK         = String(newValue).toFloat();

  if (!stricmp(field, "gd_tariff"))         settingGDT          = String(newValue).toFloat();  
  if (!stricmp(field, "gas_netw_costs"))    settingGNBK         = String(newValue).toFloat();
  if (!stricmp(field, "water_m3")){
    P1Status.wtr_m3         = String(newValue).toInt();
    CHANGE_INTERVAL_MS(StatusTimer, 100);
  }
  if (!stricmp(field, "water_l")) {
    P1Status.wtr_l         = String(newValue).toInt();
    CHANGE_INTERVAL_MS(StatusTimer, 100);
  }

  if (!stricmp(field, "sm_has_fase_info")) 
  {
    settingSmHasFaseInfo = String(newValue).toInt(); 
    if (settingSmHasFaseInfo != 0)  settingSmHasFaseInfo = 1;
    else                            settingSmHasFaseInfo = 0;  
  }

//  if (!stricmp(field, "tlgrm_interval"))    
//  {
//    settingTelegramInterval     = String(newValue).toInt();  
//    CHANGE_INTERVAL_SEC(nextTelegram, settingTelegramInterval)
//  }

  if (!stricmp(field, "IndexPage"))        strCopy(settingIndexPage, (sizeof(settingIndexPage) -1), newValue);  

  if (!stricmp(field, "mqtt_broker"))  {
    DebugT("settingMQTTbroker! to : ");
    memset(settingMQTTbroker, '\0', sizeof(settingMQTTbroker));
    strCopy(settingMQTTbroker, 100, newValue);
    Debugf("[%s]\r\n", settingMQTTbroker);
    mqttIsConnected = false;
    CHANGE_INTERVAL_MS(reconnectMQTTtimer, 100); // try reconnecting cyclus timer
    MQTTclient.disconnect();
  }
  if (!stricmp(field, "mqtt_broker_port")) {
    settingMQTTbrokerPort = String(newValue).toInt();  
    mqttIsConnected = false;
    CHANGE_INTERVAL_MS(reconnectMQTTtimer, 100); // try reconnecting cyclus timer
  }
  if (!stricmp(field, "mqtt_user")) {
    strCopy(settingMQTTuser    ,35, newValue);  
    mqttIsConnected = false;
    CHANGE_INTERVAL_MS(reconnectMQTTtimer, 100); // try reconnecting cyclus timer
  }
  if (!stricmp(field, "mqtt_passwd")) {
    strCopy(settingMQTTpasswd  ,25, newValue);  
    mqttIsConnected = false;
    CHANGE_INTERVAL_MS(reconnectMQTTtimer, 100); // try reconnecting cyclus timer
  }
  if (!stricmp(field, "mqtt_interval")) {
    settingMQTTinterval   = String(newValue).toInt();  
    CHANGE_INTERVAL_SEC(publishMQTTtimer, settingMQTTinterval);
  }
  if (!stricmp(field, "mqtt_toptopic"))     strCopy(settingMQTTtopTopic, 20, newValue);  
  
  if (!stricmp(field, "b_auth_user")) strCopy(bAuthUser,25, newValue);  
  if (!stricmp(field, "b_auth_pw")) strCopy(bAuthPW,25, newValue); 

  if (!stricmp(field, "water_fact")) WtrFactor = String(newValue).toFloat(); 
  
  if (!stricmp(field, "ota_url")) {
    char ota_url[sizeof(BaseOTAurl)] = "http://";
    strcat(ota_url, newValue);
    strCopy(BaseOTAurl,sizeof(ota_url), ota_url ); 
  }
  
  //booleans
  if (!stricmp(field, "led")) LEDenabled = (stricmp(newValue, "true") == 0?true:false); 
  if (!stricmp(field, "hist")) EnableHistory = (stricmp(newValue, "true") == 0?true:false); 
  if (!stricmp(field, "water_enabl")) WtrMtr = (stricmp(newValue, "true") == 0?true:false);  
  if (!stricmp(field, "ha_disc_enabl")) EnableHAdiscovery = (stricmp(newValue, "true") == 0?true:false);  
  if ( P1Status.dev_type == PRO_BRIDGE ) {
    if (!stricmp(field, "led-prt")) {
      bLED_PRT = (stricmp(newValue, "true") == 0?true:false);  
      digitalWrite(PRT_LED, bLED_PRT);
    }
   }
  if (!stricmp(field, "pre40")) {
    bPre40 = (stricmp(newValue, "true") == 0?true:false);    
    SetupSMRport();
  }
  if (!stricmp(field, "raw-port")) bRawPort = (stricmp(newValue, "true") == 0?true:false);  
  if (!stricmp(field, "act-json-mqtt")) bActJsonMQTT = (stricmp(newValue, "true") == 0?true:false);  
  
  writeSettings();
  
} // updateSetting()

//=======================================================================
void LogFile(const char* payload, bool toDebug = false) {
  if (toDebug) DebugTln(payload);
  if (!FSmounted) return;
  File LogFile = LittleFS.open("/P1.log", "a"); // open for appending  
  if (!LogFile) {
    DebugTln(F("open P1.log FAILED!!!--> Bailout\r\n"));
    return;
  }
  //log rotate
  if (LogFile.size() > 8000){ 
//    DebugT(F("LogFile filesize: "));Debugln(RebootFile.size());
    LittleFS.remove("/P1_log.old");     //remove .old if existing 
    //rename file
    DebugTln(F("RebootLog: rename file"));
    LogFile.close(); 
    LittleFS.rename("/P1.log", "/P1_log.old");
    LogFile = LittleFS.open("/P1.log", "a"); // open for appending  
    }
    
    if (strlen(payload)==0) {
      //reboot
      //make one record : {"time":"2020-09-23 17:03:25","reason":"Software/System restart","reboots":42}
      LogFile.println("{\"time\":\"" + buildDateTimeString(actTimestamp, sizeof(actTimestamp)) + "\",\"reboot\":\"" + lastReset + "\",\"reboots\":" +  (int)P1Status.reboots + "}");
    } else { 
      //make one record : {"time":"2020-09-23 17:03:25","log":"Software/System restart"}
      LogFile.println("{\"time\":\"" + buildDateTimeString(actTimestamp, sizeof(actTimestamp)) + "\",\"log\":\"" + payload + "\"}");
    }
    //closing the file
    LogFile.close(); 
}

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
