#ifndef _NETWORK_H
#define _NETWORK_H
/*
***************************************************************************  
**  Program  : network.h, part of DSMRloggerAPI
**
**  Copyright (c) 2021 Willem Aandewiel / Martijn Hendriks
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

#include "DSMRloggerAPI.h"
#include "esp_timer.h"

#ifdef ETHERNET
  #include <ETH.h>
  #include <SPI.h>
  #include "esp_mac.h"

  #define ETH_TYPE            ETH_PHY_W5500
  #define ETH_RST            -1
  #define ETH_ADDR            1

  //workaround to use the ESP_MAC_ETH mac address instead of local mac address
  class EthernetClass {
  public:
    bool myBeginSPI(ETHClass& eth, eth_phy_type_t type, int32_t phy_addr, uint8_t *mac_addr, int cs, int irq, int rst, SPIClass *spi, int sck, int miso, int mosi, spi_host_device_t spi_host, uint8_t spi_freq_mhz);
  };

  bool EthernetClass::myBeginSPI(ETHClass& eth, eth_phy_type_t type, int32_t phy_addr, uint8_t *mac_addr, int cs, int irq, int rst, SPIClass *spi, int sck, int miso, int mosi, spi_host_device_t spi_host, uint8_t spi_freq_mhz) {
    return eth.beginSPI(type, phy_addr, mac_addr, cs, irq, rst, spi, sck, miso, mosi, spi_host, spi_freq_mhz);
  }

  EthernetClass myEthernet;
#endif

//int WifiDisconnect = 0;
bool bNoNetworkConn = false;
bool bEthUsage = false;
static esp_timer_handle_t s_reconnTimer = nullptr;
static bool     s_authBlocked = false;
static uint32_t s_lastKickMs  = 0;
static const uint32_t KICK_DEBOUNCE_MS = 1500;

static inline bool isCoreReconnectable(uint8_t r){
  switch (r) {
    case WIFI_REASON_UNSPECIFIED:
    // timeouts (retry)
    case WIFI_REASON_AUTH_EXPIRE:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
    case WIFI_REASON_802_1X_AUTH_FAILED:     // enterprise: core probeert nog
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    // transient (reconnect)
    case WIFI_REASON_AUTH_LEAVE:
    case WIFI_REASON_ASSOC_EXPIRE:
    case WIFI_REASON_ASSOC_TOOMANY:
    case WIFI_REASON_NOT_AUTHED:
    case WIFI_REASON_NOT_ASSOCED:
    case WIFI_REASON_ASSOC_NOT_AUTHED:
    case WIFI_REASON_MIC_FAILURE:
    case WIFI_REASON_IE_IN_4WAY_DIFFERS:
    case WIFI_REASON_INVALID_PMKID:
    case WIFI_REASON_BEACON_TIMEOUT:         // 200
    case WIFI_REASON_NO_AP_FOUND:            // 201
    case WIFI_REASON_ASSOC_FAIL:
    case WIFI_REASON_CONNECTION_FAIL:
    case WIFI_REASON_AP_TSF_RESET:
    case WIFI_REASON_ROAMING:
      return true;
    default:
      return false;
  }
}

WiFiManager manageWiFi;


// Echte “fatal auth” (verkeerd wachtwoord) → blokkeer tot user ingrijpt
static inline bool isAuthFatal(uint8_t r){
  switch (r) {
    case WIFI_REASON_AUTH_FAIL:            // 202 op veel targets
    case WIFI_REASON_INVALID_PMKID:        // vaak ook definitief fout
      return true;
    default:
      return false;
  }
}

static void kickOnce(){
  uint32_t now = millis();
  if (now - s_lastKickMs < KICK_DEBOUNCE_MS) return;
  esp_wifi_start();                 //  start STA if stopped
  esp_wifi_connect();               // driver allready in action → ESP_ERR_WIFI_CONN
  s_lastKickMs = now;
}

void GetMacAddress(){

  String _mac = MAC_Address();
  strcpy( macStr, _mac.c_str() );
  _mac.replace( ":","" );
  strcpy( macID, _mac.c_str() );
  USBPrint( "MacStr : " );USBPrintln( macStr ); //only at setup
//  USBSerial.print( "MacID: " );USBSerial.println( macID );
}

/***===========================================================================================
    POST MAC + IP
    https://www.allphptricks.com/create-and-consume-simple-rest-api-in-php/
    http://g2pc1.bu.edu/~qzpeng/manual/MySQL%20Commands.htm
**/
void PostMacIP() {
  if ( skipNetwork ) return;
  HTTPClient http;
  http.begin(wifiClient, APIURL);
  http.setConnectTimeout(4000);
  http.setTimeout(5000);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
  String httpRequestData = "mac=" + String(macStr) + "&ip=" + IP_Address() + "&version=" + _VERSION_ONLY;           
  
  int httpResponseCode = http.POST(httpRequestData);

#ifdef DEBUG  
  DebugT(F("HTTP RequestData: "));Debugln(httpRequestData);
  DebugT(F("HTTP Response code: "));Debugln(httpResponseCode);
#endif  

  http.end();  
}

void WifiOff(){
  if ( WiFi.isConnected() ) WiFi.disconnect(false,false);
#ifdef CONFIG_BT_ENABLED
  btStop();
#endif
  WiFi.mode(WIFI_OFF);
  esp_wifi_stop();
  // esp_wifi_deinit();
  WiFi.setSleep(true);
}

// https://github.com/espressif/arduino-esp32/blob/master/libraries/Network/src/NetworkEvents.h
static void onNetworkEvent (WiFiEvent_t event, arduino_event_info_t info) {
  switch (event) {
#ifdef ETHERNET  
  //****** ETH    
    case ARDUINO_EVENT_ETH_START: //1
      DebugTln("ETH Started");
      ETH.setHostname(settingHostname);
      break;
    case ARDUINO_EVENT_ETH_CONNECTED: //3
      DebugTln("\nETH Connected");
      WifiOff();
      netw_state = NW_ETH_LINK;
      bEthUsage = true; //set only once 
      break;
    case ARDUINO_EVENT_ETH_GOT_IP6: //7
      LogFile("ETH GOT IP V6", true);
      [[fallthrough]];
    case ARDUINO_EVENT_ETH_GOT_IP: //5
      {
        netw_state = NW_ETH;
        WifiOff();
        LogFile("ETH GOT IP", true);
        bEthUsage = true; //set only once 
        SwitchLED( LED_ON, LED_BLUE ); //Ethernet available = RGB LED Blue
        // tLastConnect = 0;
        break;
      }
    // case ARDUINO_EVENT_ETH_LOST_IP: //6 not available in SDK 2.x
    //   DebugTln("!!! ETH Lost IP");
    //   netw_connected = false;
    //   break;
    case ARDUINO_EVENT_ETH_STOP: //2
      DebugTln("!!! ETH Stopped");
      [[fallthrough]];
    case ARDUINO_EVENT_ETH_DISCONNECTED: //4
      if ( netw_state != NW_WIFI ) netw_state = NW_NONE;
      SwitchLED( LED_ON , LED_RED );
      LogFile("ETH Disconnected", true);
      break;
#endif //ETHERNET
    //****** WIFI
    case ARDUINO_EVENT_WIFI_STA_START: //110
      // initial nudge (soms is begin() traag met eerste connect)
      // if ( !manageWiFi.getConfigPortalActive() ) kickOnce();
      // Debugln("--- WIFI START ----");
      break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED: //4
        sprintf(cMsg,"Connected to %s. Asking for IP address", WiFi.BSSIDstr().c_str());
        LogFile(cMsg, true);
        break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP: //7
        LogFile("Wifi Connected",true);
        SwitchLED( LED_ON, LED_BLUE );
        Debug (F("IP address: " ));  Debug (WiFi.localIP());Debug(" )\n\n");

        if ( bEthUsage ) WifiOff();        
        else netw_state = NW_WIFI;
        s_authBlocked = false;
        s_lastKickMs = 0;               // reset voor snelle roam
        bNoNetworkConn = false;
        break;
    case ARDUINO_EVENT_WIFI_STA_STOP: //8
      // Voor de zekerheid: driver soms stopt STA → start ’m weer
      // LogFile("Wifi STA stopped-> restart",true);
      // esp_wifi_start(); // idempotent als al gestart
      break;
    case ARDUINO_EVENT_WIFI_STA_LOST_IP: //117
        if ( netw_state != NW_ETH ) netw_state = NW_NONE;
        if ( !bNoNetworkConn ) LogFile("Wifi connection lost - LOST IP",true); //only once
        break;           
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: //5
      {
        #ifdef ULTRA
          if ( netw_state != NW_ETH ) SwitchLED( LED_ON, LED_RED );
        #else
          SwitchLED( LED_OFF, LED_BLUE );
        #endif      
        uint8_t reason = info.wifi_sta_disconnected.reason;
        String discon_res = "Wifi connection lost - DISCONNECTED | reason: " + String(reason);
        // if ( !bNoNetworkConn ) LogFile("Wifi connection lost - DISCONNECTED",true); //only once
        LogFile(discon_res.c_str(),true); //every time
              
        bNoNetworkConn = true;

        if (isAuthFatal(reason)) {
        s_authBlocked = true;
        // hier evt: WiFi.disconnect(true,true); startPortal();
        return;
      }
      s_authBlocked = false;

      if (reason == WIFI_REASON_ASSOC_LEAVE /*8*/) {
        // Bekende “stilte”-case: STA valt stil → start & één kick
        esp_wifi_start();
        kickOnce();
        return;
      }

      if (isCoreReconnectable(reason)) {
        // Laat de core z’n autoretry doen; niets extra’s om dubbelwerk te vermijden
        return;
      }

      // Rest: core retryt niet → help een handje (rate-limited)
      
      if ( !manageWiFi.getConfigPortalActive() ) kickOnce();
      else Debugln("config portal active - no kickonce");
        break;
        }
    default:{
        const char *ename = Network.eventName((arduino_event_id_t)event);
        int rssi = WiFi.isConnected() ? WiFi.RSSI() : 0;
        int ch   = WiFi.isConnected() ? WiFi.channel() : 0;
        sprintf(cMsg,
        "Network-event : %d (%s) | %s | rssi:%d | ch:%d", event, ename ? ename : "?", WiFi.isConnected() ? "wifi" : "no-wifi", rssi, ch);
        LogFile(cMsg, true);
        break;}
    }
}

//gets called when WiFiManager enters configuration mode
//===========================================================================================
void configModeCallback (WiFiManager *myWiFiManager) {
  DebugTln(F("Wifi Connection Failed -> Entered config mode\r"));
  DebugTln(WiFi.softAPIP().toString());
  DebugTln(myWiFiManager->getConfigPortalSSID());
  esp_task_wdt_reset();
} // configModeCallback()

//===========================================================================================
#include <esp_wifi.h>

void startWiFi(const char* hostname, int timeOut) {  
if ( skipNetwork ) return;
#if not defined ETHERNET || defined ULTRA
#ifdef ULTRA
  //lets wait on ethernet first for 4.5sec and consume some time to charge the capacitors
  uint8_t timeout = 0;
  while ( (netw_state == NW_NONE) && (timeout++ < 65) ) {
    delay(100); 
    Debug("e");
    esp_task_wdt_reset();
  } 
  Debugln();
  if ( netw_state != NW_NONE ) return;
  enterPowerDownMode(); //disable ETH after 6.5 sec waiting ... lower power consumption
#endif 
  
  if ( netw_state != NW_NONE ) return;

  esp_wifi_set_ps(WIFI_PS_MAX_MODEM); //lower calibration power

  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);  
  esp_wifi_set_ps(WIFI_PS_NONE); // IDF: forced NO modem-sleep

  WiFi.setHostname(hostname);
  WiFi.setMinSecurity(WIFI_AUTH_WPA_PSK);
  
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);  //to solve mesh issues 
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);      //to solve mesh issues 
  if ( bFixedIP ) WiFi.config(staticIP, gateway, subnet, dns);
  LogFile("Wifi Starting",true);
  SwitchLED( LED_OFF, LED_BLUE );  
  
  manageWiFi.setConnectTimeout(15);
  manageWiFi.setConfigPortalBlocking(false);
  manageWiFi.setDebugOutput(false);
  manageWiFi.setShowStaticFields(true); // force show static ip fields
  manageWiFi.setShowDnsFields(true);    // force show dns field always  
  manageWiFi.setRemoveDuplicateAPs(false);
  manageWiFi.setScanDispPerc(true); // display percentages instead of graphs for RSSI
  //  manageWiFi.setWiFiAutoReconnect(true); //buggy

  manageWiFi.setClass("invert"); //dark theme
  
  //--- set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  manageWiFi.setAPCallback(configModeCallback);
  manageWiFi.setConfigPortalTimeout(timeOut); //config portal timeout in seconds
  
  esp_task_wdt_reset();
  manageWiFi.autoConnect(_HOTSPOT);

  //handle wifi webinterface timeout and connection (timeOut in sec) timeout in 30 sec
  uint16_t i = 0;
  while ( (i++ < 3000) && (netw_state == NW_NONE) && !bEthUsage && !skipNetwork ) {
    Debug("*");
    delay(100);
    manageWiFi.process();
    PushButton.handler();
    esp_task_wdt_reset();
    SwitchLED(i%4?LED_ON:LED_OFF,LED_BLUE); //fast blinking
  }
  Debugln();
  manageWiFi.stopWebPortal();
  if ( skipNetwork ) return; 
  if ( netw_state == NW_NONE && !bEthUsage ) {
    LogFile("reboot: Wifi failed to connect and hit timeout",true);
    P1Reboot(); //timeout 
  }
  if ( netw_state == NW_ETH || bEthUsage ) {
    WifiOff();
    delay(500);
  }
  SwitchLED( LED_ON, LED_BLUE );
#else 
  Debugln(F("NO WIFI SUPPORT"));
#endif //ifndef ETHERNET
} // startWiFi()

//===========================================================================================
void WaitOnNetwork()
{
  if ( skipNetwork ) { 
    enterPowerDownMode();   //ethernet power down
    WifiOff();              //wifi power down
    EnableHistory = false;  //not usefull to store data
    writeSettings();        //make persistant
    SwitchLED(LED_OFF,LED_BLUE); //erase led status 
    return; 
    }
  while ( netw_state != NW_ETH && netw_state != NW_WIFI ) { //endless wait for network connection
    Debug(".");
    delay(200);
    esp_task_wdt_reset();
  }
  Debugln("\nNetwork connected");
}

bool loadFixedIPConfig(const char *filename) {

  File file = LittleFS.open(filename, "r");
  if (!file) {
    Debugln("Configuratiebestand niet gevonden.");
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Debugln("Fout bij het parsen van JSON-bestand.");
    file.close();
    return false;
  }

  staticIP.fromString(doc["static_ip"].as<const char *>());
  gateway.fromString(doc["gateway"].as<const char *>());
  subnet.fromString(doc["subnet"].as<const char *>());
  dns.fromString(doc["dns"].as<const char *>());

  file.close();
  return true;
}

// validate fixed ip config
bool validateConfig() {
  if (!staticIP || !gateway || !subnet || !dns) {
    Serial.println("IP-instellingen zijn ongeldig.");
    return false;
  }
#ifdef DEBUG  
  Debugf("\n\nFixed IP values: ip[%s] gw[%s] sub[%s] dns[%s]\n\n",staticIP.toString(), gateway.toString(), subnet.toString(), dns.toString());
#endif

  return true;
}

//===========================================================================================
void startNetwork()
{
  Network.onEvent(onNetworkEvent);
  // Network.enableIPv6();
  if ( loadFixedIPConfig("/fixedip.json") ) bFixedIP = validateConfig();
  startETH();
  startWiFi(settingHostname, 240);  // timeout 4 minuten
  WaitOnNetwork();
  GetMacAddress();
  snprintf( MQTopTopic, sizeof(MQTopTopic), "%s%s%s", settingMQTTtopTopic, MacIDinToptopic?macID:"",MacIDinToptopic?"/":"" ); //set proper MQTTtoptopic
  USBPrint("Ip-addr: ");USBPrintln(IP_Address());
}

//===========================================================================================
void startTelnet() 
{
  if ( skipNetwork ) return;
  TelnetStream.begin();
  ws_raw.begin();
  TelnetStream.flush();
  DebugTln(F("Telnet server started .."));
} // startTelnet()

//=======================================================================
void startMDNS(const char *Hostname) {
  if ( skipNetwork ) return;
  DebugTf("[1] mDNS setup as [%s.local]\r\n", Hostname);
  if ( !MDNS.begin(Hostname) ) DebugTln(F("[3] Error setting up MDNS responder!\r\n"));
  else {
    MDNS.addService( Hostname, "tcp", 80);
    MDNS.addService( "p1dongle", "tcp", 80);
    MDNS.addServiceTxt("p1dongle", "tcp", "id", String(macID).c_str() );
#ifdef ULTRA    
    MDNS.addServiceTxt("p1dongle", "tcp", "hw", "P1U" );    
#elif defined (ETHERNET)
    MDNS.addServiceTxt("p1dongle", "tcp", "hw", "P1E" );    
#else
    MDNS.addServiceTxt("p1dongle", "tcp", "hw", "P1P" );    
#endif
  }
} // startMDNS()

#endif

#ifdef ETHERNET
  uint8_t cs = CS_GPIO;
  uint8_t miso  = MISO_GPIO;
  uint8_t mosi  = MOSI_GPIO;
  uint8_t sck   = SCK_GPIO;
  
void startETH(){
  uint8_t _int  = INT_GPIO;

  uint8_t mac_eth[6];
  esp_read_mac(mac_eth, ESP_MAC_ETH); //derive ETH mac address

  if ( HardwareType == P1UX2 ) {
    cs    = 13;
    miso  = 15;
    mosi  = 16;
    sck   = 14;
    _int  = 18;
    }

  myEthernet.myBeginSPI(ETH, ETH_TYPE, ETH_ADDR, mac_eth, cs, _int, ETH_RST, NULL, sck, miso, mosi, SPI2_HOST, ETH_PHY_SPI_FREQ_MHZ );

  if ( bFixedIP ) ETH.config(staticIP, gateway, subnet, dns);

}

void W5500_Read_PHYCFGR() {
  digitalWrite(cs, LOW);

  SPI.transfer16(0x002E);     // MSB, LSB
  SPI.transfer(0x01);       // control: BSB=0 (common), OM=VDM, RWB=1(read)
  uint8_t phycfgr = SPI.transfer(0x00);
  
  digitalWrite(cs, HIGH);

#ifdef DEBUG
    // Print registerwaarde en status
    Debug("PHYCFGR Register: 0x");
    Debugln(phycfgr, HEX);

    Debug("PHY Status - ");
    Debug((phycfgr & 0x40) ? "Software control, " : "Auto mode, ");
    Debug((phycfgr & 0x4) ? "Full Duplex, " : "Half Duplex, ");
    Debug((phycfgr & 0x2) ? "100 Mbps, " : "10 Mbps, ");
    Debugln((phycfgr & 0x1) ? "Link UP, " : "Link DOWN, ");
#endif

}

void enterPowerDownMode() {
  // DebugTln(F("ETH POWER DOWN"));
  LogFile("ETH powered down, switching to WiFi", true);
  ETH.end();
  
  //powerdown W5500 when RST is connected to the ESP32
  if ( HardwareType == P1UX2 && HardwareVersion >= 101 ) { digitalWrite(17,LOW); return;}

  pinMode(cs, OUTPUT);
  SPI.begin(sck, miso, mosi);
  
  W5500_Read_PHYCFGR();

  // NOTE:
  // We gebruiken control byte 0x04 (FDM write mode) i.p.v. 0x00 (VDM write).
  // Op dit board reageert de W5500 na ETH.end() niet op 0x00,
  // maar 0x04 + 0x70 zet 'm wél betrouwbaar in low-power.
  // Niet veranderen naar 0x00 tenzij je TEST dat powerdown nog steeds werkt.
  SPI.beginTransaction(SPISettings(8000000UL, MSBFIRST, SPI_MODE0));
  digitalWrite(cs, LOW);
  SPI.transfer16(0x002E);
  SPI.transfer(0x04);               // Control byte (W5500 write)
  SPI.transfer(0x70);              // Data to write
  digitalWrite(cs, HIGH);
  SPI.endTransaction();

  delay(500);
  W5500_Read_PHYCFGR();
}

#else
  void startETH(){}
  void enterPowerDownMode(){}
#endif //def ETHERNET

String IP_Address(){

#ifdef ETHERNET
  if ( netw_state == NW_ETH ) return ETH.localIP().toString();
  else return WiFi.localIP().toString();
#else
  return WiFi.localIP().toString();
#endif
}

String MAC_Address(){
#ifdef ETHERNET
  if ( netw_state == NW_ETH ) return ETH.macAddress();
  else return WiFi.macAddress();
#else
  return WiFi.macAddress();
#endif
}