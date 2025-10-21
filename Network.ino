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
#include "esp_mac.h"

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
  esp_wifi_start();                 // idempotent; start STA als die gestopt is
  esp_wifi_connect();               // als driver al bezig is → ESP_ERR_WIFI_CONN
  s_lastKickMs = now;
}

static void scheduleReconnect(uint32_t delayMs); // fwd

void GetMacAddress(){

  String _mac = MAC_Address();
  strcpy( macStr, _mac.c_str() );
  _mac.replace( ":","" );
  strcpy( macID, _mac.c_str() );
  USBPrint( "MacStr   : " );USBPrintln( macStr ); //only at setup

  strncpy(DongleID, macID + 6, 6);
  DongleID[6] = '\0';
  Debug( "DongleID : " );Debugln( DongleID );
//  USBSerial.print( "MacID: " );USBSerial.println( macID );
}

/***===========================================================================================
    POST MAC + IP
    https://www.allphptricks.com/create-and-consume-simple-rest-api-in-php/
    http://g2pc1.bu.edu/~qzpeng/manual/MySQL%20Commands.htm
**/
void PostMacIP() {
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

void setHostname() {
  //only once at startup when settingsfile isnt available
  // char hostname[32]; // Max lengte hostname is 32 karakters
  // sprintf(settingHostname, "%s-%0X", settingHostname, DongleID);
  strcat(settingHostname,"-");
  strcat(settingHostname,DongleID);
}

void WifiOff() {
#ifndef ESPNOW 
  if ( WiFi.isConnected() ) WiFi.disconnect(true,true);
  WiFi.mode(WIFI_OFF);
  // esp_wifi_stop();
  // esp_wifi_deinit();
  WiFi.setSleep(true);
#endif
  btStop();
}

static inline bool isAuthError(uint8_t reason)
{
  // Enum is wifi_err_reason_t (esp_wifi_types.h). Niet alle borden hebben dezelfde set,
  // dus houd 'm iets breder:
  switch (reason) {
    case WIFI_REASON_AUTH_EXPIRE:             // 2
    case WIFI_REASON_AUTH_FAIL:               // (ESP-IDF nieuwe naam, bij sommige cores is dit 202)
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:  // 15
    case WIFI_REASON_HANDSHAKE_TIMEOUT:       // alias op sommige targets
    case WIFI_REASON_NOT_AUTHED:              // 6
    case WIFI_REASON_ASSOC_FAIL:              // 18 (kan ook auth-gerelateerd zijn)
      return true;
    default:
      return false;
  }
}

//int WifiDisconnect = 0;
// bool bNoNetworkConn = false;
// bool bEthUsage = false;

// Network event -> Ethernet is dominant
// https://github.com/espressif/arduino-esp32/blob/master/libraries/Network/src/NetworkEvents.h
static void onNetworkEvent (WiFiEvent_t event, arduino_event_info_t info) {
  switch (event) {
#ifdef ETHERNET  
  //ETH    
    case ARDUINO_EVENT_ETH_START: //1
      DebugTln("ETH Started");
      // ETH.setHostname(_HOSTNAME); SDK 3.0 feature
      break;
    case ARDUINO_EVENT_ETH_CONNECTED: //3
      DebugTln("\nETH Connected");
      WifiOff();
      // netw_state = NW_ETH;
      bEthUsage = true; //set only once 
      break;
    case ARDUINO_EVENT_ETH_GOT_IP6: //7
      LogFile("ETH GOT IP V6", true);
      break;
    case ARDUINO_EVENT_ETH_GOT_IP: //5
      {
        netw_state = NW_ETH;
        WifiOff();
        LogFile("ETH GOT IP", true);
        bEthUsage = true; //set only once 
        SwitchLED( LED_ON, LED_BLUE ); //Ethernet available = RGB LED Blue
        // tLastConnect = 0;
        bNoNetworkConn = false;
        break;
      } 
    case ARDUINO_EVENT_ETH_STOP: //2
      DebugTln("!!! ETH Stopped");
    case ARDUINO_EVENT_ETH_DISCONNECTED: //4
      // if ( Update.isRunning() ) Update.abort();
      if ( netw_state != NW_WIFI ) netw_state = NW_NONE;
      SwitchLED( LED_ON , LED_RED );
      LogFile("ETH Disconnected", true);
      bNoNetworkConn = true;
      break;
#endif //ETHERNET
//WIFI
    case ARDUINO_EVENT_WIFI_STA_START: //110
      // initial nudge (soms is begin() traag met eerste connect)
      if ( !manageWiFi.getConfigPortalActive() ) kickOnce();
      break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED: //4
        sprintf(cMsg,"Connected to %s. Asking for IP address", WiFi.BSSIDstr().c_str());
        LogFile(cMsg, true);
      //  tWifiLost = 0;
        break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP: //7
        LogFile("Wifi Connected",true);
        SwitchLED( LED_ON, LED_BLUE );
        Debug (F("IP address: " ));  Debug (WiFi.localIP());Debug(" )\n\n");

        if ( bEthUsage ) WifiOff();        
        else {
          enterPowerDownMode(); //disable ETH
          netw_state = NW_WIFI;
        }
        s_authBlocked = false;
        s_lastKickMs = 0;               // reset voor snelle roam
        bNoNetworkConn = false;
        break;
    case ARDUINO_EVENT_WIFI_STA_STOP: //8
      // Voor de zekerheid: driver soms stopt STA → start ’m weer
      LogFile("Wifi STA stopped-> restart",true);
      esp_wifi_start(); // idempotent als al gestart
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
        return;                       // niets forceren
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
    default:
        sprintf(cMsg,"Network-event : %d | reason: %d| rssi: %d | channel : %i",event, info.wifi_sta_disconnected.reason, WiFi.RSSI(), WiFi.channel());
        LogFile(cMsg, true);
        break;
    }
}

//gets called when WiFiManager enters configuration mode
//===========================================================================================
void configModeCallback (WiFiManager *myWiFiManager) 
{
  DebugTln(F("Entered config mode\r"));
  DebugTln(WiFi.softAPIP().toString());
  DebugTln(myWiFiManager->getConfigPortalSSID());
  esp_task_wdt_reset();
} // configModeCallback()

//===========================================================================================
#include <esp_wifi.h>

void startWiFi(const char* hostname, int timeOut) {  

#if not defined ETHERNET || defined ULTRA
#ifdef ULTRA
  //lets wait on ethernet first for 3.5sec and consume some time to charge the capacitors
  uint8_t timeout = 0;
  while ( (netw_state == NW_NONE) && (timeout++ < 35) ) {
    delay(100); 
    Debug(".");
    esp_task_wdt_reset();
  } 
  Debugln();
  // w5500_powerDown();
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
  while ( (i++ < 3000) && (netw_state == NW_NONE) && !bEthUsage ) {
    Debug("*");
    delay(100);
    manageWiFi.process();
    esp_task_wdt_reset();
    SwitchLED(i%4?LED_ON:LED_OFF,LED_BLUE); //fast blinking
  }
  Debugln();
  manageWiFi.stopWebPortal();

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
  while ( netw_state == NW_NONE ) { //endless wait for network connection
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
  USBPrint("Ip-addr: ");USBPrintln(IP_Address());
}

//===========================================================================================
void startTelnet() 
{
  TelnetStream.begin();
  ws_raw.begin();
  TelnetStream.flush();
  DebugTln(F("Telnet server started .."));
} // startTelnet()


//=======================================================================
void startMDNS(const char *Hostname) 
{
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

#include <ETH.h>
#include <SPI.h>


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

void startETH(){
  
  //derive ETH mac address
  uint8_t mac_eth[6] = { 0xFE, 0xED, 0xDE, 0xAD, 0xBE, 0xEF };
  esp_read_mac(mac_eth, ESP_MAC_ETH);
  
  // ETH.enableIPv6();
  myEthernet.myBeginSPI(ETH, ETH_TYPE, ETH_ADDR, mac_eth, CS_GPIO, INT_GPIO, ETH_RST, NULL, SCK_GPIO, MISO_GPIO, MOSI_GPIO, SPI2_HOST, ETH_PHY_SPI_FREQ_MHZ );
  if ( bFixedIP ) ETH.config(staticIP, gateway, subnet, dns);

}

void W5500_Read_PHYCFGR() {
    digitalWrite(CS_GPIO, LOW);

    // Stuur het PHYCFGR-adres met het "read" bit (bit 15 = 1)
    SPI.transfer16(0x802E); // 0x802E = 0x002E met "read" bit ingesteld
    uint8_t phycfgr = SPI.transfer(0x00); // Ontvang data

    digitalWrite(CS_GPIO, HIGH);

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
  // Select the W5500 chip
  W5500_Read_PHYCFGR();

  digitalWrite(CS_GPIO, LOW);
  SPI.transfer((0x002E >> 8) & 0x7F); // Upper address byte
  SPI.transfer(0x002E & 0xFF);        // Lower address byte
  SPI.transfer(0x04);               // Control byte (W5500 write)
  SPI.transfer(0x70);              // Data to write
  digitalWrite(CS_GPIO, HIGH);
  
  // ETH.end(); //controler stopped and this will throw an error
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
  uint8_t efuseMac[6];
  char macAddressString[13];

  // esp_read_mac( efuseMac, ESP_MAC_BASE );
  // sprintf(macAddressString, "%02X%02X%02X%02X%02X%02X", efuseMac[0], efuseMac[1], efuseMac[2], efuseMac[3], efuseMac[4], efuseMac[5]);
  // Debug("ESP_MAC_BASE: ");Debugln(macAddressString);

  // esp_read_mac( efuseMac, ESP_MAC_ETH );
  // sprintf(macAddressString, "%02X%02X%02X%02X%02X%02X", efuseMac[0], efuseMac[1], efuseMac[2], efuseMac[3], efuseMac[4], efuseMac[5]);
  // Debug("ESP_MAC_ETH: ");Debugln(macAddressString);

#ifdef ETHERNET 
  esp_read_mac( efuseMac, ESP_MAC_ETH );
#else
  esp_read_mac( efuseMac, ESP_MAC_BASE );
#endif
  sprintf(macAddressString, "%02X%02X%02X%02X%02X%02X", efuseMac[0], efuseMac[1], efuseMac[2], efuseMac[3], efuseMac[4], efuseMac[5]);
  return String(macAddressString);
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
