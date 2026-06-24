/*
***************************************************************************  
**  Program  : P1-Dongel-ESP32
**  Copyright (c) 2026 Smartstuff / based on DSMR Api Willem Aandewiel
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      

BACKLOG
- detailgegevens voor korte tijd opslaan in werkgeheugen (eens per 10s voor bv 1 uur)
- 90 dagen opslaan van uur gegevens ( R de Grijs )
- in de uurstaat loopt de tijd van rechts (oudst) naar links (laatste uurmeting)
- Harold B: Dark-mode frontend
- Harold B: dynamische tarieven dus de onderverdeling naar Tarief 1 en 2 is niet relevant. (Overigens de P1-meter levert wel twee standen aan). Persoonlijk vind ik de grafieken onleesbaar worden (ik lever ook terug) vier verschillende kleurtjes groen en vier kleurtjes rood. Dus het heeft mijn voorkeur om dit onderscheid in de grafieken achterwege te laten. Dus als dat aan te sturen zou zijn via de instellingen, heel graag!
- front-end: issue Stroom ( terug + afname bij 3 fase wordt opgeteled ipv - I voor teruglevering )
- RNGhours files vergroten (nu 48h -> 336h) (Broes)
- eigen NTP kunnen opgeven of juist niet (stopt pollen)
- detect and repair issues RNG files
- Daily Insights: Inzichten vanaf opstarten dongle / 00:00 reset
    - loadbalancing over de fases heen
    - detail P per fase afgelopen uur (sample eens per 10s)
- kwartierpiek historie opnemen (wanneer nieuwe piek ontstaat)
- Huawei FusionSolar integratie ( Francis )
- inlezen van solar config in frontend
- issue cost gas 4.16/5.2 (Karel)
- De waarden in daily insights labelen met het datum/tijdstip waarop gemeten (Harrie)
- one hostname for all dongles (V6)
- add tempature  in price cap (A van Dijken) = lat / long of location selection 

Default checks
- wifi
- port 82
- MQTT
- webserver
- EID
- ethernet
- 4h test on 151

Planner display checks
- niet aanwezig
- updaten 5.2 en de 3.0.0
- eid stond aan maar is uitgezet
- uur overgang

************************************************************************************
Arduino-IDE settings for P1 Dongle hardware ESP32:
  - Board: "ESP32C3 Dev Module"
  - Flash mode: "QIO"
  - Flash size: "4MB (32Mb)"
  - CorenDebug Level: "None"
  - Flash Frequency: "80MHz"
  - CPU Frequency: "160MHz"
  - Upload Speed: "961600"                                                                                
  - Port: <select port>

5.8.4
- SDK 3.3.10
- add: support of the hex water sensor dongles
- fix: P1UM rtu support

5.9.0
- SDK 3.3.10
- 3 button control (a-pair, b-reboot, c=factory reset)
- tooltips bij de diverse settings (Gerben)
- Add remote Proxy
- refactoring targets
- add wallbox mapping
- kWh meter als bron voor productie data gebruiken (Harrie)
- update button in HA to trigger the update (mqtt based #70) 
   - HA auto update ala : https://www.zigbee2mqtt.io/guide/usage/ota_updates.html#automatic-checking-for-available-updates
- extra report "jaarbalans": op basis van nog te verwachten maandnw ( Leo B )

*/

/******************** compiler options  ********************************************/

// #define DEBUG
// #define XTRA_LOG

//---  PROFILES  ---
// #define ULTRA            //ultra (mini) dongle
// #define ETHERNET         //ethernet dongle
// #define ETH_P1EP         //ethernet pro+ dongle
// #define NRG_DONGLE       // + D1MC and NRGDH 
// #define _P1P

//SPECIAL
// #define __Az__
// #define OTAURL_PREFIX "me/"

//FEATURES
#define MBUS
// #define MQTT_DISABLE
// #define MB_RTU
#define ESPNOW  
// #define UDP_BCAST
// #define USB_CONFIG
// #define POST_POWERCH
// #define POST_MEENT
// #define VIRTUAL_P1
// #define HAN_READER
// #define HAN_TESTDATA
// #define HAN_TESTDATA_RAW
// #define HAN_TESTDATA_DYNAMIC
// #define DIRECT_AP_CONNECT 1

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
  WorkerBegin();
  actT = epoch(actTimestamp, strlen(actTimestamp), true); // set the time to actTimestamp!
  P1StatusWrite();
  LogFile("",false); // write reboot status to file
  if (!LittleFS.exists(SETTINGS_FILE)) writeSettingsDirect(); //otherwise the dongle crashes some times on the first boot
  else readSettings(true);
  if (LittleFS.exists("/Frontend.json")) LittleFS.remove("/Frontend.json");
#if DIRECT_AP_CONNECT
  EnableHistory = false;
  FSNotPopulated = false;
#endif
  WDT_FEED();
//=============start Networkstuff ==================================
  USBconfigBegin();
  startNetwork();
  WDT_FEED();
  PostMacIP(); //post mac en ip
  startTelnet();
  startMDNS(settingHostname);
  startNTP();
  handleManifestCheckSchedule(true); // startup manifest check right after network and time init
  WDT_FEED();
//================ Check necessary files ============================
  if ( !skipNetwork ) {
#if !DIRECT_AP_CONNECT
  if (!EnsureIndexFilePresent()) FSNotPopulated = true;
#endif
  
  setupFSexplorer();
  } //! skipNetwork
  WDT_FEED();
  esp_register_shutdown_handler(ShutDownHandler);

  setupWater();
  if (EnableHistory) {
    CheckRingExists();
    if (loadRNGDaysHistory()) DebugTln(F("Day history restored from RNGdays"));
    else DebugTln(F("Day history not restored: will initialize from current values"));
  }
  SetupNetSwitch();

//================ Start Slimme Meter ===============================

#ifdef MBUS
  mbusSetup();
  SetupMB_RTU();
#endif  
  ReadSolarConfigs();
  delay(500);
  setCpuFrequencyMhz(Freq); //restore original clockspeed

  StartP1Task();
  StartMqttTask();
  EIDStart();
  ShellyEmuBegin();
  setupApiWebSocket();
  
  DebugTf("Startup complete! actTimestamp[%s]\r\n", actTimestamp);  
  StartESPNOW();
  StartWebhook();
  UdpBegin();
} // setup()

void loop () { 
  esp_task_wdt_reset();
  httpServer.handleClient();
  handleApiWebSocket();
  if ( DUE(StatusTimer) && (telegramCount > 2) ) { 
    P1StatusWrite();
    MQTTSentStaticInfo();
    CHANGE_INTERVAL_MIN(StatusTimer, 30);
  }
  handleKeyInput();
  handleManifestCheckSchedule(false);
  WifiWatchDog();
  handleRemoteUpdate();
  handleWater();
  handleEnergyID();  
  GetSolarDataN();
  handleRawPort();
  handleVirtualP1();
  PrintHWMark(2);
  handleP2P();
  handleUDP();
  PostWebhook();
} // loop()
