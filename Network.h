
/*
***************************************************************************  
**  Program  : network.h, part of DSMRloggerAPI
**
**  Copyright (c) 2021 Willem Aandewiel / Martijn Hendriks
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/
#include <WiFi.h>        // Core WiFi Library         
#include <ESPmDNS.h>        // part of Core https://github.com/esp8266/Arduino
#include <Update.h>
#include "Html.h"
#include <WiFiManager.h>        // version 0.16.0 - https://github.com/tzapu/WiFiManager
#include <HTTPClient.h>
#include "NetTypes.h"

#ifdef INSIGHT
  #include "Insights.h"
  #include <insights_custom.h>
#endif

WebServer httpServer(80);
NetServer ws_raw(82);

bool FSmounted           = false; 
bool WifiConnected       = false;
bool WifiBoot            = true;
char APIurl[42]          = "http://api.smart-stuff.nl/v1/register.php";

#define   MaxWifiReconnect  10
DECLARE_TIMER_SEC(WifiReconnect, 5); //try after x sec

void LogFile(const char*, bool);
void P1Reboot();
void SwitchLED( byte mode, byte color);

/***===========================================================================================
    POST MAC + IP
    https://www.allphptricks.com/create-and-consume-simple-rest-api-in-php/
    http://g2pc1.bu.edu/~qzpeng/manual/MySQL%20Commands.htm
**/
void PostMacIP() {
  HTTPClient http;
  http.begin(wifiClient, APIurl);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String httpRequestData = "mac=" + WiFi.macAddress() + "&ip=" + IPAddress()+ "&version=" + _VERSION_ONLY;           
  int httpResponseCode = http.POST(httpRequestData);
  DebugT(F("HTTP Response code: "));Debugln(httpResponseCode);
  http.end();  
}

#ifndef ETHERNET

static void onWifiEvent (WiFiEvent_t event) {
    switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
        DebugTf ("Connected to %s. Asking for IP address.\r\n", WiFi.BSSIDstr().c_str());
//        digitalWrite(LED, LED_ON);
        SwitchLED( LED_ON, BLUE );
        break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        LogFile("Wifi Connected",true);
//        digitalWrite(LED, LED_ON);
        SwitchLED( LED_ON, BLUE );
        Debug (F("\nConnected to " )); Debugln (WiFi.SSID());
        Debug (F("IP address: " ));  Debug (WiFi.localIP());
        Debug (F(" ( gateway: " ));  Debug (WiFi.gatewayIP());Debug(" )\n\n");
        WifiBoot = false;
        WifiConnected = true;
        break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
//        digitalWrite(LED, LED_OFF);
        SwitchLED( LED_OFF, BLUE );
        if (DUE(WifiReconnect)) {
          if ( WifiConnected ) LogFile("Wifi connection lost",true); //log only once 
          WifiConnected = false;                 
          WiFi.reconnect();
        }
        break;
    default:
        DebugT(F("[WiFi-event] event: ")); Debugln(event);
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
void startWiFi(const char* hostname, int timeOut) 
{
  WiFi.setHostname(hostname);
  WiFiManager manageWiFi;
  uint32_t lTime = millis();

//  DebugTln("start ...");
  LogFile("Wifi Starting",true);
//  digitalWrite(LED, LED_OFF);
  SwitchLED( LED_OFF, BLUE );
  
  WifiBoot = true;
  WiFi.onEvent(onWifiEvent);
  manageWiFi.setDebugOutput(false);
      
  //add custom html at inside <head> for all pages -> show password function
//  manageWiFi.setCustomHeadElement("<script>function f() {var x = document.getElementById('p');x.type==='password'?x.type='text':x.type='password';}</script>");
  manageWiFi.setClass("invert"); //dark theme
  
  //--- set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  manageWiFi.setAPCallback(configModeCallback);

  manageWiFi.setTimeout(timeOut);  // in seconden ...

  if ( !manageWiFi.autoConnect("P1-Dongle-Pro") )
  {
    LogFile("Wifi failed to connect and hit timeout",true);
//    DebugTf(" took [%d] seconds ==> ERROR!\r\n", (millis() - lTime) / 1000);
    P1Reboot();
    return;
  } 
  //  phy_bbpll_en_usb(true); 
  DebugTf("Took [%d] seconds => OK!\n", (millis() - lTime) / 1000);
#ifdef INSIGHT 
  Insights.begin(insights_auth_key);
#endif
  PostMacIP(); //post mac en ip 

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
  if (MDNS.begin(Hostname)) DebugTf("[2] mDNS responder started as [%s.local]\r\n", Hostname);    
  else  DebugTln(F("[3] Error setting up MDNS responder!\r\n"));
  MDNS.addService("http", "tcp", 80);
  
} // startMDNS()

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
