/*
***************************************************************************  
**  Program  : P1-Dongel-ESP32
**  Copyright (c) 2022 Martijn Hendriks / based on DSMR Api Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*      
TODO
- lees/schrijffouten ringfiles terugmelden in frontend
- keuze om voor lokale frontend assets of uit het cdn
- frontend options in json (max stroom per fase, uitzetten van spanningsoverzicht in dash, etc)
- mqtt broker benaderen via host name http://www.iotsharing.com/2017/06/how-to-get-ip-address-from-mdns-host-name-in-arduino-esp32.html 
-- message bij drempelwaardes 
-- verbruiksrapport einde dag/week/maand
- monitor proces en fail over indien het niet goed gaat (cpu 1 <-> cpu 0)
- AsynWebserver implementatie
- bug telegram RAW serial
- watersensor historie / ringfiles
- watersensor only mode
X ticker blynk
- Telegram komt niet altijd door

FIXES

************************************************************************************
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
#define USE_WATER_SENSOR              // define if there is enough memory and updateServer to be used
//#define USE_NTP_TIME              // define to generate Timestamp from NTP (Only Winter Time for now)
//#define HAS_NO_SLIMMEMETER        // define for testing only!
//#define SHOW_PASSWRDS             // well .. show the PSK key and MQTT password, what else?
#define HA_DISCOVER

#ifdef USE_WATER_SENSOR
  #define ALL_OPTIONS "[MQTT][LITTLEFS][WATER][HA_DISCOVER]"
#else
  #define ALL_OPTIONS "[MQTT][LITTLEFS][HA_DISCOVER]"
#endif

/******************** don't change anything below this comment **********************/
#include "DSMRloggerAPI.h"

//===========================================================================================
void setup() 
{
  Serial.begin(115200, SERIAL_8N1); //debug stream
  P1Serial.begin(115200, SERIAL_8N1, 16,0,true); //p1 serial input
  pinMode(DTR_IO, OUTPUT);
  pinMode(LED, OUTPUT);
  // sign of life
  digitalWrite(LED, LOW); //ON
  delay(1200);
  ToggleLED();

  lastReset = getResetReason();
  Debug("\n\n ----> BOOTING....[" _VERSION "] <-----\n\n");
  DebugTln("The board name is: " ARDUINO_BOARD);
  DebugT(F("Last reset reason: ")); Debugln(lastReset);
  
//================ File System ===========================================
  if (LittleFS.begin()) 
  {
    DebugTln(F("File System Mount succesfull\r"));
    FSmounted = true;     
  } else DebugTln(F("!!!! File System Mount failed\r"));   // Serious problem with File System 
  
//==========================================================//
// writeLastStatus();  // only for firsttime initialization //
//==========================================================//
  P1StatusBegin(); //leest laatste opgeslagen status & rebootcounter + 1
  actT = epoch(actTimestamp, strlen(actTimestamp), true); // set the time to actTimestamp!
  P1StatusWrite();
  LogFile("",false); // write reboot status to file
  readSettings(true);
  
//=============start Networkstuff==================================
  startWiFi(settingHostname, 240);  // timeout 4 minuten
  startTelnet();
  startMDNS(settingHostname);
 
//================ startNTP =========================================
#ifdef USE_NTP_TIME 
  if (!startNTP()) {                                            
    DebugTln(F("ERROR!!! No NTP server reached!\r\n\r"));   //USE_NTP
    P1Reboot();                                             //USE_NTP
  }                                                         //USE_NTP
  prevNtpHour = hour();                                     //USE_NTP
  time_t t = now(); // store the current time in time variable t                    //USE_NTP
  snprintf(cMsg, sizeof(cMsg), "%02d%02d%02d%02d%02d%02dW\0\0", (year(t) - 2000), month(t), day(t), hour(t), minute(t), second(t)); 
  DebugTf("Time is set to [%s] from NTP\r\n", cMsg);                                //USE_NTP
#endif

//================ Start MQTT  ======================================

if ( (strlen(settingMQTTbroker) > 3) && (settingMQTTinterval != 0) ) connectMQTT();

//================ Start HTTP Server ================================

  //test if index page is available
  if (!DSMRfileExist(settingIndexPage, false) ) {
    DebugTln(F("Oeps! not all files found on FS -> present FSexplorer!\r"));
    FSNotPopulated = true;
  } else {
    DebugTln(F("FS correct populated -> normal operation!\r"));
    httpServer.serveStatic("/", LittleFS, settingIndexPage);
  } 

  setupFSexplorer();
 
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
    delay(20);
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
  handleKeyInput();
  yield();

} // doSystemTasks()

void loop () 
{  
  //--- do the tasks that has to be done as often as possible
  doSystemTasks();

  //--- update statusfile + ringfiles
  if ( DUE(antiWearRing) || RingCylce ) writeRingFiles(); //eens per 25min + elk uur overgang

  if (DUE(StatusTimer)) { //eens per 10min of indien extra m3
    P1StatusWrite();
    MQTTSentStaticInfo();
    #ifdef USE_WATER_SENSOR  
      sendMQTTWater();
    #endif
    CHANGE_INTERVAL_MIN(StatusTimer, 10);
  }

  if (UpdateRequested) RemoteUpdate(UpdateVersion,bUpdateSketch);

#ifdef USE_WATER_SENSOR    
  if ( WtrTimeBetween )  {
    DebugTf("Wtr delta readings: %d | debounces: %d | waterstand: %i.%i\n",WtrTimeBetween,debounces, P1Status.wtr_m3, P1Status.wtr_l);
    WtrTimeBetween = 0;
  }
#endif

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
