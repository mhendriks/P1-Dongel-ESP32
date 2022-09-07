/*
***************************************************************************  
**  Program  : P1-Dongel-ESP32
**  Copyright (c) 2022 Martijn Hendriks / based on DSMR Api Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*      
TODO
- keuze om voor lokale frontend assets of uit het cdn
- mqtt broker benaderen via host name http://www.iotsharing.com/2017/06/how-to-get-ip-address-from-mdns-host-name-in-arduino-esp32.html 
-- message bij drempelwaardes 
-- verbruiksrapport einde dag/week/maand
- AsynWebserver implementatie
- 24uur eens per minuut weergeven van gegevens
- C3: logging via usb poort mogelijk maken
- load ballancing feature (1000 samples)
- ESP32C3 channel 13 wifi issues uitzoeken
- feature: nieuwe meter = beginstand op 0
- Standalone S0 API
- front-end: splitsen dashboard / eenmalige instellingen bv fases 
- aanpassen telegram capture -> niet continu 
- pro: esphome samenvoegen p1 + watersensor 
- front-end: functie toevoegen die de beschikbare versies toont (incl release notes) en je dan de keuze geeft welke te flashen. (Erik)
- remote-update: redirect  na succesvolle update (Erik)
- verbruik - teruglevering lijn door maandgrafiek (Erik)
- domoticz auto discovery mqtt
- uitbreiden settingsscherm met niewue settings (bv authenticatie / historie)
- automatische update

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
//#define USE_NTP_TIME              // define to generate Timestamp from NTP (Only Winter Time for now)
//#define SHOW_PASSWRDS             // well .. show the PSK key and MQTT password, what else?
//#define USE_PROFILE_MIN      

#define ALL_OPTIONS "[CORE]"

/******************** don't change anything below this comment **********************/
#include "DSMRloggerAPI.h"

//===========================================================================================
void setup() 
{
  SerialOut.begin(115200); //debug stream
  Serial1.begin(115200, SERIAL_8N1, RXP1, TXP1, true); //p1 serial input

  pinMode(DTR_IO, OUTPUT);
  pinMode(LED, OUTPUT);
  // sign of life
  digitalWrite(LED, LED_ON);
  delay(1200);
  digitalWrite(LED, LED_OFF);

  lastReset = getResetReason();
  Debug("\n\n ----> BOOTING....[" _VERSION "] <-----\n\n");
  DebugTln("The board name is: " ARDUINO_BOARD);
  DebugT(F("Last reset reason: ")); Debugln(lastReset);
  
//================ File System =====================================
  if (LittleFS.begin(true)) 
  {
    DebugTln(F("File System Mount succesfull\r"));
    FSmounted = true;     
  } else DebugTln(F("!!!! File System Mount failed\r"));   // Serious problem with File System 
  
//================ Status update ===================================
  P1StatusBegin(); //leest laatste opgeslagen status & rebootcounter + 1
  actT = epoch(actTimestamp, strlen(actTimestamp), true); // set the time to actTimestamp!
  P1StatusWrite();
  LogFile("",false); // write reboot status to file
  readSettings(true);
  
//=============start Networkstuff ==================================
  startWiFi(settingHostname, 240);  // timeout 4 minuten
  startTelnet();
  startMDNS(settingHostname);
  startNTP();

//================ Start MQTT  ======================================
if ( (strlen(settingMQTTbroker) > 3) && (settingMQTTinterval != 0) ) connectMQTT();

//================ Check necessary files ============================
  if (!DSMRfileExist(settingIndexPage, false) ) {
    DebugTln(F("Oeps! Index file not pressent, try to download it!\r"));
    GetFile(settingIndexPage); //download file from cdn
    if (!DSMRfileExist(settingIndexPage, false) ) { //check again
      DebugTln(F("Index file still not pressent!\r"));
      FSNotPopulated = true;
      }
  }
  if (!FSNotPopulated) {
    DebugTln(F("FS correct populated -> normal operation!\r"));
    httpServer.serveStatic("/", LittleFS, settingIndexPage);
  }
 
  if (!DSMRfileExist("/Frontend.json", false) ) {
    DebugTln(F("Frontend.json not pressent, try to download it!"));
    GetFile("/Frontend.json");
  }
  setupFSexplorer();
 
  DebugTf("Startup complete! actTimestamp[%s]\r\n", actTimestamp);  

//================ Start Slimme Meter ===============================

  DebugTln(F("Enable slimmeMeter..\n"));
  slimmeMeter.enable(true);

//================ End of Slimmer Meter ============================

  //create a task that will be executed in the Task1code() function, with priority 1 and executed on core 0
  xTaskCreatePinnedToCore(
                    CPU0Loop,   /* Task function. */
                    "CPU0",     /* name of task. */
                    30000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &CPU0,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 0 */                  
  delay(250); 

  esp_register_shutdown_handler(ShutDownHandler);

  setupWater();
  setupAuxButton(); //esp32c3 only

  if (EnableHistory) CheckRingExists();

} // setup()


//2nd proces
void CPU0Loop( void * pvParameters ){
  while(true) {
    //--- verwerk inkomende data
    if ( slimmeMeter.loop() ) PrevTelegram = slimmeMeter.raw();   
        
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
void doSystemTasks() {
  
  handleSlimmemeter();
  MQTTclient.loop();
  httpServer.handleClient();
  handleKeyInput();
  yield();
  
} // doSystemTasks()

void loop () { 
  
  doSystemTasks();

  //--- update statusfile + ringfiles
  if ( DUE(antiWearRing) || RingCylce ) writeRingFiles(); //eens per 25min + elk uur overgang

  if (DUE(StatusTimer)) { //eens per 10min of indien extra m3
    P1StatusWrite();
    MQTTSentStaticInfo();
    CHANGE_INTERVAL_MIN(StatusTimer, 10);
  }
  
  handleRemoteUpdate();

  //only when compiler option is set
  handleButton();
  handleWater();
  handleNTP();
  
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
