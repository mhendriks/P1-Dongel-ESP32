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
  HTTPClient http;
  http.begin(wifiClient, APIURL);
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
  if ( WiFi.isConnected() ) WiFi.disconnect(true,true);
  btStop();
  WiFi.mode(WIFI_OFF);
  esp_wifi_stop();
  esp_wifi_deinit();
  WiFi.setSleep(true);
}

//int WifiDisconnect = 0;
bool bNoNetworkConn = false;
bool bEthUsage = false;

// Network event -> Ethernet is dominant
// https://github.com/espressif/arduino-esp32/blob/master/libraries/Network/src/NetworkEvents.h
static void onNetworkEvent (WiFiEvent_t event) {
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
        break;
      }
    // case ARDUINO_EVENT_ETH_LOST_IP: //6 not available in SDK 2.x
    //   DebugTln("!!! ETH Lost IP");
    //   netw_connected = false;
    //   break;
    case ARDUINO_EVENT_ETH_STOP: //2
      DebugTln("!!! ETH Stopped");
    case ARDUINO_EVENT_ETH_DISCONNECTED: //4
      // if ( Update.isRunning() ) Update.abort();
      if ( netw_state != NW_WIFI ) netw_state = NW_NONE;
      SwitchLED( LED_ON , LED_RED );
      LogFile("ETH Disconnected", true);
      break;
#endif //ETHERNET
//WIFI
    case ARDUINO_EVENT_WIFI_STA_CONNECTED: //4
        sprintf(cMsg,"Connected to %s. Asking for IP address", WiFi.BSSIDstr().c_str());
        LogFile(cMsg, true);
//        tWifiLost = 0;
        break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP: //7
        LogFile("Wifi Connected",true);
        SwitchLED( LED_ON, LED_BLUE );
        Debug (F("IP address: " ));  Debug (WiFi.localIP());
        Debug(" )\n\n");
        WifiReconnect = 0;
        if ( bEthUsage ) WifiOff();        
        else {
          enterPowerDownMode(); //disable ETH
          netw_state = NW_WIFI;
        }
        bNoNetworkConn = false;
        break;
    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
        tWifiLost = millis();
        if ( netw_state != NW_ETH ) netw_state = NW_NONE;
        break;           
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: //5
#ifdef ULTRA
        if ( netw_state != NW_ETH ) SwitchLED( LED_ON, LED_RED );
#else
        SwitchLED( LED_OFF, LED_BLUE );
#endif        
        break;
    default:
        sprintf(cMsg,"Network-event : %d | rssi: %d | channel : %i",event, WiFi.RSSI(), WiFi.channel());
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
  //if you used auto generated SSID, print it
  DebugTln(myWiFiManager->getConfigPortalSSID());
} // configModeCallback()

//===========================================================================================
void WifiWatchDog(){
#if not defined ETHERNET || defined ULTRA
  //try to reconnect or reboot when wifi is down
  if ( bEthUsage ) return; //leave when ethernet is prefered network
  if ( WiFi.status() != WL_CONNECTED ){
    if ( !bNoNetworkConn ) {
      LogFile("Wifi connection lost",true); //log only once 
      tWifiLost = millis();
      bNoNetworkConn = true;
    }
    
    if ( (millis() - tWifiLost) >= 20000 ) {
      DebugTln("WifiLost > 20.000, disconnect");
      WiFi.disconnect();
      delay(100); //give it some time
      WiFi.reconnect();
//      Wifi.begin(WiFi.SSID().c_str(), WiFi.psk().c_str()); // better to disconnect and begin?
      tWifiLost = millis();
      WifiReconnect++;
      DebugT("WifiReconnect: ");Debug(WifiReconnect);
      return;
   }
    if (WifiReconnect >= 3) {
      LogFile("Wifi -> Reboot because of timeout",true); //log only once 
      P1Reboot(); //after 3 x 20.000 millis
    }
  }
#endif  
}

//===========================================================================================
void startWiFi(const char* hostname, int timeOut) 
{  
#if not defined ETHERNET || defined ULTRA
#ifdef ULTRA
  //lets wait on ethernet first for 3.5sec and consume some time to charge the capacitors
  uint8_t timeout = 0;
  while ( (netw_state == NW_NONE) && (timeout++ < 35) ) {
    delay(100); 
    Debug(".");
  } 
  Debugln();
  // w5500_powerDown();
#endif 
  
  if ( netw_state != NW_NONE ) return;
  
  //lower calibration power
  esp_wifi_set_ps(WIFI_PS_MAX_MODEM);

  WiFi.setHostname(hostname);
  WiFi.enableIPv6();
//  WiFi.setMinSecurity(WIFI_AUTH_WEP);
  WiFi.setMinSecurity(WIFI_AUTH_WPA_PSK);
  
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);  //to solve mesh issues 
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);      //to solve mesh issues 
  if ( bFixedIP ) WiFi.config(staticIP, gateway, subnet, dns);
  WiFiManager manageWiFi;
  LogFile("Wifi Starting",true);
  SwitchLED( LED_OFF, LED_BLUE );
  
  manageWiFi.setConfigPortalBlocking(false);
  manageWiFi.setDebugOutput(false);
  manageWiFi.setShowStaticFields(true); // force show static ip fields
  manageWiFi.setShowDnsFields(true);    // force show dns field always  
  manageWiFi.setRemoveDuplicateAPs(false);
  manageWiFi.setScanDispPerc(true); // display percentages instead of graphs for RSSI
  //  manageWiFi.setWiFiAutoReconnect(true); //buggy

  //add custom html at inside <head> for all pages -> show pasessword function
  manageWiFi.setClass("invert"); //dark theme
  
  //--- set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  manageWiFi.setAPCallback(configModeCallback);

  manageWiFi.setTimeout(timeOut);  // in seconden ...
  manageWiFi.autoConnect(_HOTSPOT);

  //handle wifi webinterface timeout and connection (timeOut in sec)
  uint16_t i = 0;
  while ( (i++ < timeOut*10) && (netw_state == NW_NONE) && !bEthUsage ){
    Debug("*");
    delay(100);
    manageWiFi.process();
    SwitchLED(i%4?LED_ON:LED_OFF,LED_BLUE); //fast blinking
  }
  Debugln();
  if ( netw_state == NW_NONE && !bEthUsage ) {
    LogFile("WIFI: failed to connect and hit timeout",true);
    P1Reboot(); //timeout 
  }
  manageWiFi.stopWebPortal();
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
  if ( loadFixedIPConfig("/fixedip.json") ) bFixedIP = validateConfig();
  startETH();
  startWiFi(settingHostname, 240);  // timeout 4 minuten
  WaitOnNetwork();
  GetMacAddress();
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

void startETH(){
  
  //derive ETH mac address
  uint8_t mac_eth[6] = { 0xFE, 0xED, 0xDE, 0xAD, 0xBE, 0xEF };
  esp_read_mac(mac_eth, ESP_MAC_ETH);
  
  // SPI.begin(SCK_GPIO, MISO_GPIO, MOSI_GPIO);
  ETH.enableIPv6();
  // ETH.begin(ETH_TYPE, ETH_ADDR, CS_GPIO, INT_GPIO, ETH_RST, SPI2_HOST ,SCK_GPIO, MISO_GPIO, MOSI_GPIO); //sdk3.0
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
#ifdef ETHERNET
  if ( netw_state == NW_ETH ) return ETH.macAddress();
  else return WiFi.macAddress();
#else
  return WiFi.macAddress();
#endif
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
