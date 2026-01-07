/*
***************************************************************************  
**  Program  : P1-Dongel-ESP32
**  Copyright (c) 2025 Smartstuff / based on DSMR Api Willem Aandewiel
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      

BACKLOG
- detailgegevens voor korte tijd opslaan in werkgeheugen (eens per 10s voor bv 1 uur)
- verbruik - teruglevering lijn door maandgrafiek (Erik)
- auto switch 3 - 1 fase max fuse
- temperatuur ook opnemen in grafieken (A van Dijken)
- SSE of websockets voor de communicatie tussen client / dongle ( P. van Bennekom )
- 90 dagen opslaan van uur gegevens ( R de Grijs )
- Interface HomeKit ivm triggeren op basis van energieverbruik/teruglevering (Thijs v Z)
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
- RNGDays 31 days
- eigen NTP kunnen opgeven of juist niet (stopt pollen)
- detect and repair issues RNG files
- HA auto update ala : https://www.zigbee2mqtt.io/guide/usage/ota_updates.html#automatic-checking-for-available-updates
- Daily Insights: Inzichten vanaf opstarten dongle / 00:00 reset
    - loadbalancing over de fases heen
    - detail P per fase afgelopen uur (sample eens per 10s)
- kwartierpiek historie opnemen (wanneer nieuwe piek ontstaat)
- dynamische prijzen inl
- improvement: modbus in own process = non-blocking 
- check and repair rng files on startup
- hostname aanpassen met laatste 3 segmenten mac-adres
- Huawei FusionSolar integratie ( Francis )

Default checks
- wifi
- port 82
- MQTT
- webserver
- EID
- dev_pairing
- ethernet
- 4h test on 151

- default mqtt : mqtt://core-mosquitto:1883 en addons als user
- change: solar support for 3 inverters
- Shelly EM udp emulation
- issue cost gas 4.16/5.2 (Karel)
- ESPHome migratie voor de Ultra / Ultra V2 en Ultra X2 gaat niet goed. Wijst naar 1 esphome versie. -> oplossen in de updata routine omdat in de dongle duidelijk is welke hw versie het is.
- bug HW api water meter id

Planner display checks
- niet aanwezig
- updaten 5.2 en de 3.0.0
- eid stond aan maar is uitgezet
- uur overgang

task wtd
- https://forum.arduino.cc/t/watchdog-reset-esp32-if-stuck-more-than-120-seconds/1266565/2
- https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/wdts.html
- https://esp32.com/viewtopic.php?t=31155


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


5.3.0
!!! V5 supports only for P1 Pro/Pro+, Ethernet Pro/Pro+ and Ultra dongles
√ merge 4.17.1 with 5.2.9
√ add: network connection information
√ update: SDK 3.3.5
√ change: mac/macid/MQTT macid in topic /etc detemined on startup
√ refactor: type and module config
√ change: wifi power management optimised (Low Power when starting)
√ refactor: mac address functions
√ add: settings try_calc_i = true; > (don't) calculate current based on P and U

5.3.1
√ add: auto detect P1 meter (v5/v4 or v2)
- add: indication detected smart meter to Information overview
- change: dongle type to postIP
- modify: split the settings page into main/mqtt/modbus
- winter -> zomertijd issue. 2e uur mist en data van 2 dagen geleden staat er dan.
--> oplossing : 

bool isDSTTransition(int lastHour, int currentHour) {
    return (lastHour == 1 && currentHour == 3); // Detecteer de sprong
}

5.4.0
- refactor: asyncwebserver

*/

/******************** compiler options  ********************************************/

#define DEBUG
// #define INSIGHTS
// #define XTRA_LOG

//PROFILES -> NO PROFILE = WiFi Dongle 
#define ULTRA         //ultra (mini) dongle
// #define ETHERNET      //ethernet dongle
// #define ETH_P1EP          //ethernet pro+ dongle
// #define NRG_DONGLE 
// #define DEVTYPE_H2OV2 // P1 Dongle Pro with h2o and p1 out

//SPECIAL
// #define __Az__

//FEATURES
// #define DEV_PAIRING
#define MBUS
//#define MQTT_DISABLE
//#define NO_STORAGE
//#define VOLTAGE_MON
// #define NO_HA_AUTODISCOVERY
//#define POST_TELEGRAM
// #define MQTTKB
// #define MB_RTU
#define ESPNOW
// #define SHELLY_EMU
// #define USB_CONFIG
// #define POST_POWERCH

#include "DSMRloggerAPI.h"
#include <esp_task_wdt.h>

void setup() 
{
  DebugBegin(115200);
  uint16_t Freq = getCpuFrequencyMhz();
  setCpuFrequencyMhz(80); //lower power mode
  USBPrintf( "\n\n------> BOOTING %s %s ( %s %s ) <------\n\n", _DEFAULT_HOSTNAME, _VERSION_ONLY, __DATE__, __TIME__ ); 
  Debugf("Original cpu speed: %d\n",Freq);
  SetupWDT();
  SetupButton();
  GetMacAddress();

  P1StatusBegin(); //leest laatste opgeslagen status & rebootcounter + 1
  SetConfig();
  WDT_FEED();
  lastReset = getResetReason();
  DebugT(F("Last reset reason: ")); Debugln(lastReset);
  DebugFlush();
//================ File System =====================================
  if ( LittleFS.begin(true) ) { DebugTln(F("FS Mount OK\r")); FSmounted = true;  } 
  else DebugTln(F("!!!! FS Mount ERROR\r"));   // Serious problem with File System 
  WDT_FEED();
//================ Status update ===================================
  actT = epoch(actTimestamp, strlen(actTimestamp), true); // set the time to actTimestamp!
  P1StatusWrite();
  LogFile("",false); // write reboot status to file
  if (!LittleFS.exists(SETTINGS_FILE)) writeSettings(); //otherwise the dongle crashes some times on the first boot
  else readSettings(true);
  WDT_FEED();
//=============scan, repair and convert RNG files ==================
  // CheckRingFile(RINGDAYS);
  // loadRingfile(RINGDAYS);
  // printRecordArray(RNGDayRec, RingFiles[RINGDAYS].slots, "RINGDAYS");
  // saveRingfile(RINGDAYS);

  // patchJsonFile_Add7thValue(RINGMONTHS);
  // patchJsonFile_Add7thValue(RINGHOURS);
  // convertRingfileWithSlotExpansion(RINGDAYS,32);
//=============start Networkstuff ==================================
  USBconfigBegin();
  startNetwork();
  WDT_FEED();
  PostMacIP(); //post mac en ip
  yield();
#ifdef INSIGHTS  
  if ( Insights.begin(INSIGHTS_KEY) ) Debugf("ESP Insights enabled Node ID %s\n", Insights.nodeID());
#endif  
  WDT_FEED();
  startTelnet();
  startMDNS(settingHostname);
  startNTP();
  WDT_FEED();
//================ Check necessary files ============================
  if ( !skipNetwork ) {
  if (!DSMRfileExist(settingIndexPage, false) ) {
    DebugTln(F("Oeps! Index file not pressent, try to download it!\r"));
    GetFile(settingIndexPage, PATH_DATA_FILES); //download file from cdn
    if (!DSMRfileExist(settingIndexPage, false) ) {
      DebugTln(F("Oeps! Index file not pressent, try to download it!\r"));
      GetFile(settingIndexPage, URL_INDEX_FALLBACK);
    }
    if (!DSMRfileExist(settingIndexPage, false) ) { //check again
      DebugTln(F("Index file still not pressent!\r"));
      FSNotPopulated = true;
      }
  }
  WDT_FEED();
  if (!DSMRfileExist("/Frontend.json", false) ) {
    DebugTln(F("Frontend.json not pressent, try to download it!"));
    GetFile("/Frontend.json", PATH_DATA_FILES);
  }
  
  setupFSexplorer();
  } //! skipNetwork
  WDT_FEED();
  esp_register_shutdown_handler(ShutDownHandler);

  setupWater();
  WDT_FEED();
  if (EnableHistory) CheckRingExists();
  SetupNetSwitch();

//================ Start Slimme Meter ===============================

#ifdef MBUS
  mbusSetup();
  SetupMB_RTU();
#endif  
  ReadSolarConfigs();
  ReadAccuConfig();
  delay(500);
  setCpuFrequencyMhz(Freq); //restore original clockspeed

  StartP1Task();
  StartMqttTask();
  EIDStart();
  ShellyEmuBegin();
  // setupWS();
  
  DebugTf("Startup complete! actTimestamp[%s]\r\n", actTimestamp);  
  StartESPNOW();
  StartPowerCH();
} // setup()

void loop () { 
  esp_task_wdt_reset();
  httpServer.handleClient();      
  if ( DUE(StatusTimer) && (telegramCount > 2) ) { 
    P1StatusWrite();
    MQTTSentStaticInfo();
    CHANGE_INTERVAL_MIN(StatusTimer, 30);
  }
  handleKeyInput();
  handleRemoteUpdate();
  handleWater();
  handleEnergyID();  
  PostTelegram();
  GetSolarDataN();
  handleVirtualP1();
  PrintHWMark(2);
  handleP2P();
} // loop()