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
-- message bij drempelwaardes 
-- verbruiksrapport einde dag/week/maandstartWiFi
- C3: logging via usb poort mogelijk maken
- detailgegevens voor korte tijd opslaan in werkgeheugen (eens per 10s voor bv 1 uur)
- feature: nieuwe meter = beginstand op 0
- front-end: splitsen dashboard / eenmalige instellingen bv fases 
- front-end: functie toevoegen die de beschikbare versies toont (incl release notes) en je dan de keuze geeft welke te flashen. (Erik)
- remote-update: redirect  na succesvolle update (Erik)
- verbruik - teruglevering lijn door maandgrafiek (Erik)
- automatische update
- influxdb koppeling onderzoeken
- auto update check + update every night 3:<random> hour
- issue met reconnect dns name mqtt (Eric)
- auto switch 3 - 1 fase max fuse
- localisation frontend (resource files) https://phrase.com/blog/posts/step-step-guide-javascript-localization/
- issue met basic auth afscherming rng bestanden
- scheiding tussen T1 en T2 willen zien
- temparatuur ook opnemen in grafieken (A van Dijken)
- websockets voor de communicatie tussen client / dongle ( P. van Bennekom )
- 90 dagen opslaan van uur gegevens ( R de Grijs )
- eigen NTP server instellen ( P. bij de Leij )
- fixed ip kunnen opgeven ( P. bij de Leij )
- eigen NTP kunnen opgeven of juist niet (stopt pollen)
- Roberto: P1 H2O watersensor gegevens apart versturen (MQTT) van P1 
- Sluipverbruik bijhouden
- localisation + 3 decimals frontend tables (R Wiegers)
- issue: wegvallen wifi geen reconnect / reconnect mqtt

WiP
√ raw poort ook in web settings muteren
√ bugje: upload new firmware zonder ingave beginnen en rebooten ( P. bij de Leij )
√ waterstand in telnet bugfix ( Roberto )
√ Dongle type in Preferences T1 = PRo Bridge, T0 - Pro
- Initiele verbruik wegschrijven
-- check initeel opstart
-- schrijf initele data weg op 1 slot eerder
√- avoid peaks default aan

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
//#define SHOW_PASSWRDS             // well .. show the PSK key and MQTT password, what else?     
#define SE_VERSION
//#define USE_NTP_TIME              
//#define ETHERNET
//#define STUB                      //test only : first draft
//#define HEATLINK                  //first draft
//#define INSIGHT            

#define ALL_OPTIONS "[CORE]"
  
#ifdef SE_VERSION
  #undef ALL_OPTIONS
  #define ALL_OPTIONS "[CORE][SE]"
#endif

#ifdef HEATLINK
  #undef ALL_OPTIONS
  #define ALL_OPTIONS "[CORE][Q]"
#endif

/******************** don't change anything below this comment **********************/
#include "DSMRloggerAPI.h"

//===========================================================================================
void setup() 
{
  SerialOut.begin(115200); //debug stream
  P1StatusBegin(); //leest laatste opgeslagen status & rebootcounter + 1

  pinMode(DTR_IO, OUTPUT);
  pinMode(LED, OUTPUT);

  if ( P1Status.dev_type == PRO_BRIDGE ){
    pinMode(PRT_LED, OUTPUT);
    digitalWrite(PRT_LED, true); //default on
    RGBLED.begin();           
    RGBLED.clear();
    RGBLED.show();            
    RGBLED.setBrightness(BRIGHTNESS);
  }
  // sign of life = ON during setup
  SwitchLED( LED_ON, BLUE );
  delay(1000);
  SwitchLED( LED_OFF, BLUE );

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
  actT = epoch(actTimestamp, strlen(actTimestamp), true); // set the time to actTimestamp!
  P1StatusWrite();
  LogFile("",false); // write reboot status to file
  readSettings(true);
  
//=============start Networkstuff ==================================
#ifndef ETHERNET
  startWiFi(settingHostname, 240);  // timeout 4 minuten
#else
  startETH();
#endif
  delay(100);
  startTelnet();
  startMDNS(settingHostname);
  startNTP();
  MQTTclient.setBufferSize(800);

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
 
  esp_register_shutdown_handler(ShutDownHandler);

  setupWater();
  setupAuxButton(); //esp32c3 only

  if (EnableHistory) CheckRingExists();

//================ Start Slimme Meter ===============================
  
  SetupSMRport();
  
//create a task that will be executed in the fP1Reader() function, with priority 2
  if( xTaskCreate( fP1Reader, "p1-reader", 30000, NULL, 2, &tP1Reader ) == pdPASS ) DebugTln(F("Task tP1Reader succesfully created"));
  
  DebugTf("Startup complete! actTimestamp[%s]\r\n", actTimestamp);  
  
} // setup()


//P1 reader task
void fP1Reader( void * pvParameters ){
    DebugTln(F("Enable slimme meter..."));
    slimmeMeter.enable(false);
    while(true) {
      handleSlimmemeter();
      vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void loop () { 
        
        httpServer.handleClient();
        MQTTclient.loop();
        
        yield();
      
       if ( DUE(StatusTimer) && (telegramCount > 2) ) { 
          P1StatusWrite();
          MQTTSentStaticInfo();
          CHANGE_INTERVAL_MIN(StatusTimer, 30);
       }
       
       handleKeyInput();
       handleRemoteUpdate();
       handleButton();
       handleWater();
  
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
