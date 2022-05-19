/*
***************************************************************************  
**  Program  : ntpStuff, part of DSMRloggerAPI
**  Version  : v4.0.0
**
**  Copyright (c) 2021 Willem Aandewiel / Martijn Hendriks
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

//--- if NTP set, see if it needs synchronizing
void handleNTP()
{
#ifdef USE_NTP_TIME
  if DUE(synchrNTP) {
    if (timeStatus() == timeNeedsSync || prevNtpHour != hour()) {
      prevNtpHour = hour();
      setSyncProvider(getNtpTime);
      setSyncInterval(600);
    }
  }
#endif
}

//=======================================================================
void startNTP() {
#ifdef USE_NTP_TIME 
  DebugTln(F("Starting UDP"));
  Udp.begin(localPort);
  DebugT(F("Local port: "));
  DebugTln(String(Udp.localPort()));
  DebugTln(F("waiting for NTP sync"));
  setSyncProvider(getNtpTime);
  setSyncInterval(60);  // seconds!
  if (timeStatus() == timeSet) {
    prevNtpHour = hour(); 
    time_t t = now(); // store the current time in time variable t
    snprintf(cMsg, sizeof(cMsg), "%02d%02d%02d%02d%02d%02dW\0\0", (year(t) - 2000), month(t), day(t), hour(t), minute(t), second(t)); 
    DebugTf("Time is set to [%s] from NTP\r\n", cMsg); 
  } else {
    DebugTln(F("ERROR!!! No NTP server reached!\r\n\r"));
    P1Reboot();
  }
  
#endif
  
} // startNTP()


#ifdef USE_NTP_TIME

#include <WiFiUdp.h>            // - part of ESP8266 Core https://github.com/esp8266/Arduino
WiFiUDP           Udp;

#define           timeZone 1       // Central European (Winter) Time
#define           localPort  8888   // local port to listen for UDP packets

// NTP Servers:
static const PROGMEM char ntpPool[][30] = { "time.google.com",
                                    "nl.pool.ntp.org",
                                    "0.nl.pool.ntp.org",
                                    "1.nl.pool.ntp.org",
                                    "3.nl.pool.ntp.org"
                                   };
static int        ntpPoolIndx = 0;

char              ntpServerName[50];

#define         NTP_PACKET_SIZE 48 // NTP time is in the first 48 bytes of message
byte              packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

static int        ntpServerNr = 0;
static bool       externalNtpTime = false;
static IPAddress  ntpServerIP; // NTP server's ip address





//=======================================================================
time_t getNtpTime() 
{
  DECLARE_TIMER_MS(waitForPackage, 1500);   

  while(true) 
  {
    yield;
    ntpPoolIndx++;
    if ( ntpPoolIndx > (sizeof(ntpPool) / sizeof(ntpPool[0]) -1) ) 
    {
      ntpPoolIndx = 0;
    }
    snprintf(ntpServerName, sizeof(ntpServerName), "%s", String(ntpPool[ntpPoolIndx]).c_str());

    while (Udp.parsePacket() > 0) ; // discard any previously received packets
    TelnetStream.println("Transmit NTP Request");
    // get a random server from the pool
    WiFi.hostByName(ntpServerName, ntpServerIP);
    TelnetStream.print(ntpServerName);
    TelnetStream.print(": ");
    TelnetStream.println(ntpServerIP);
    TelnetStream.flush();
    sendNTPpacket(ntpServerIP);
    RESTART_TIMER(waitForPackage);
    while (!DUE(waitForPackage))
    {
      int size = Udp.parsePacket();
      if (size >= NTP_PACKET_SIZE) 
      {
        //TelnetStream.print("Receive NTP Response: ");
        Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
        unsigned long secsSince1900;
        // convert four bytes starting at location 40 to a long integer
        secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
        secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
        secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
        secsSince1900 |= (unsigned long)packetBuffer[43];
        time_t t = (secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR);
        snprintf(cMsg, sizeof(cMsg), "%02d:%02d:%02d", hour(t), minute(t), second(t));   
        TelnetStream.printf("[%s] Received NTP Response => new time [%s]  (Winter)\r\n", cMsg, cMsg);
        // return epoch ..
        return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
      }
    }
    TelnetStream.println("No NTP Response :-(");

  } // while true ..
  
  return 0; // return 0 if unable to get the time

} // getNtpTime()


// send an NTP request to the time server at the given address
//=======================================================================
void sendNTPpacket(IPAddress &address) 
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
  
} // sendNTPpacket()


//=======================================================================
time_t dateTime2Epoch(char const *date, char const *time) 
{
    char s_month[5];
    int year, day, h, m, s;
    tmElements_t t;

    static const char month_names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

    if (sscanf(date, "%s %d %d", s_month, &day, &year) != 3) 
    {
      DebugTf("Not a valid date string [%s]\r\n", date);
      return 0;
    }
    t.Day  = day;
    // Find where is s_month in month_names. Deduce month value.
    t.Month = (strstr(month_names, s_month) - month_names) / 3 + 1;
    
  //DebugTf("date=>[%d-%02d-%02d]\r\n", t.Year, t.Month, t.Day);
    
    // Find where is s_month in month_names. Deduce month value.
    t.Month = (strstr(month_names, s_month) - month_names) / 3 + 1;

    if (sscanf(time, "%2d:%2d:%2d", &h, &m, &s) != 3) 
    {
      DebugTf("Not a valid time string [%s] (time set to '0')\r\n", time);
      h = 0;
      m = 0;
      s = 0;
    }
    t.Hour    = h;
    t.Minute  = m;
    t.Second  = s;
    
  //DebugTf("time=>[%02d:%02d:%02d]\r\n", t.Hour, t.Minute, t.Second);

    t.Year = CalendarYrToTm(year);
    
  //DebugTf("converted=>[%d-%02d-%02d @ %02d:%02d:%02d]\r\n", t.Year, t.Month, t.Day, t.Hour, t.Minute, t.Second);

    return makeTime(t);
    
} // dateTime2Epoch()

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
