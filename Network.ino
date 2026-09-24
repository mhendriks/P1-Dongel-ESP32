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
    DebugVerboseT(F("WifiLost -> watchdog immediate reconnect attempt: "));
    DebugVerboseLn(s_wifiReconnectCnt + 1);
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
    DebugVerboseT(F("WifiLost > "));
    DebugVerbose(WIFI_WATCHDOG_LOST_MS);
    DebugVerboseLn(F(" ms, reconnect"));
    DebugVerbose(F("WifiReconnect attempt: "));
    DebugVerboseLn(s_wifiReconnectCnt + 1);
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

#ifdef ETHERNET
static bool isZeroMac(const uint8_t *mac){
  for (uint8_t i = 0; i < 6; i++) {
    if (mac[i] != 0x00) return false;
  }
  return true;
}
#endif

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

// Keep the provisioning access point active after a successful connection
// until the user has confirmed they are ready to leave the portal.
static const uint32_t WIFI_PROVISION_CONTINUE_TIMEOUT_MS = 60000;
// Three 15-second connection attempts may be needed before WiFiManager gives
// up, so keep the portal feedback pending a little longer than that.
static const uint32_t WIFI_PROVISION_CONNECT_TIMEOUT_MS = 50000;
static const uint8_t WIFI_BOOT_CONNECT_ATTEMPTS = 3;
static uint32_t wifiProvisioningConnectStartedAt = 0;
static bool wifiProvisioningContinueRequested = false;
static bool wifiProvisioningRetryPrepared = false;
static uint8_t wifiLastDisconnectReason = 0;

static bool wifiAuthenticationFailed(uint8_t reason) {
  return reason == WIFI_REASON_AUTH_EXPIRE ||
         reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
         reason == WIFI_REASON_AUTH_FAIL ||
         reason == WIFI_REASON_HANDSHAKE_TIMEOUT;
}

static const char WIFI_PROVISIONING_DARK_STYLE[] PROGMEM =
  "<style>body{font-family:verdana,sans-serif;text-align:center;margin:0;padding:1.5em;line-height:1.5;background:#060606;color:#fff}"
  "h1{color:#fff}.address{display:block;margin:.7em 0;padding:.8em;background:#282828;color:#fff;border:1px solid #555;border-radius:.4em;font-size:1.05em;word-break:break-all;text-decoration:none}"
  ".action,button{display:inline-block;margin-top:1em;padding:.8em 1em;border:0;border-radius:.3em;background:#1fa3ec;color:#fff;font-size:1em;text-decoration:none}"
  "small{display:block;margin-top:1.5em;color:#ccc}</style></head><body>";

static String wifiProvisioningPageStart(const __FlashStringHelper* title, size_t capacity) {
  String page;
  page.reserve(capacity);
  page += F("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'><meta http-equiv='cache-control' content='no-store'><title>");
  page += title;
  page += F("</title>");
  page += FPSTR(WIFI_PROVISIONING_DARK_STYLE);
  return page;
}

static String wifiProvisioningSuccessPage() {
  const String ipAddress = WiFi.localIP().toString();
  const String hostname = manageWiFi.htmlEntities(String(settingHostname));

  String page = wifiProvisioningPageStart(F("Wi-Fi connected"), 1400);
  page += F("<h1>Wi-Fi connected</h1><p>The dongle is connected to your Wi-Fi network.</p>");
  page += F("<p>IP address:</p><a class='address' href='http://");
  page += ipAddress;
  page += F("/'>http://");
  page += ipAddress;
  page += F("/</a><p>Hostname:</p><a class='address' href='http://");
  page += hostname;
  page += F(".local/'>http://");
  page += hostname;
  page += F(".local/");
  page += F("</a><small>Keep this page open until you have noted the addresses. Select Continue when you are ready to reconnect your phone to your normal Wi-Fi network.</small><form action='/provisioning-continue'><button id='continueButton' type='submit'>Continue (60)</button></form><script>let seconds=60;const button=document.getElementById('continueButton');setInterval(()=>{if(seconds>0){button.textContent='Continue ('+(--seconds)+')';}},1000);</script></body></html>");
  return page;
}

static String wifiProvisioningCompletePage() {
  String page = wifiProvisioningPageStart(F("Configuration complete"), 600);
  page += F("<h1>Configuration complete</h1><p>You can now reconnect to your normal Wi-Fi network.</p></body></html>");
  return page;
}

static String wifiProvisioningFailurePage() {
  String page = wifiProvisioningPageStart(F("Wi-Fi not connected"), 900);
  page += F("<h1>Wi-Fi not connected</h1><p>The dongle could not connect to this Wi-Fi network.</p>");
  page += F("<p>Check the network name and password, then try again.</p><a class='action' href='/wifi?refresh=1'>Try again</a>");
  page += F("<small>The configuration hotspot remains available.</small></body></html>");
  return page;
}

static void sendWifiProvisioningPage(const String& page) {
  manageWiFi.server->sendHeader("Cache-Control", "no-store");
  manageWiFi.server->send(200, "text/html", page);
}

static void wifiProvisioningConnectStarted() {
  wifiProvisioningConnectStartedAt = millis();
  wifiProvisioningRetryPrepared = false;
}

static bool wifiProvisioningConnectionFailed() {
  const uint8_t result = manageWiFi.getLastConxResult();
  return result == WL_CONNECT_FAILED || result == WL_NO_SSID_AVAIL ||
         result == WL_CONNECTION_LOST || result == WL_DISCONNECTED;
}

static void wifiProvisioningPrepareRetry() {
  if (wifiProvisioningRetryPrepared) return;
  // WiFiManager leaves the failed STA connection running in non-blocking mode.
  // Stop only STA (not the provisioning AP) so its native Wi-Fi page can make
  // a fresh scan when the user selects Try again.
  WiFi.disconnect(false, false);
  WiFi.scanDelete();
  wifiProvisioningRetryPrepared = true;
}

static void setupWifiProvisioningSuccessPage() {
  // This callback runs after WiFiManager has created its web server but before
  // it installs its own routes. Our distinct route therefore remains intact.
  manageWiFi.server->on("/provisioning-success", []() {
    if (WiFi.status() == WL_CONNECTED) {
      sendWifiProvisioningPage(wifiProvisioningSuccessPage());
      return;
    }
    if (wifiProvisioningConnectStartedAt != 0 &&
        (wifiProvisioningConnectionFailed() ||
         (uint32_t)(millis() - wifiProvisioningConnectStartedAt) >= WIFI_PROVISION_CONNECT_TIMEOUT_MS)) {
      wifiProvisioningPrepareRetry();
      sendWifiProvisioningPage(wifiProvisioningFailurePage());
      return;
    }
    manageWiFi.server->send(204, "text/plain", "");
  });
  manageWiFi.server->on("/provisioning-continue", []() {
    wifiProvisioningContinueRequested = true;
    sendWifiProvisioningPage(wifiProvisioningCompletePage());
  });
}

static const char WIFI_PROVISIONING_SUCCESS_SCRIPT[] PROGMEM =
  "<script>if(location.pathname=='/wifisave')addEventListener('DOMContentLoaded',()=>{document.body.innerHTML='<h2>Connecting to Wi-Fi...</h2><p>Please wait.</p>';(async function poll(){try{const r=await fetch('/provisioning-success',{cache:'no-store'});if(r.status==200){document.open();document.write(await r.text());document.close();return}}catch(_){}setTimeout(poll,1000)})()})</script>";

#if DIRECT_AP_CONNECT
static bool directApIsHex(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static String directApNormalizeSerial(const String& serial) {
  String out;
  out.reserve(serial.length());
  for (size_t i = 0; i < serial.length(); i++) {
    char c = serial.charAt(i);
    if (c == ':' || c == '-') continue;
    if (!directApIsHex(c)) return "";
    out += (char)toupper(c);
  }
  return ((out.length() >= 8) && ((out.length() % 2) == 0)) ? out : "";
}

static String directApPasswordFromSerial(const String& serial) {
  String normalized = directApNormalizeSerial(serial);
  if (normalized.length() < 8) return "";

  String password;
  password.reserve(8);
  for (int i = normalized.length() - 2; i >= (int)normalized.length() - 8; i -= 2) {
    password += normalized.substring(i, i + 2);
  }
  return password;
}

static bool directApSsidAllowed(const String& ssid) {
  const String ssidPrefix = DIRECT_AP_SSID_PREFIX;
  if (!ssidPrefix.length()) {
    DebugTln(F("DirectAP: ssid prefix missing"));
    return false;
  }
  if (!ssid.startsWith(ssidPrefix)) return false;

  const String targetSerial = DIRECT_AP_TARGET_SERIAL;
  if (!targetSerial.length()) {
    return directApPasswordFromSerial(ssid.substring(ssidPrefix.length())).length() == 8;
  }

  return directApNormalizeSerial(ssid.substring(ssidPrefix.length())) == directApNormalizeSerial(targetSerial);
}

static bool directApFindAccessPoint(String& ssid, String& password) {
  int networkCount = WiFi.scanNetworks(false, true);
  int bestIndex = -1;
  int32_t bestRssi = INT32_MIN;

  for (int i = 0; i < networkCount; i++) {
    String candidate = WiFi.SSID(i);
    if (!directApSsidAllowed(candidate)) continue;
    if (bestIndex < 0 || WiFi.RSSI(i) > bestRssi) {
      bestIndex = i;
      bestRssi = WiFi.RSSI(i);
    }
  }

  if (bestIndex < 0) {
    WiFi.scanDelete();
    return false;
  }

  ssid = WiFi.SSID(bestIndex);
  password = directApPasswordFromSerial(ssid.substring(String(DIRECT_AP_SSID_PREFIX).length()));
  WiFi.scanDelete();
  return password.length() == 8;
}

static bool directApConnectTo(const String& ssid, const String& password, uint32_t timeoutMs) {
  if (!ssid.length() || password.length() != 8) return false;

  DebugVerboseTf("DirectAP: connecting to [%s]\n", ssid.c_str());
  WiFi.persistent(true);
  WiFi.begin(ssid.c_str(), password.c_str());

  uint32_t startMs = millis();
  while ((uint32_t)(millis() - startMs) < timeoutMs && !skipNetwork && !bEthUsage) {
    if (WiFi.status() == WL_CONNECTED || netw_state == NW_WIFI) return true;
    DebugTrace(F("t"));
    delay(250);
    esp_task_wdt_reset();
    SwitchLED(((millis() / 500) % 2) ? LED_ON : LED_OFF, LED_BLUE);
  }
  DebugTraceLn();
  return WiFi.status() == WL_CONNECTED || netw_state == NW_WIFI;
}

static bool directApStartWiFi() {
  if (!String(DIRECT_AP_SSID_PREFIX).length()) {
    DebugTln(F("DirectAP: DIRECT_AP_SSID_PREFIX missing"));
    return false;
  }

  String ssid = preferences.getString("direct_ap_ssid", "");
  String password = preferences.getString("direct_ap_psk", "");
  if (ssid.length() && password.length() == 8 && directApSsidAllowed(ssid)) {
    if (directApConnectTo(ssid, password, DIRECT_AP_CONNECT_TIMEOUT_MS)) return true;
    DebugTln(F("DirectAP: stored AP did not connect, scanning again"));
    WiFi.disconnect(true, false);
    delay(250);
  }

  uint32_t lastScanMs = 0;
  while (!skipNetwork && !bEthUsage) {
    uint32_t now = millis();
    if (lastScanMs == 0 || (uint32_t)(now - lastScanMs) >= DIRECT_AP_SCAN_INTERVAL_MS) {
      lastScanMs = now;
      DebugTln(F("DirectAP: scanning for access point"));
      if (directApFindAccessPoint(ssid, password)) {
        preferences.putString("direct_ap_ssid", ssid);
        preferences.putString("direct_ap_psk", password);
        return directApConnectTo(ssid, password, DIRECT_AP_CONNECT_TIMEOUT_MS);
      }
      DebugTln(F("DirectAP: no matching access point found"));
    }
    delay(250);
    esp_task_wdt_reset();
    SwitchLED(((millis() / 500) % 2) ? LED_ON : LED_OFF, LED_BLUE);
  }

  return false;
}
#endif

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
  DebugVerbose(F("DongleID : ")); DebugVerboseLn(DongleID);
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
#else
  (void)httpResponseCode;
#endif  

  http.end();  
}

void WifiOff() {
#ifdef ESPNOW
  if ( bNRGMenabled ) return;
  StopESPNOW();
#endif
  if ( WiFi.isConnected() ) WiFi.disconnect(true,true);
  WiFi.mode(WIFI_OFF);
  esp_wifi_stop();
  WiFi.setSleep(true);
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
        wifiLastDisconnectReason = reason;
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

bool wifiPortalWasUsed = false;

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

#if DIRECT_AP_CONNECT
  if (!directApStartWiFi() && !skipNetwork && !bEthUsage) {
    LogFile("reboot: DirectAP Wifi failed to connect", true);
    P1Reboot();
  }
  if ( skipNetwork ) return;
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_max_tx_power(78);
  SwitchLED( LED_ON, LED_BLUE );
  return;
#endif
  
  manageWiFi.setConnectTimeout(15);
  // WiFiManager performs retry attempts in one blocking call. That can starve
  // our task watchdog, so retries are deliberately scheduled below instead.
  manageWiFi.setConnectRetries(1);
  manageWiFi.setConfigPortalBlocking(false);
  // Keep the AP and portal available while we present the DHCP result.
  manageWiFi.setDisableConfigPortal(false);
  manageWiFi.setDebugOutput(false);
  manageWiFi.setShowStaticFields(true);
  manageWiFi.setShowDnsFields(true);
  manageWiFi.setRemoveDuplicateAPs(false);
  manageWiFi.setScanDispPerc(true);
  manageWiFi.setClass("invert");
  manageWiFi.setWebServerCallback(setupWifiProvisioningSuccessPage);
  manageWiFi.setPreSaveConfigCallback(wifiProvisioningConnectStarted);
  manageWiFi.setCustomHeadElement(WIFI_PROVISIONING_SUCCESS_SCRIPT);
  
  manageWiFi.setAPCallback(configModeCallback);
  manageWiFi.setConfigPortalTimeout(timeOut);
  
  esp_task_wdt_reset();
  allowSkipNetworkByButton = true;
  wifiPortalWasUsed = false;
  wifiProvisioningConnectStartedAt = 0;
  wifiProvisioningContinueRequested = false;
  wifiProvisioningRetryPrepared = false;
  wifiLastDisconnectReason = 0;
  bool wifiConnected = false;
  manageWiFi.setEnableConfigPortal(false);
  for (uint8_t attempt = 1; attempt <= WIFI_BOOT_CONNECT_ATTEMPTS; attempt++) {
    DebugVerboseTf("WiFi boot connection attempt %u of %u\n", attempt, WIFI_BOOT_CONNECT_ATTEMPTS);
    if (manageWiFi.autoConnect(settingHostname) || WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      break;
    }
    esp_task_wdt_reset();
    if (wifiAuthenticationFailed(wifiLastDisconnectReason)) {
      LogFile("WiFi authentication failed; opening configuration portal", true);
      break;
    }
    if (attempt < WIFI_BOOT_CONNECT_ATTEMPTS) {
      // Give the ESP-IDF station state machine time to finish its failed
      // attempt before WiFiManager restarts the station for the next one.
      delay(1000);
      esp_task_wdt_reset();
    }
  }
  if (!wifiConnected) {
    manageWiFi.setEnableConfigPortal(true);
    manageWiFi.startConfigPortal(settingHostname);
  }

  uint16_t i = 0;
  while ( (i++ < 3000) && (netw_state == NW_NONE) && !bEthUsage && !skipNetwork ) {
    DebugTrace(F("*"));
    delay(100);
    if (WiFi.getMode() & WIFI_AP) wifiPortalWasUsed = true;
    manageWiFi.process();
    esp_task_wdt_reset();
    SwitchLED(i%4?LED_ON:LED_OFF,LED_BLUE);
  }
  DebugTraceLn();
  allowSkipNetworkByButton = false;
  if (wifiPortalWasUsed && !skipNetwork && !bEthUsage && (WiFi.status() == WL_CONNECTED || netw_state == NW_WIFI)) {
    LogFile("Wifi configured via captive portal; showing IP address", true);
    // Do not let the original portal timeout cut short the confirmation page.
    manageWiFi.setConfigPortalTimeout(0);
    const uint32_t continueWaitStartedAt = millis();
    while (!wifiProvisioningContinueRequested &&
           (uint32_t)(millis() - continueWaitStartedAt) < WIFI_PROVISION_CONTINUE_TIMEOUT_MS) {
      manageWiFi.process();
      delay(10);
      esp_task_wdt_reset();
      SwitchLED(LED_ON, LED_BLUE);
    }
    // Let the response to the Continue button leave the webserver before its
    // portal and access point are shut down.
    delay(500);
    manageWiFi.stopConfigPortal();
    LogFile("Wifi provisioning confirmation complete", true);
  }
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
    DebugTrace(F("."));
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
