/*
***************************************************************************  
**  Program  : P1-Dongel-ESP32
**  Copyright (c) 2024 Smartstuff / based on DSMR Api Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      

WENSEN
- detailgegevens voor korte tijd opslaan in werkgeheugen (eens per 10s voor bv 1 uur)
- verbruik - teruglevering lijn door maandgrafiek (Erik)
- auto switch 3 - 1 fase max fuse
- temparatuur ook opnemen in grafieken (A van Dijken)
- SSE of websockets voor de communicatie tussen client / dongle ( P. van Bennekom )
- 90 dagen opslaan van uur gegevens ( R de Grijs )
- Roberto: P1 H2O watersensor gegevens apart versturen (MQTT) van P1 
- optie in settings om te blijven proberen om de connectie met de router te maken (geen hotspot) (Wim Zwart)
- Interface HomeKit ivm triggeren op basis van energieverbruik/teruglevering (Thijs v Z)
- #18 water en gas ook in de enkele json string (mqtt)
- spanning bij houden igv teruglevering (+% teruglevering weergeven van de dag/uur/maand)
- loadbalancing bijhouden over de fases
- grafische weergave als standaardoptie weergave en cijferlijsten als tweede keuze. Nu is het andersom. 
- Consistentie tijd-assen, links oud, rechts nieuw
  - in Actueel staat de laatste meting rechts en de oudste meting links
  - in de uurstaat loopt de tijd van rechts (oudst) naar links (laatste uurmeting)
- Harold B: Dark-mode frontend
- Harold B: dynamische tarieven dus de onderverdeling naar Tarief 1 en 2 is niet relevant. (Overigens de P1-meter levert wel twee standen aan). Persoonlijk vind ik de grafieken onleesbaar worden (ik lever ook terug) vier verschillende kleurtjes groen en vier kleurtjes rood. Dus het heeft mijn voorkeur om dit onderscheid in de grafieken achterwege te laten. Dus als dat aan te sturen zou zijn via de instellingen, heel graag!
- Idee voor een toekomstige release: hergebruik de Prijsplafond grafieken voor een vergelijk tussen Afname en Levering gedurende het jaar. Ik zit steeds uit te rekenen of ik overschot aan kWh heb of inmiddels een tekort. De grafieken maken dat wel helder. ( Leo B )
- front-end: issue Stroom ( terug + afname bij 3 fase wordt opgeteled ipv - I voor teruglevering )
- Rob v D: 'Actueel' --> 'Grafisch' staat gasverbruik (blauw) vermeld, terwijl ik geen gas heb (verbruik is dan ook nul). Waterverbruik zie ik daar niet. In de uur/dag/maand overzichten zie ik wel water en geen gas.
- RNGhours files vergroten (nu 48h -> 336h) (Broes)
- teruglevering dashboard verkeerde verhoudingen ( Pieter ) 
- localisation frontend (resource files) https://phrase.com/blog/posts/step-step-guide-javascript-localization/
- RNGDays 31 days
- eigen NTP kunnen opgeven of juist niet (stopt pollen)
- detect and repair issues RNG files

- experimenteel: Inzichten vanaf opstarten dongle
    - sluipverbruik (meten tussen 23u - 06u, uitgaande dat er dan geen teruglevering is)
    - loadbalancing over de fases heen
    - Pmax per fase 
    - Overspanning per fase (gewist om 00:00)
    - detail P per fase afgelopen uur (sample eens per 10s)

4.12.0
√ SDK 3.1.3
√ utilisation info
√ Calculate current I = P/U because this is more accurate. NL is 0  and BE 1 decimal
- checken dev_pairing
√ containskey migration to .is<>
- ArduinoJson 7.x update

4.12.1
- inlezen van solar config in frontend

************************************************************************************
Arduino-IDE settings for P1 Dongle hardware ESP32:
  - Board: "ESP32C3 Dev Module"
  - Flash mode: "QIO"
  - Flash size: "4MB (32Mb)"
  - CorenDebug Level: "None"
  - Flash Frequency: "80MHz"
  - CPU Frequency: "160MHz"
  - Upload Speed: "961600"                                                                                
  - Port: <select correct port>
*/
/******************** compiler options  ********************************************/
// #define DEBUG
// #define XTRA_LOG

//PROFILES
// #define ULTRA         //ultra (mini) dongle
// #define ETHERNET      //ethernet dongle
// #define ETH_P1EP          //ethernet pro+ dongle
// #define NRG_DONGLE   
// #define DEVTYPE_H2OV2 // P1 Dongle Pro with h2o and p1 out
//#define P1_WIFI       // DOES NOTHING; 
// #define __Az__

//FEATURES
#define DEV_PAIRING
#define MBUS
//#define MQTT_DISABLE
//#define NO_STORAGE
//#define VOLTAGE_MON
// #define NO_HA_AUTODISCOVERY
//#define POST_TELEGRAM
// #define MQTTKB

#include "DSMRloggerAPI.h"

void setup() 
{
  // make_version();
  uint16_t Freq = getCpuFrequencyMhz();
  setCpuFrequencyMhz(80); //lower power mode
  DebugBegin(115200);
  USBPrintf( "\n\n------> BOOTING %s [%s] <------\n\n", _DEFAULT_HOSTNAME, Firmware.Version ); 
  Debugf("Original cpu speed: %d\n",Freq);
  PushButton.begin(IO_BUTTON);

  P1StatusBegin(); //leest laatste opgeslagen status & rebootcounter + 1
  SetConfig();
  lastReset = getResetReason();
  DebugT(F("Last reset reason: ")); Debugln(lastReset);
  DebugFlush();
//================ File System =====================================
  if ( LittleFS.begin(true) ) { DebugTln(F("FS Mount OK\r")); FSmounted = true;  } 
  else DebugTln(F("!!!! FS Mount ERROR\r"));   // Serious problem with File System 
//================ Status update ===================================
  actT = epoch(actTimestamp, strlen(actTimestamp), true); // set the time to actTimestamp!
  P1StatusWrite();
  LogFile("",false); // write reboot status to file
  if (!LittleFS.exists(SETTINGS_FILE)) writeSettings(); //otherwise the dongle crashes some times on the first boot
  else readSettings(true);

//=============start Networkstuff ==================================
  
  startNetwork();
  PostMacIP(); //post mac en ip   
  delay(100);
  startTelnet();
  startMDNS(settingHostname);
  startNTP();
#ifndef MQTT_DISABLE
  MQTTSetBaseInfo();
  MQTTsetServer();
#endif  

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

  if (EnableHistory) CheckRingExists();
  SetupNetSwitch();

//================ Start Slimme Meter ===============================
  
  SetupSMRport();
  
//create a task that will be executed in the fP1Reader() function, with priority 2
  if( xTaskCreate( fP1Reader, "p1-reader", 30000, NULL, 2, &tP1Reader ) == pdPASS ) DebugTln(F("Task tP1Reader succesfully created"));

  DebugTf("Startup complete! actTimestamp[%s]\r\n", actTimestamp);  

#ifdef MBUS
  mbusSetup();
#endif  
  ReadSolarConfigs();
  delay(500);
  setCpuFrequencyMhz(Freq); //restore original clockspeed

} // setup()


//P1 reader task
void fP1Reader( void * pvParameters ){
    DebugTln(F("Enable slimme meter..."));
    SetupP1Out();
    slimmeMeter.enable(false);
#ifdef ULTRA
    digitalWrite(DTR_IO, LOW); //default on
#endif   
    while(true) {
      handleSlimmemeter();
      P1OutBridge();
      vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void loop () { 
        
        httpServer.handleClient();
        handleMQTT();   
        yield();
      
       if ( DUE(StatusTimer) && (telegramCount > 2) ) { 
          P1StatusWrite();
          MQTTSentStaticInfo();
          CHANGE_INTERVAL_MIN(StatusTimer, 30);
       }
       WifiWatchDog();
       handleKeyInput();
       handleRemoteUpdate();
       PushButton.handler();
       handleWater();
       handleEnergyID();  
       PostTelegram();
       GetSolarDataN();
       handleVirtualP1();
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
