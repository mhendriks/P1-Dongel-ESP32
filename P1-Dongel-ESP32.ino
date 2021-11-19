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
*      - keuze om voor lokale frontend assets of uit het cdn
*      - frontend options in json (max stroom per fase, uitzetten van spanningsoverzicht in dash, etc)
*      - mqtt broker benaderen via host name http://www.iotsharing.com/2017/06/how-to-get-ip-address-from-mdns-host-name-in-arduino-esp32.html 
*      -- message bij drempelwaardes 
*      -- verbruiksrapport einde dag/week/maand
*      - monitor proces en fail over indien het niet goed gaat (cpu 1 <-> cpu 0)
*      - AsynWebserver implementatie
*      - Multi core tasks (xSemaphore)
*      -- smr  -> sync naar mqtt, file write, processing
*      -- frontend & api server
*      -- mqtt
*      √ bugfix: niet aanmaken van nieuwe ring file bij file not found
*      - update proces loopt niet goed met gz files
*      - bug telegram RAW serial
*      √ mdns.queryhostname implementatie
*      √ water_sensor telnet/statusfile
*      √ watersensor mqtt
*      - watersensor json actuals
*      - watersensor historie / ringfiles
*      - watersensor only mode
*      √ ringfiles met watermtr gegevens
*      √ ringfiles verwijderd uit de default fileupload 
*      - check of ringfiles bestaan bij startup ... anders aanmaken.
  
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
//#define USE_WATER_SENSOR              // define if there is enough memory and updateServer to be used
//  #define USE_NTP_TIME              // define to generate Timestamp from NTP (Only Winter Time for now)
//  #define HAS_NO_SLIMMEMETER        // define for testing only!
//  #define SHOW_PASSWRDS             // well .. show the PSK key and MQTT password, what else?

#define ALL_OPTIONS "[MQTT][UPDATE_SERVER][LITTLEFS]"

/******************** don't change anything below this comment **********************/
#include "DSMRloggerAPI.h"

//===========================================================================================
void setup() 
{
  Serial.begin(115200, SERIAL_8N1); //debug stream
  P1Serial.begin(115200, SERIAL_8N1, 16,0,true); //p1 serial input

  Debug("\n\n ----> BOOTING....[" _VERSION "] <-----\n\n");
  DebugTln("The board name is: " ARDUINO_BOARD);

  lastReset = getResetReason();
  DebugT(F("Last reset reason: ")); Debugln(lastReset);
  delay(1500);
  
//================ File System ===========================================
  if (LittleFS.begin()) 
  {
    DebugTln(F("File System Mount succesfull\r"));
    FSmounted = true;     
  } else DebugTln(F("!!!! File System Mount failed\r"));   // Serious problem with File System 
    
//------ read status file for last Timestamp --------------------
  
  //==========================================================//
  // writeLastStatus();  // only for firsttime initialization //
  //==========================================================//
  P1StatusBegin(); //leest laatste opgeslagen status & rebootcounter + 1
  actT = epoch(actTimestamp, strlen(actTimestamp), true); // set the time to actTimestamp!
  LogFile("",false); // write reboot status to file
  readSettings(true);
  
//=============start Networkstuff==================================
  delay(1500);
  startWiFi(settingHostname, 240);  // timeout 4 minuten
  
  Debugln();
  Debug (F("Connected to " )); Debugln (WiFi.SSID());
  Debug (F("IP address: " ));  Debugln (WiFi.localIP());
  Debug (F("IP gateway: " ));  Debugln (WiFi.gatewayIP());
  Debugln();
  
  delay(500);
  startTelnet();

//-----------------------------------------------------------------
  startMDNS(settingHostname);
 
//=============end Networkstuff======================================

#ifdef USE_NTP_TIME
//================ startNTP =========================================
                                                            //USE_NTP
  if (!startNTP()) {                                            
    DebugTln(F("ERROR!!! No NTP server reached!\r\n\r"));   //USE_NTP
    P1Reboot();                                           //USE_NTP
  }                                                         //USE_NTP
                                                    //USE_NTP
  prevNtpHour = hour();                                     //USE_NTP
                                                            //USE_NTP
#endif  //USE_NTP_TIME                                      //USE_NTP
//================ end NTP =========================================

//test if File System is correct populated!
if (!DSMRfileExist(settingIndexPage, false) ) FSNotPopulated = true;   
  
#if defined(USE_NTP_TIME)                                                           //USE_NTP
  time_t t = now(); // store the current time in time variable t                    //USE_NTP
  snprintf(cMsg, sizeof(cMsg), "%02d%02d%02d%02d%02d%02dW\0\0", (year(t) - 2000), month(t), day(t), hour(t), minute(t), second(t)); 
  DebugTf("Time is set to [%s] from NTP\r\n", cMsg);                                //USE_NTP
#endif  // use_dsmr_30


//================ Start MQTT  ======================================

if ( (strlen(settingMQTTbroker) > 3) && (settingMQTTinterval != 0) ) connectMQTT();

//================ End of Start MQTT  ===============================
//================ Start HTTP Server ================================

  if (!FSNotPopulated) {
    DebugTln(F("FS correct populated -> normal operation!\r"));
    httpServer.serveStatic("/",                 LittleFS, settingIndexPage);
    httpServer.serveStatic("/DSMRindex.html",   LittleFS, settingIndexPage);
    httpServer.serveStatic("/DSMRindexEDGE.html",LittleFS, settingIndexPage);
    httpServer.serveStatic("/index",            LittleFS, settingIndexPage);
    httpServer.serveStatic("/index.html",       LittleFS, settingIndexPage);
  } else {
    DebugTln(F("Oeps! not all files found on FS -> present FSexplorer!\r"));
    FSNotPopulated = true;
  }
    
  setupFSexplorer();
  delay(500);
  
//================ END HTTP Server ================================

  DebugTf("Startup complete! actTimestamp[%s]\r\n", actTimestamp);  

//================ Start Slimme Meter ===============================

#ifndef HAS_NO_SLIMMEMETER
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

#ifdef USE_WATER_SENSOR  
  setupWater();
#endif

  ConvRing3_2_0();
  CheckRingExists();
 
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
  DebugTln(F("Delayms"));
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
  MQTTclient.loop();
  httpServer.handleClient();
//  MDNS.update();
  handleKeyInput();
  yield();

} // doSystemTasks()


void loop () 
{  
  //--- do the tasks that has to be done as often as possible
  doSystemTasks();

  //--- update statusfile + ringfiles
  if ( DUE(antiWearRing) || RingCylce ) writeRingFiles(); //eens per 25min + elk uur overgang

  if (DUE(StatusTimer)) { //eens per 15min of indien extra m3
    P1StatusWrite();
    MQTTSentStaticInfo();
//    CHANGE_INTERVAL_MIN(StatusTimer, 15);
  }

  if (UpdateRequested) RemoteUpdate(UpdateVersion,bUpdateSketch);
  
#ifdef USE_WATER_SENSOR
  if ( WtrMtr && DUE(WaterTimer) ) {
    P1StatusWrite();
    sendMQTTWater();
    CHANGE_INTERVAL_MIN(WaterTimer, 30);
  }
#endif

  //--- if connection lost, try to reconnect to WiFi
  if ( DUE(reconnectWiFi) && (WiFi.status() != WL_CONNECTED) ) {
    sprintf(cMsg,"Wifi connection lost | rssi: %d",WiFi.RSSI());
    LogFile(cMsg,false);  
    startWiFi(settingHostname, 10);
    if (WiFi.status() != WL_CONNECTED){
          LogFile("Wifi connection still lost",false);
    } else snprintf(cMsg, sizeof(cMsg), "IP:[%s], Gateway:[%s]", WiFi.localIP().toString().c_str(), WiFi.gatewayIP().toString().c_str());
  }

//--- if NTP set, see if it needs synchronizing
#ifdef USE_NTP_TIME                                                 //USE_NTP
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
