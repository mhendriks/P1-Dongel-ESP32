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
#include <ESPmDNS.h>        
#include <Update.h>
#include <WiFiManager.h>        // https://github.com/tzapu/WiFiManager
#include <HTTPClient.h>
#include "NetTypes.h"

WebServer httpServer(80);
NetServer ws_raw(82);

time_t tWifiLost        = 0;
byte  WifiReconnect     = 0;

void LogFile(const char*, bool);
void P1Reboot();
void SwitchLED( byte mode, uint32_t color);
void startETH();
void startWiFi(const char* hostname, int timeOut);

String MAC_Address();
String  IP_Address();

void GetMacAddress(){

  String _mac = MAC_Address();
  strcpy( macStr, _mac.c_str() );
  _mac.replace( ":","" );
  strcpy( macID, _mac.c_str() );
  USBPrint( "MacStr: " );USBPrintln( macStr ); //only at setup
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

//int WifiDisconnect = 0;
bool bNoNetworkConn = false;

// Network event -> Ethernet is dominant
static void onNetworkEvent (WiFiEvent_t event) {
  switch (event) {
  //ETH    
    case ARDUINO_EVENT_ETH_START: //1
      DebugTln("ETH Started");
      // ETH.setHostname(_HOSTNAME); SDK 3.0 feature
      break;
    case ARDUINO_EVENT_ETH_CONNECTED: //3
      DebugTln("\nETH Connected");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP: //5
      {
        WiFiManager manageWiFi;
        if ( WiFi.isConnected() ) WiFi.disconnect();
        LogFile("ETH GOT IP", true);
        netw_state = NW_ETH;
        SwitchLED( LED_ON , LED_BLUE ); //Ethernet available = RGB LED Blue
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
        if ( netw_state == NW_ETH ) WiFi.disconnect();
        else netw_state = NW_WIFI;
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
        sprintf(cMsg,"WiFi-event : %d | rssi: %d | channel : %i",event, WiFi.RSSI(), WiFi.channel());
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
#if not defined ETHERNET  || defined ULTRA
  //try to reconnect or reboot when wifi is down
   if ( WiFi.status() != WL_CONNECTED && WiFi.status() != WL_DISCONNECTED ){
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
  } 
#endif 
  if ( netw_state != NW_NONE ) return;
  WiFi.setHostname(hostname);
//  WiFi.setMinSecurity(WIFI_AUTH_WEP);
  WiFi.setMinSecurity(WIFI_AUTH_WPA_PSK);
  
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);  //to solve mesh issues 
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);      //to solve mesh issues 
  WiFiManager manageWiFi;
//  uint32_t lTime = millis();
//  DebugTln("start ...");
  LogFile("Wifi Starting",true);
//  digitalWrite(LED, LED_OFF);
  SwitchLED( LED_OFF, LED_BLUE );
  
  manageWiFi.setConfigPortalBlocking(false);
  manageWiFi.setDebugOutput(false);
  manageWiFi.setShowStaticFields(true); // force show static ip fields
  manageWiFi.setShowDnsFields(true);    // force show dns field always  
  manageWiFi.setRemoveDuplicateAPs(false);
  manageWiFi.setScanDispPerc(true); // display percentages instead of graphs for RSSI
//  manageWiFi.setWiFiAutoReconnect(true); //buggy

  //add custom html at inside <head> for all pages -> show pasessword function
//  manageWiFi.setCustomHeadElement("<script>function f() {var x = document.getElementById('p');x.type==='password'?x.type='text':x.type='password';}</script>");
  manageWiFi.setClass("invert"); //dark theme
  
  //--- set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  manageWiFi.setAPCallback(configModeCallback);

  manageWiFi.setTimeout(timeOut);  // in seconden ...
  manageWiFi.autoConnect(_HOTSPOT);
 /* 
  if ( !manageWiFi.autoConnect(_HOTSPOT) )
  {
    LogFile("Wifi failed to connect and hit timeout",true);
//    DebugTf(" took [%d] seconds ==> ERROR!\r\n", (millis() - lTime) / 1000);
    P1Reboot();
    return;
  } 
*/
  //handle wifi webinterface timeout and connection
  //timeOut in sec
  uint16_t i = 0;
  while ( (i++ < timeOut*10) && (netw_state == NW_NONE) ){
    Debug("*");
    delay(100);
    manageWiFi.process();
    SwitchLED(i%4?LED_ON:LED_OFF,LED_BLUE); //fast blinking
  }
  Debugln();
  if ( netw_state == NW_NONE ) {
    P1Reboot(); //timeout hit
    LogFile("Wifi failed to connect and hit timeout",true);
  }
  manageWiFi.stopWebPortal();
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
//===========================================================================================
void startNetwork()
{
  WiFi.onEvent(onNetworkEvent);
  startETH();
  startWiFi(settingHostname, 240);  // timeout 4 minuten
  WaitOnNetwork();
  GetMacAddress();
  USBPrint("ip-adres: ");USBPrintln(IP_Address());
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
  MDNS.addService("http", "tcp", 80);
  
} // startMDNS()

#endif
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
