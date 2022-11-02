/*
***************************************************************************  
**  Program  : ntpStuff, part of DSMRloggerAPI
**  Version  : v4.2.1
**
**  Copyright (c) 2021 Martijn Hendriks
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

/** 
  inspiration: 
  https://randomnerdtutorials.com/esp32-ntp-timezones-daylight-saving/
  https://randomnerdtutorials.com/esp32-ntp-client-date-time-arduino-ide/
  
  TZ table https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv

  Europe/Amsterdam  CET-1CEST,M3.5.0,M10.5.0/3
  Europe/Brussels   CET-1CEST,M3.5.0,M10.5.0/3 same

  DST mrt - oct -> +1h
  zone UTC+1 (normaal = wintertijd) tijdens DST UTC+2

*/

#include "esp_sntp.h"

#define NTP_SYNC_INTERVAL 3600 // 60 * 60 = 3600 sec = 1h
//#define NTP_SYNC_INTERVAL 600 //10min

//=======================================================================
void setTimezone(String timezone){
  DebugTf("Set Timezone to %s\n",timezone.c_str());
  setenv("TZ",timezone.c_str(),1);  //  Now adjust the TZ.  Clock settings are adjusted to show the new local time
  tzset();
}

//=======================================================================
void printLocalTime(){
  if(getLocalTime(&tm)) DebugTln(&tm, "%A, %B %d %Y %H:%M:%S zone %Z %z ");
  else DebugTln(F("Failed to obtain time"));  
}


//=======================================================================
void cbSyncTime(struct timeval *tv)  // callback function to show when NTP was synchronized
{
  if(getLocalTime(&tm)) setTime(tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_mday, tm.tm_mon + 1, tm.tm_year - 100 );
  DSTactive = tm.tm_isdst;
//  LogFile("NTP time synched", true);
}

//=======================================================================
void startNTP() {
  DebugTln(F("Starting NTP"));

  sntp_set_time_sync_notification_cb(cbSyncTime);  // set a Callback function for time synchronization notification
  sntp_set_sync_interval( NTP_SYNC_INTERVAL * 1000UL ); //sync 
  configTime(0, 0, "europe.pool.ntp.org");    // First connect to NTP server, with 0 TZ offset
  setTimezone("CET-1CEST,M3.5.0,M10.5.0/3");
  if(!getLocalTime(&tm)){
    DebugTln(F("Failed to obtain time"));
    return;
  }
  DebugT(F("NTP Sync interval [sec]: "));Debugln(sntp_get_sync_interval()/1000);
  printLocalTime();
  sprintf(actTimestamp, "%02d%02d%02d%02d%02d%02d%s\0\0", (tm.tm_year - 100 ), tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,DSTactive?"S":"W");  
  
} // startNTP()


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
