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

bool FSmounted          = false; 
//bool WifiConnected    = false;
//time_t tWifiReconnect = 0;
time_t tWifiLost        = 0;
byte  WifiReconnect     = 0;

void LogFile(const char*, bool);
void P1Reboot();
void SwitchLED( byte mode, uint32_t color);
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
  
#ifndef AP_ONLY
  String httpRequestData = "mac=" + String(macStr) + "&ip=" + IP_Address() + "&version=" + _VERSION_ONLY;           
#else
  String httpRequestData = "mac=" + String(macStr) + "&ip=" + IP_Address() + "&version=" + _VERSION_ONLY;           
#endif  
  
  int httpResponseCode = http.POST(httpRequestData);

#ifdef DEBUG  
  DebugT(F("HTTP RequestData: "));Debugln(httpRequestData);
  DebugT(F("HTTP Response code: "));Debugln(httpResponseCode);
#endif  

  http.end();  
}

#ifndef ETHERNET

//int WifiDisconnect = 0;
bool bNoNetworkConn = false;

static void onWifiEvent (WiFiEvent_t event) {
    sprintf(cMsg,"WiFi-event : %d | rssi: %d | channel : %i",event, WiFi.RSSI(), WiFi.channel());
    LogFile(cMsg, true);
    switch (event) {
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
        bNoNetworkConn = false;
        break;
    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
        tWifiLost = millis();
        break;           
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: //5
        SwitchLED( LED_OFF, LED_BLUE );
        break;
    default:
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
  //try to reconnect or reboot when wifi is down
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
}

//===========================================================================================
void startWiFi(const char* hostname, int timeOut) 
{
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
  
  WiFi.onEvent(onWifiEvent);
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
  if ( !manageWiFi.autoConnect(_HOTSPOT) )
  {
    LogFile("Wifi failed to connect and hit timeout",true);
//    DebugTf(" took [%d] seconds ==> ERROR!\r\n", (millis() - lTime) / 1000);
    P1Reboot();
    return;
  } 
  //  phy_bbpll_en_usb(true); 
//  DebugTf("Took [%d] seconds => OK!\n", (millis() - lTime) / 1000);

} // startWiFi()

#endif //ifndef ETHERNET

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
