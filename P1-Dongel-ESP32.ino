/*
***************************************************************************  
**  Program  : P1-Dongel-ESP32
**  Copyright (c) 2021 Martijn Hendriks / based on DSMR Api Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*      
*      TODO
*      - lees/schrijffouten ringfiles terugmelden in frontend
*      - update via site ipv url incl logica voor uitvragen hiervan
*      - keuze om voor lokale frontend assets of uit het cdn
*      - frontend options in json (max stroom per fase, uitzetten van spanningsoverzicht in dash, etc)
*      - Telegram notificaties (waarschijnlijk pas in de ESP32 implementatie ivm memory ssl certificaten)
*      -- message bij drempelwaardes 
*      -- verbruiksrapport einde dag/week/maand
*      - monitor proces en fail over indien het niet goed gaat (cpu 1 <-> cpu 0)
*      - AsynWebserver implementatie
*      - Multi core tasks (xSemaphore)
*      -- smr  -> sync naar mqtt, file write, processing
*      -- frontend & api server
*      -- mqtt
*      √ uptime op basis van esp_log_timestamp()/1000
*      √ debug mode is niet meer nodig ... geen swap meer ;-)
*      √ reboot after 4 min AP mode
*      √ NL en BE SMR in 1 
*      √ reconnectMQTTtimer
*      √ reconnectWiFi
*      √ bugfix: niet aanmaken van nieuwe ring file bij file not found
*      - update proces loopt niet goed met gz files
*      - bug telegram RAW serial
*      √ mdns.queryhostname implementatie
*      esp_register_shutdown_handler()
*      
  
  Arduino-IDE settings for P1 Dongle hardware ESP32:
    - Board: "ESP32 Dev Module"
    - Flash mode: "QIO"
    - Flash size: "4MB (32Mb)"
    - CorenDebug Level: "None"
    - Flash Frequency: "80MHz"
    - CPU Frequency: "240MHz"
    - Upload Speed: "961600"                                                                                                                                                                                                                                                 
    - Port: <select correct port>
*/
/******************** compiler options  ********************************************/
#define USE_MQTT                      // define if you want to use MQTT (configure through webinterface)
#define USE_UPDATE_SERVER             // define if there is enough memory and updateServer to be used
//  #define USE_NTP_TIME              // define to generate Timestamp from NTP (Only Winter Time for now)
//  #define HAS_NO_SLIMMEMETER        // define for testing only!
//  #define USE_SYSLOGGER             // define if you want to use the sysLog library for debugging
//  #define SHOW_PASSWRDS             // well .. show the PSK key and MQTT password, what else?

#define ALL_OPTIONS "[MQTT][UPDATE_SERVER][LITTLEFS]"

/******************** don't change anything below this comment **********************/
#include "DSMRloggerAPI.h"

TaskHandle_t CPU0;

#ifdef USE_SYSLOGGER
//===========================================================================================
void openSysLog(bool empty)
{
  if (sysLog.begin(500, 100, empty))  // 500 lines use existing sysLog file
  {   
    DebugTln(F("Succes opening sysLog!"));
  }
  else DebugTln(F("Error opening sysLog!"));
 
  sysLog.setDebugLvl(1);
  sysLog.setOutput(&TelnetStream);
  sysLog.status();
  sysLog.write("\r\n");
  for (uint8_t q=0;q<3;q++)
  {
    sysLog.write("******************************************************************************************************");
  }
  writeToSysLog(F("Last Reset Reason [%s]"), getResetReason());
  writeToSysLog("actTimestamp[%s], nrReboots[%u], Errors[%u]", actTimestamp , nrReboots , slotErrors);
  sysLog.write(" ");

} // openSysLog()
#endif

//===========================================================================================
void setup() 
{
  Serial.begin(115200, SERIAL_8N1); //debug stream
  Serial1.begin(115200, SERIAL_8N1, 16,0,true); //p1 serial input

  Debug("\n\n ----> BOOTING....[" _VERSION "] <-----\n\n");
  DebugTln("The board name is: " ARDUINO_BOARD);

  lastReset = getResetReason();
  DebugT(F("Last reset reason: ")); Debugln(lastReset);
  delay(1500);
  
//================ File System ===========================================
  if (LITTLEFS.begin()) 
  {
    DebugTln(F("File System Mount succesfull\r"));
    FSmounted = true;     
  } else DebugTln(F("!!!! File System Mount failed\r"));   // Serious problem with File System 
    
//------ read status file for last Timestamp --------------------
  
  //==========================================================//
  // writeLastStatus();  // only for firsttime initialization //
  //==========================================================//
  readLastStatus(); // place it in actTimestamp
  // set the time to actTimestamp!
  actT = epoch(actTimestamp, strlen(actTimestamp), true);
  LogFile(); // write reboot status to file
  nrReboots++;
  DebugTf("===> actTimestamp[%s]-> nrReboots[%u] - Errors[%u]\n", actTimestamp, nrReboots, slotErrors);                                                                    
  readSettings(true);
  writeLastStatus(); //update reboots
  
//=============start Networkstuff==================================
  delay(1500);
  startWiFi(settingHostname, 240);  // timeout 4 minuten

  Debug (F("\nConnected to " )); Debugln (WiFi.SSID());
  Debug (F("IP address: " ));  Debugln (WiFi.localIP());
  Debug (F("IP gateway: " ));  Debugln (WiFi.gatewayIP());
  Debugln();

  delay(500);
  startTelnet();

//-----------------------------------------------------------------
#ifdef USE_SYSLOGGER
  openSysLog(false);
  snprintf(cMsg, sizeof(cMsg), "SSID:[%s],  IP:[%s], Gateway:[%s]", WiFi.SSID().c_str()
                                                                  , WiFi.localIP().toString().c_str()
                                                                  , WiFi.gatewayIP().toString().c_str());
  writeToSysLog("%s", cMsg);
#endif
  startMDNS(settingHostname);
 
//=============end Networkstuff======================================

#if defined(USE_NTP_TIME)                                   //USE_NTP
//================ startNTP =========================================
                                                        //USE_NTP
                                                            //USE_NTP
  if (!startNTP())                                          //USE_NTP
  {                                                         //USE_NTP
    DebugTln(F("ERROR!!! No NTP server reached!\r\n\r"));   //USE_NTP
                                                     //USE_NTP
    delay(1500);                                            //USE_NTP
    ESP.restart();                                          //USE_NTP
    delay(2000);                                            //USE_NTP
  }                                                         //USE_NTP
                                                    //USE_NTP
  prevNtpHour = hour();                                     //USE_NTP
                                                            //USE_NTP
#endif  //USE_NTP_TIME                                      //USE_NTP
//================ end NTP =========================================

//test if File System is correct populated!
  if (!DSMRfileExist(settingIndexPage, false) )    FSNotPopulated = true;   
#ifdef USE_SYSLOGGER
  if (FSNotPopulated)
  {
    sysLog.write(F("FS is not correct populated (files are missing)"));
  }
#endif
  
#if defined(USE_NTP_TIME)                                                           //USE_NTP
  time_t t = now(); // store the current time in time variable t                    //USE_NTP
  snprintf(cMsg, sizeof(cMsg), "%02d%02d%02d%02d%02d%02dW\0\0", (year(t) - 2000), month(t), day(t), hour(t), minute(t), second(t)); 
  DebugTf("Time is set to [%s] from NTP\r\n", cMsg);                                //USE_NTP
#endif  // use_dsmr_30


//================ Start MQTT  ======================================

#ifdef USE_MQTT                                                 //USE_MQTT
  if ( (strlen(settingMQTTbroker) > 3) && (settingMQTTinterval != 0)) connectMQTT();
#endif                                                          //USE_MQTT

//================ End of Start MQTT  ===============================
//================ Start HTTP Server ================================

  if (!FSNotPopulated) {
    DebugTln(F("FS correct populated -> normal operation!\r"));
    httpServer.serveStatic("/",                 LITTLEFS, settingIndexPage);
    httpServer.serveStatic("/DSMRindex.html",   LITTLEFS, settingIndexPage);
    httpServer.serveStatic("/DSMRindexEDGE.html",LITTLEFS, settingIndexPage);
    httpServer.serveStatic("/index",            LITTLEFS, settingIndexPage);
    httpServer.serveStatic("/index.html",       LITTLEFS, settingIndexPage);
  } else {
    DebugTln(F("Oeps! not all files found on FS -> present FSexplorer!\r"));
    FSNotPopulated = true;
  }
    
  setupFSexplorer();
  delay(500);
  
//================ END HTTP Server ================================

  DebugTf("Startup complete! actTimestamp[%s]\r\n", actTimestamp);  
  writeToSysLog("Startup complete! actTimestamp[%s]", actTimestamp);  

//================ Start Slimme Meter ===============================

#if !defined( HAS_NO_SLIMMEMETER) 
  DebugTln(F("Enable slimmeMeter..\n"));
  slimmeMeter.enable(true);
#endif //test or normal mode

//================ End of Slimmer Meter ============================

  //create a task that will be executed in the Task1code() function, with priority 1 and executed on core 0
  xTaskCreatePinnedToCore(
                    CPU0Loop,   /* Task function. */
                    "CPU0",     /* name of task. */
                    20000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &CPU0,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 0 */                  
  delay(500); 

  esp_register_shutdown_handler(ShutDownHandler);
 
} // setup()


//CPU0Loop
void CPU0Loop( void * pvParameters ){
  while(true) {
    //--- verwerk inkomende data
    slimmeMeter.loop();

    //--- start volgend telegram
    if DUE(nextTelegram) {
       if (Verbose1) DebugTln(F("Next Telegram"));
       slimmeMeter.enable(true); 
    }
    delay(50); // max 20x per seconde 
  } 
}

//===[ no-blocking delay with running background tasks in ms ]============================
DECLARE_TIMER_MS(timer_delay_ms, 1);
void delayms(unsigned long delay_ms)
{
  CHANGE_INTERVAL_MS(timer_delay_ms, delay_ms);
  RESTART_TIMER(timer_delay_ms);
  while (!DUE(timer_delay_ms)) doSystemTasks();
    
} // delayms()

//===[ Do System tasks ]=============================================================
void doSystemTasks()
{
  #ifndef HAS_NO_SLIMMEMETER
    handleSlimmemeter();
  #endif
  #ifdef USE_MQTT
    MQTTclient.loop();
  #endif
  httpServer.handleClient();
//  MDNS.update();
  handleKeyInput();
  yield();

} // doSystemTasks()

uint32_t LoopCount=0;
unsigned int heap_before;
ParseResult<void> res;
 char rawbe1[] =
    "/FLU5\\253770234_A\r\n"
  "\r\n"
  "0-0:96.1.4(50213)\r\n"
  "0-0:96.1.1(3153414731313030303236393436)\r\n"
  "0-0:1.0.0(190919165430S)\r\n"
  "1-0:1.8.1(000031.250*kWh)\r\n"
  "1-0:1.8.2(000040.792*kWh)\r\n"
  "1-0:2.8.1(000061.957*kWh)\r\n"
  "1-0:2.8.2(000023.548*kWh)\r\n"
  "0-0:96.14.0(0001)\r\n"
  "1-0:1.7.0(00.342*kW)\r\n"
  "1-0:2.7.0(00.000*kW)\r\n"
  "1-0:32.7.0(236.7*V)\r\n"
  "1-0:31.7.0(002*A)\r\n"
  "0-0:96.3.10(1)\r\n"
  "0-0:17.0.0(999.9*kW)\r\n"
  "1-0:31.4.0(999*A)\r\n"
  "0-0:96.13.0()\r\n"
  "0-1:24.1.0(003)\r\n"
  "0-1:96.1.1(37464C4F32313139303035343533)\r\n"
  "0-1:24.4.0(1)\r\n"
  "0-1:24.2.3(190919165007S)(00001.315*m3)\r\n"
  "!BFE7\r\n";

 char rawbe2[] =
    "/FLU5\\253770234_A\r\n"
  "\r\n"
  "0-0:96.1.4(50213)\r\n"
  "0-0:96.1.1(3153414731313030303236393436)\r\n"
  "0-0:1.0.0(190919165430S)\r\n"
  "1-0:1.8.1(000031.250*kWh)\r\n"
  "1-0:1.8.2(000040.792*kWh)\r\n"
  "1-0:2.8.1(000061.957*kWh)\r\n"
  "1-0:2.8.2(000023.548*kWh)\r\n"
  "0-0:96.14.0(0001)\r\n"
  "1-0:1.7.0(00.342*kW)\r\n"
  "1-0:2.7.0(00.000*kW)\r\n"
  "1-0:32.7.0(236.7*V)\r\n"
  "1-0:31.7.0(002*A)\r\n"
  "0-0:96.3.10(1)\r\n"
  "0-0:17.0.0(999.9*kW)\r\n"
  "1-0:31.4.0(999*A)\r\n"
  "0-0:96.13.99()\r\n"
  "0-1:24.1.0(003)\r\n"
  "0-1:96.1.1(37464C4F32313139303035343533)\r\n"
  "0-1:24.4.0(1)\r\n"
  "0-1:24.2.3(190919165007S)(00001.315*m3)\r\n"
  "!1b46\r\n";
  

struct buildJsonMQTT2 {
/* twee types
 *  {energy_delivered_tariff1":[{"value":11741.29,"unit":"kWh"}]}"
 *  {equipment_id":[{"value":"4530303435303033383833343439383137"}]}"
 *  
 *  msg = "{\""+Name+"\":[{\"value\":"+value_to_json(i.val())+",\"unit\":"+Unit+"}]}";
 *  msg = "\"{\""+Name+"\":[{\"value\":"+value_to_json(i.val())+"}]}\""
 *  
 */
// StaticJsonDocument<400> doc2;  
  String msg;//--
//  char   mqttBuff[100]; // todo kan issue geven bij lange berichten

    template<typename Item>
    void apply(Item &i) {
      if (i.present()) 
      {
        String Name = String(Item::name);
        String Unit = Item::unit();
        strcpy(cMsg,settingMQTTtopTopic);
        strConcat(cMsg, sizeof(cMsg), Name.c_str());
        if (Verbose2) DebugTf("topicId[%s]\r\n", cMsg);
//        doc2.clear();
//        JsonObject nested = doc2[Name].createNestedObject();
        if (Unit.length() > 0) {
//          nested["value"] = value_to_json(i.val());
//          nested["unit"] = Unit;
          msg = "{\""+Name+"\":[{\"value\":"+value_to_json(i.val())+",\"unit\":\""+Unit+"\"}]}";//--
        }
        else {
//          nested["value"] = value_to_json(i.val());
          msg = "{\""+Name+"\":[{\"value\":"+value_to_json(i.val())+"}]}";//--
        }
//        serializeJson(doc2, mqttBuff);
        DebugTln("mqtt zelf: " + msg);//--
        if (!MQTTclient.publish(cMsg, msg.c_str()) ) DebugTf("Error publish(%s) [%s] [%d bytes]\r\n", cMsg, msg.c_str(), (strlen(cMsg) + msg.length()));
//        DebugT("mqtt lib : ");Debugln(mqttBuff);//--
      } // if i.present
  } //apply
  
  template<typename Item>
  String value_to_json(Item& i) {
    return "\"" + String( i )  + "\"";
  }

  String value_to_json(TimestampedFixedValue i) {
    return String( i );
  }
  
  float value_to_json(FixedValue i) {
    return i;
  }
}; // buildJsonMQTT2

void loop() {
  LoopCount++; 
  heap_before = ESP.getFreeHeap();
 
  //sendMQTTData();
//  writeRingFiles(); 
//  writeLastStatus();
  DSMRdata = {};
  if (!(LoopCount % 25)) res = P1Parser::parse(&DSMRdata, rawbe2, lengthof(rawbe2), true);
  else res = P1Parser::parse(&DSMRdata, rawbe1, lengthof(rawbe1), true);
  if (res.err) {
    // Parsing error, show it
    Debugln(res.fullError(rawbe1, rawbe1 + lengthof(rawbe1)));
  } else 
  { //TelnetStream.println("PARSE SUCCESFULL");
  
  delay(50);// some time for the esp to clean up
  Debugf("%4d | %d | %d | %d\n",LoopCount,heap_before,ESP.getFreeHeap(),heap_before-ESP.getFreeHeap());
  DSMRdata.applyEach(buildJsonMQTT2());
  }
  delay(30); //other wise it will be to heavy for the mqtt server
  
}

void loop2 () 
{  
  //--- do the tasks that has to be done as often as possible
  doSystemTasks();

  //--- update files
  if (DUE(antiWearTimer))
  {
    writeRingFiles();
    writeLastStatus();
  }

  //--- if connection lost, try to reconnect to WiFi
  if ( DUE(reconnectWiFi) && (WiFi.status() != WL_CONNECTED) )
  {
    writeToSysLog("Restart wifi with [%s]...", settingHostname);
    LogFile("Wifi connection lost");  
    startWiFi(settingHostname, 10);
    if (WiFi.status() != WL_CONNECTED){
          writeToSysLog("%s", "Wifi still not connected!");
          LogFile("Wifi connection still lost");
    } else {
          snprintf(cMsg, sizeof(cMsg), "IP:[%s], Gateway:[%s]", WiFi.localIP().toString().c_str(), WiFi.gatewayIP().toString().c_str());
          writeToSysLog("%s", cMsg);
    }
  }

//--- if NTP set, see if it needs synchronizing
#if defined(USE_NTP_TIME)                                           //USE_NTP
  if DUE(synchrNTP)                                                 //USE_NTP
  {
    if (timeStatus() == timeNeedsSync || prevNtpHour != hour())     //USE_NTP
    {
      prevNtpHour = hour();                                         //USE_NTP
      setSyncProvider(getNtpTime);                                  //USE_NTP
      setSyncInterval(600);                                         //USE_NTP
    }
  }
#endif                                                              //USE_NTP

  yield();
  
} // loop()


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
