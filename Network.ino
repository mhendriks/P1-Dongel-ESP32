#ifndef _NETWORK_H
#define _NETWORK_H

#include "DSMRloggerAPI.h"
#include "esp_timer.h"
#include "esp_mac.h"

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
#endif

bool bNoNetworkConn = false;
bool bEthUsage = false;
static uint32_t s_wifiLostMs = 0;
static uint32_t s_wifiRetryMs = 0;
static uint8_t  s_wifiReconnectCnt = 0;
static const uint32_t WIFI_WATCHDOG_LOST_MS = 20000;
#ifdef DEBUG
  static const uint32_t WIFI_WATCHDOG_REBOOT_MS = 30*1000; // 30 sec
#else
  static const uint32_t WIFI_WATCHDOG_REBOOT_MS = 5*60*1000; // 5 minuten
#endif

static inline bool markWifiDisconnected(uint32_t nowMs);


static void attemptWifiReconnect() {
  WiFi.disconnect();
  delay(100);
  WiFi.reconnect();
}

// Event handlers only signal loss. Reconnect/retry decisions happen only in WifiWatchDog().
static inline void signalWifiLoss(const String& reason) {
  if ( !bNoNetworkConn ) LogFile(reason.c_str(), true); // only once
  markWifiDisconnected(millis());
}

void WifiWatchDog() {
  if ( skipNetwork ) return;
  if ( bEthUsage || netw_state == NW_ETH || netw_state == NW_ETH_LINK ) return;
  if ( WiFi.status() == WL_CONNECTED ) return;

  uint32_t now = millis();

  // Event can arrive late; make sure downtime detection starts even if event state is not yet updated.
  if ( !bNoNetworkConn || (s_wifiLostMs == 0) ) {
    s_wifiLostMs = now;
    s_wifiRetryMs = now;
    s_wifiReconnectCnt = 0;
    bNoNetworkConn = true;
    DebugT("WifiLost -> watchdog immediate reconnect attempt: ");
    Debugln(s_wifiReconnectCnt + 1);
    sprintf(cMsg, "Wifi reconnect attempt %d", s_wifiReconnectCnt + 1);
    LogFile(cMsg, true);
    attemptWifiReconnect();
    s_wifiReconnectCnt++;
    s_wifiRetryMs = now;
    return;
  }
  
  if ( (uint32_t)(now - s_wifiLostMs) >= WIFI_WATCHDOG_REBOOT_MS ) {
    LogFile("Wifi no reconnect possible -> reboot", true);
    P1Reboot();
    return;
  }

  if ( (uint32_t)(now - s_wifiRetryMs) >= WIFI_WATCHDOG_LOST_MS ) {
    DebugT("WifiLost > ");
    Debug(WIFI_WATCHDOG_LOST_MS);
    Debugln(" ms, reconnect");
    Debug("WifiReconnect attempt: ");
    Debugln(s_wifiReconnectCnt + 1);
    sprintf(cMsg, "Wifi reconnect attempt %d", s_wifiReconnectCnt + 1);
    LogFile(cMsg, true);
    s_wifiReconnectCnt++;
    s_wifiRetryMs = now;
    attemptWifiReconnect();
  }
}

static inline bool markWifiDisconnected(uint32_t nowMs) {
  if ( netw_state != NW_ETH ) netw_state = NW_NONE;
  if ( !bNoNetworkConn ) {
    s_wifiLostMs = 0;
    s_wifiRetryMs = nowMs;
    s_wifiReconnectCnt = 0;
    bNoNetworkConn = true;
    return true;
  }

  return false;
}

static bool isZeroMac(const uint8_t *mac){
  for (uint8_t i = 0; i < 6; i++) {
    if (mac[i] != 0x00) return false;
  }
  return true;
}

#ifdef ETHERNET
static bool readPreferredEthMac(uint8_t *mac){
  memset(mac, 0, 6);
  if (esp_read_mac(mac, ESP_MAC_ETH) == ESP_OK && !isZeroMac(mac)) return true;

  if (esp_read_mac(mac, ESP_MAC_EFUSE_FACTORY) == ESP_OK && !isZeroMac(mac)) {
    DebugTln("ETH MAC fallback: using factory eFuse MAC");
    return true;
  }

  return false;
}
#endif

WiFiManager manageWiFi;

void GetMacAddress(){

  uint8_t efuseMac[6] = {0};
  char macAddressString[18];

#ifdef ETHERNET 
  readPreferredEthMac(efuseMac);
#else
  esp_read_mac( efuseMac, ESP_MAC_EFUSE_FACTORY );
#endif
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", efuseMac[0], efuseMac[1], efuseMac[2], efuseMac[3], efuseMac[4], efuseMac[5]);
  sprintf(macID, "%02X%02X%02X%02X%02X%02X", efuseMac[0], efuseMac[1], efuseMac[2], efuseMac[3], efuseMac[4], efuseMac[5]);
  // return String(macAddressString);


  // strcpy( macStr, _mac.c_str() );
  // _mac.replace( ":","" );
  // strcpy( macID, _mac.c_str() );
  USBPrint( "MacStr   : " ); USBPrintln( macStr ); //only at setup

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
  if ( skipNetwork ) return;
  HTTPClient http;
  http.begin(wifiClient, APIURL);
  http.setConnectTimeout(4000);
  http.setTimeout(5000);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
  String httpRequestData = "mac=" + String(macStr) + "&ip=" + IP_Address() + "&version=" + _VERSION_ONLY+ "&hw=" + HardwareType;
  
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
  char hostname[sizeof(settingHostname)];
  snprintf(hostname, sizeof(hostname), "%s-%s", settingHostname, DongleID);
  strlcpy(settingHostname, hostname, sizeof(settingHostname));
}

void WifiOff() {
#ifndef ESPNOW 
  if ( WiFi.isConnected() ) WiFi.disconnect(true,true);
  WiFi.mode(WIFI_OFF);
  esp_wifi_stop();
  // esp_wifi_deinit();
  WiFi.setSleep(true);
#endif
#ifdef CONFIG_BT_ENABLED
  btStop();
#endif
}

// Network event -> Ethernet is dominant
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
    case ARDUINO_EVENT_ETH_STOP: //2
      DebugTln("!!! ETH Stopped");
      [[fallthrough]];
    case ARDUINO_EVENT_ETH_DISCONNECTED: //4
      if ( netw_state != NW_WIFI ) netw_state = NW_NONE;
      SwitchLED( LED_ON , LED_RED );
      LogFile("ETH Disconnected", true);
      break;
#endif //ETHERNET
    
    //****** WIFI ******//

    case ARDUINO_EVENT_WIFI_STA_START: //110
      // initial nudge (soms is begin() traag met eerste connect)
      // Debugln("--- WIFI START ----");
      break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED: //4
        sprintf(cMsg,"Connected to %s. Asking for IP address", WiFi.BSSIDstr().c_str());
        LogFile(cMsg, true);
        // s_wifiKnownSsid = WiFi.SSID();
        // s_wifiKnownPsk = WiFi.psk();
        break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP: //7
        LogFile("Wifi Connected",true);
        SwitchLED( LED_ON, LED_BLUE );
        Debug (F("IP address: " ));  Debug (WiFi.localIP());Debug(" )\n\n");
        // s_wifiKnownSsid = WiFi.SSID();
        // s_wifiKnownPsk = WiFi.psk();

        if ( bEthUsage ) WifiOff(); else netw_state = NW_WIFI;
        s_wifiLostMs = 0;
        s_wifiRetryMs = 0;
        s_wifiReconnectCnt = 0;
        bNoNetworkConn = false;
        break;
    case ARDUINO_EVENT_WIFI_STA_STOP: //8
      // Voor de zekerheid: driver soms stopt STA → start ’m weer
      // LogFile("Wifi STA stopped-> restart",true);
      // esp_wifi_start(); // idempotent als al gestart
      break;
    case ARDUINO_EVENT_WIFI_STA_LOST_IP: //117
        signalWifiLoss("Wifi connection lost - LOST IP");
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
        signalWifiLoss(discon_res);
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
    //lets wait on ethernet first for 6.5sec and consume some time to charge the capacitors
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
  
  //switch to lowpower wifi mode
  esp_wifi_set_ps(WIFI_PS_MAX_MODEM); //lower calibration power
  esp_wifi_set_max_tx_power(8);   // 8 = ±8.5 dBm
  
  WiFi.setSleep(true);  //sleep when possible

  #ifdef CONFIG_BT_ENABLED
    btStop();
  #endif
  WiFi.setAutoReconnect(true);

  WiFi.setHostname(hostname);
  WiFi.setMinSecurity(WIFI_AUTH_WPA_PSK);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);  //to solve mesh issues 
  // WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);      //to solve mesh issues 
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
  manageWiFi.setClass("invert"); //dark theme
  
  // manageWiFi.setMinimumSignalQuality(-1);
  // manageWiFi.setScanDispPerc(false);

  // const char* wifiMenu[] = {
  //   "wifinoscan",
  //   "info",
  //   "sep",
  //   "restart",
  //   "exit"
  // };
  // manageWiFi.setMenu(wifiMenu, 5);
  
  //--- set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  manageWiFi.setAPCallback(configModeCallback);
  manageWiFi.setConfigPortalTimeout(timeOut); //config portal timeout in seconds
  
  esp_task_wdt_reset();
  allowSkipNetworkByButton = true;
  manageWiFi.autoConnect(_HOTSPOT);

  //handle wifi webinterface timeout and connection (timeOut in sec) timeout in 30 sec
  uint16_t i = 0;
  while ( (i++ < 3000) && (netw_state == NW_NONE) && !bEthUsage && !skipNetwork ) {
    Debug("*");
    delay(100);
    manageWiFi.process();
    esp_task_wdt_reset();
    SwitchLED(i%4?LED_ON:LED_OFF,LED_BLUE); //fast blinking
  }
  Debugln();
  allowSkipNetworkByButton = false;
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
  
  //switch to normale wifi mode
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_max_tx_power(78);  // max (~20.5 dBm)
  
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
  SetupButton();
  startWiFi(settingHostname, 240);  // timeout 4 minuten
  WaitOnNetwork();
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
  String mdnsName = Hostname;
#ifdef MIMIC_HW
  String MACID = macID;
  String mimicHostname = "p1meter-" + MACID.substring(MACID.length() - 6);
  mdnsName = mimicHostname;
  DebugTf("[2] mDNS mimic as [%s.local]\r\n", mimicHostname.c_str());
#endif

  if ( !MDNS.begin(mdnsName.c_str()) ) {
    DebugTln(F("[3] Error setting up MDNS responder!\r\n"));
    return;
  }

#ifdef MIMIC_HW
  MDNS.addService("hwenergy", "tcp", 80);
  MDNS.addService("homewizard", "tcp", 80);

  MDNS.addServiceTxt("hwenergy", "tcp", "serial", MACID.c_str());
  MDNS.addServiceTxt("hwenergy", "tcp", "product_type", "HWE-P1");
  MDNS.addServiceTxt("hwenergy", "tcp", "product_name", "P1 Meter");
  MDNS.addServiceTxt("hwenergy", "tcp", "path", "/api/v1");
  MDNS.addServiceTxt("hwenergy", "tcp", "api_enabled", "1");

  MDNS.addServiceTxt("homewizard", "tcp", "serial", MACID.c_str());
  MDNS.addServiceTxt("homewizard", "tcp", "product_type", "HWE-P1");
  MDNS.addServiceTxt("homewizard", "tcp", "product_name", "P1 Meter");
  MDNS.addServiceTxt("homewizard", "tcp", "path", "/api/v1");
  MDNS.addServiceTxt("homewizard", "tcp", "api_enabled", "1");
#else
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
#endif
} // startMDNS()

#endif

#ifdef ETHERNET
  int8_t cs, miso, mosi, sck, rst;
  
void startETH(){
  uint8_t mac_eth[6] = {0};
  readPreferredEthMac(mac_eth);
  
  // if ( HardwareType == P1UX2 ) { 
    const dev_conf& dc = DEVCONF();
    cs    = dc.eth_cs;
    miso  = dc.eth_miso;
    mosi  = dc.eth_mosi;
    sck   = dc.eth_sck;
    uint8_t _int  = dc.eth_int;
    rst   = dc.eth_rst;
    // }

  #ifdef DEBUG
    DebugTf("ETH pins: cs=%d int=%d rst=%d sck=%d miso=%d mosi=%d\n",cs, _int, rst, sck, miso, mosi);
  #endif
  
  //ETH_RST not via the driver otherwise it couldnt be reset manually
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
    // Print register value and status
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
  LogFile("ETH powered down, switching to WiFi", true);
  ETH.end();
  
  //powerdown W5500 when RST is connected to the ESP32
  if ( HardwareType == P1UX2 && HardwareVersion >= 101 ) { 
    digitalWrite(rst,LOW); 
    DebugTln("P1UX2: W5500 RESET LOW");
    return;
  }

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
