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

  // Use the ESP_MAC_ETH address instead of the local MAC address.
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
  static const uint32_t WIFI_WATCHDOG_REBOOT_MS = 5*60*1000; // 5 minutes
#endif

static inline bool markWifiDisconnected(uint32_t nowMs);

static inline bool ethernetActive() {
  return bEthUsage || netw_state == NW_ETH || netw_state == NW_ETH_LINK;
}

static void resetWifiWatchdogState() {
  s_wifiLostMs = 0;
  s_wifiRetryMs = 0;
  s_wifiReconnectCnt = 0;
  bNoNetworkConn = false;
}

static void attemptWifiReconnect() {
  WiFi.disconnect();
  delay(100);
  WiFi.reconnect();
}

// Event handlers only signal loss. Reconnect/retry decisions happen only in WifiWatchDog().
static inline void signalWifiLoss(const String& reason) {
  if ( !bNoNetworkConn ) LogFile(reason.c_str(), true);
  markWifiDisconnected(millis());
}

void WifiWatchDog() {
  if ( skipNetwork ) return;
  if ( ethernetActive() ) return;
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

#ifdef ETHERNET 
  readPreferredEthMac(efuseMac);
#else
  esp_read_mac( efuseMac, ESP_MAC_EFUSE_FACTORY );
#endif
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", efuseMac[0], efuseMac[1], efuseMac[2], efuseMac[3], efuseMac[4], efuseMac[5]);
  sprintf(macID, "%02X%02X%02X%02X%02X%02X", efuseMac[0], efuseMac[1], efuseMac[2], efuseMac[3], efuseMac[4], efuseMac[5]);
  USBPrint( "MacStr   : " ); USBPrintln( macStr );

  strncpy(DongleID, macID + 6, 6);
  DongleID[6] = '\0';
  Debug( "DongleID : " );Debugln( DongleID );
}

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

void WifiOff() {
#ifndef ESPNOW 
  if ( WiFi.isConnected() ) WiFi.disconnect(true,true);
  WiFi.mode(WIFI_OFF);
  esp_wifi_stop();
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
    case ARDUINO_EVENT_ETH_START: //1
      DebugTln("ETH Started");
      ETH.setHostname(settingHostname);
      break;
    case ARDUINO_EVENT_ETH_CONNECTED: //3
      DebugTln("\nETH Connected");
      WifiOff();
      netw_state = NW_ETH_LINK;
      bEthUsage = true;
      break;
    case ARDUINO_EVENT_ETH_GOT_IP6: //7
      LogFile("ETH GOT IP V6", true);
      [[fallthrough]];
    case ARDUINO_EVENT_ETH_GOT_IP: //5
      {
        netw_state = NW_ETH;
        WifiOff();
        LogFile("ETH GOT IP", true);
        bEthUsage = true;
        SwitchLED( LED_ON, LED_BLUE );
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
#endif // ETHERNET
    
    case ARDUINO_EVENT_WIFI_STA_START: //110
      break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED: //4
        sprintf(cMsg,"Connected to %s. Asking for IP address", WiFi.BSSIDstr().c_str());
        LogFile(cMsg, true);
        break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP: //7
        LogFile("Wifi Connected",true);
        SwitchLED( LED_ON, LED_BLUE );
        Debug (F("IP address: " ));  Debug (WiFi.localIP());Debug(" )\n\n");

        if ( bEthUsage ) WifiOff(); else netw_state = NW_WIFI;
        resetWifiWatchdogState();
        break;
    case ARDUINO_EVENT_WIFI_STA_STOP: //8
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

// Called when WiFiManager enters configuration mode.
void configModeCallback (WiFiManager *myWiFiManager) {
  DebugTln(F("Wifi Connection Failed -> Entered config mode\r"));
  DebugTln(WiFi.softAPIP().toString());
  DebugTln(myWiFiManager->getConfigPortalSSID());
  esp_task_wdt_reset();
}

#include <esp_wifi.h>

void startWiFi(const char* hostname, int timeOut) {  
  if ( skipNetwork ) return;
#if not defined ETHERNET || defined ULTRA
  #ifdef ULTRA
    // Let Ethernet get the first chance on Ultra hardware.
    uint8_t timeout = 0;
    while ( (netw_state == NW_NONE) && (timeout++ < 65) ) {
      delay(100); 
      Debug("e");
      esp_task_wdt_reset();
    } 
    Debugln();
    if ( netw_state != NW_NONE ) return;
    enterPowerDownMode();
  #endif 
  
  if ( netw_state != NW_NONE ) return;
  
  esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
  esp_wifi_set_max_tx_power(8);   // 8 = ±8.5 dBm
  
  WiFi.setSleep(true);

  #ifdef CONFIG_BT_ENABLED
    btStop();
  #endif
  WiFi.setAutoReconnect(true);

  WiFi.setHostname(settingHostname);
  WiFi.setMinSecurity(WIFI_AUTH_WPA_PSK);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  if ( bFixedIP ) WiFi.config(staticIP, gateway, subnet, dns);
  LogFile("Wifi Starting",true);
  SwitchLED( LED_OFF, LED_BLUE );  
  
  manageWiFi.setConnectTimeout(15);
  manageWiFi.setConfigPortalBlocking(false);
  manageWiFi.setDebugOutput(false);
  manageWiFi.setShowStaticFields(true);
  manageWiFi.setShowDnsFields(true);
  manageWiFi.setRemoveDuplicateAPs(false);
  manageWiFi.setScanDispPerc(true);
  manageWiFi.setClass("invert");
  
  manageWiFi.setAPCallback(configModeCallback);
  manageWiFi.setConfigPortalTimeout(timeOut);
  
  esp_task_wdt_reset();
  allowSkipNetworkByButton = true;
  manageWiFi.autoConnect(settingHostname);

  uint16_t i = 0;
  while ( (i++ < 3000) && (netw_state == NW_NONE) && !bEthUsage && !skipNetwork ) {
    Debug("*");
    delay(100);
    manageWiFi.process();
    esp_task_wdt_reset();
    SwitchLED(i%4?LED_ON:LED_OFF,LED_BLUE);
  }
  Debugln();
  allowSkipNetworkByButton = false;
  manageWiFi.stopWebPortal();
  if ( skipNetwork ) return; 
  if ( netw_state == NW_NONE && !bEthUsage ) {
    LogFile("reboot: Wifi failed to connect and hit timeout",true);
    P1Reboot();
  }
  if ( netw_state == NW_ETH || bEthUsage ) {
    WifiOff();
    delay(500);
  }
  
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_max_tx_power(78);  // max (~20.5 dBm)
  
  SwitchLED( LED_ON, LED_BLUE );
#else 
  Debugln(F("NO WIFI SUPPORT"));
#endif // !ETHERNET
}

void WaitOnNetwork()
{
  if ( skipNetwork ) { 
    enterPowerDownMode();
    WifiOff();
    EnableHistory = false;
    writeSettings();
    SwitchLED(LED_OFF,LED_BLUE);
    return; 
    }
  while ( netw_state != NW_ETH && netw_state != NW_WIFI ) {
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

void startNetwork()
{
  Network.onEvent(onNetworkEvent);
  if ( loadFixedIPConfig("/fixedip.json") ) bFixedIP = validateConfig();
  startETH();
  SetupButton();
  startWiFi(settingHostname, 240);
  WaitOnNetwork();
  USBPrint("Ip-addr: ");USBPrintln(IP_Address());
}

void startTelnet() 
{
  if ( skipNetwork ) return;
  TelnetStream.begin();
  ws_raw.begin();
  TelnetStream.flush();
  DebugTln(F("Telnet server started .."));
}

void startMDNS(const char *Hostname) {
  if ( skipNetwork ) return;
  
  DebugTf("[1] mDNS setup as [%s.local]\r\n", Hostname);
  String lowerMACID = macID;
  lowerMACID.toLowerCase();

  if ( !MDNS.begin(Hostname) ) {
    DebugTln(F("[3] Error setting up MDNS responder!\r\n"));
    return;
  }

  if (isHWMimicSelected()) {
    MDNS.addService("hwenergy", "tcp", 80);
    MDNS.addService("homewizard", "tcp", 80);

    MDNS.addServiceTxt("hwenergy", "tcp", "serial", lowerMACID);
    MDNS.addServiceTxt("hwenergy", "tcp", "product_type", "HWE-P1");
    MDNS.addServiceTxt("hwenergy", "tcp", "product_name", "P1 Meter");
    MDNS.addServiceTxt("hwenergy", "tcp", "path", "/api/v1");
    MDNS.addServiceTxt("hwenergy", "tcp", "api_enabled", "1");

    MDNS.addServiceTxt("homewizard", "tcp", "serial", lowerMACID);
    MDNS.addServiceTxt("homewizard", "tcp", "product_type", "HWE-P1");
    MDNS.addServiceTxt("homewizard", "tcp", "product_name", "P1 Meter");
    MDNS.addServiceTxt("homewizard", "tcp", "path", "/api/v1");
    MDNS.addServiceTxt("homewizard", "tcp", "api_enabled", "1");
  }

  if (isShellyPro3EmMimicSelected()) {
    String shellyInstance = "shellypro3em-";
    shellyInstance += lowerMACID;

    MDNS.addService("shelly", "tcp", 80);
    mdns_service_instance_name_set("_shelly", "_tcp", shellyInstance.c_str());
    MDNS.addServiceTxt("shelly", "tcp", "gen", "2");
    MDNS.addServiceTxt("shelly", "tcp", "app", "Pro3EM");
    MDNS.addServiceTxt("shelly", "tcp", "ver", "1.4.2");

    MDNS.addService("http", "tcp", 80);
    mdns_service_instance_name_set("_http", "_tcp", shellyInstance.c_str());
    MDNS.addServiceTxt("http", "tcp", "gen", "2");
  }

  MDNS.addService( Hostname, "tcp", 80);
  MDNS.addService( "p1dongle", "tcp", 80);
  MDNS.addServiceTxt("p1dongle", "tcp", "id", lowerMACID );
  MDNS.addServiceTxt("p1dongle", "tcp", "hw", HWTypeNames[HardwareType] );    
}

#endif

#ifdef ETHERNET
  int8_t cs, miso, mosi, sck, rst;
  
void startETH(){
  uint8_t mac_eth[6] = {0};
  readPreferredEthMac(mac_eth);
  
    const dev_conf& dc = DEVCONF();
    cs    = dc.eth_cs;
    miso  = dc.eth_miso;
    mosi  = dc.eth_mosi;
    sck   = dc.eth_sck;
    uint8_t _int  = dc.eth_int;
    rst   = dc.eth_rst;

  #ifdef DEBUG
    DebugTf("ETH pins: cs=%d int=%d rst=%d sck=%d miso=%d mosi=%d\n",cs, _int, rst, sck, miso, mosi);
  #endif
  
  // Keep ETH_RST outside the driver so it can still be reset manually.
  myEthernet.myBeginSPI(ETH, ETH_TYPE, ETH_ADDR, mac_eth, cs, _int, ETH_RST, NULL, sck, miso, mosi, SPI2_HOST, ETH_PHY_SPI_FREQ_MHZ );

  if ( bFixedIP ) ETH.config(staticIP, gateway, subnet, dns);

}

void W5500_Read_PHYCFGR() {
  digitalWrite(cs, LOW);

  SPI.transfer16(0x002E);
  SPI.transfer(0x01);
  uint8_t phycfgr = SPI.transfer(0x00);
  
  digitalWrite(cs, HIGH);

#ifdef DEBUG
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
  
  // Power down W5500 when RST is connected to the ESP32.
  if ( HardwareType == P1UX2 && HardwareVersion >= 101 ) { 
    digitalWrite(rst,LOW); 
    DebugTln("P1UX2: W5500 RESET LOW");
    return;
  }

  pinMode(cs, OUTPUT);
  SPI.begin(sck, miso, mosi);
  
  W5500_Read_PHYCFGR();

  // This board needs FDM write mode here: after ETH.end(), the W5500 does not
  // reliably react to 0x00, while 0x04 + 0x70 does enter low-power mode.
  SPI.beginTransaction(SPISettings(8000000UL, MSBFIRST, SPI_MODE0));
  digitalWrite(cs, LOW);
  SPI.transfer16(0x002E);
  SPI.transfer(0x04);
  SPI.transfer(0x70);
  digitalWrite(cs, HIGH);
  SPI.endTransaction();

  delay(500);
  W5500_Read_PHYCFGR();
}

#else
  void startETH(){}
  void enterPowerDownMode(){}
#endif // ETHERNET

String IP_Address(){

#ifdef ETHERNET
  if ( netw_state == NW_ETH ) return ETH.localIP().toString();
  else return WiFi.localIP().toString();
#else
  return WiFi.localIP().toString();
#endif
}
