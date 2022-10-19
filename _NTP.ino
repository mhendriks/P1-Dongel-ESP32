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
  inspiration: https://randomnerdtutorials.com/esp32-ntp-timezones-daylight-saving/
  TZ table https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv

  Europe/Amsterdam  CET-1CEST,M3.5.0,M10.5.0/3
  Europe/Brussels   CET-1CEST,M3.5.0,M10.5.0/3 same

*/

#ifdef USE_NTP_TIME 

#define NPT_SYNC_INTERVAL 600 //10 minutes

//=======================================================================
void setTimezone(String timezone){
  Serial.printf("  Setting Timezone to %s\n",timezone.c_str());
  setenv("TZ",timezone.c_str(),1);  //  Now adjust the TZ.  Clock settings are adjusted to show the new local time
  tzset();
}

//=======================================================================
void printLocalTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    DebugTln(F("Failed to obtain time 1"));
    return;
  }
  DebugTln(&timeinfo, "%A, %B %d %Y %H:%M:%S zone %Z %z ");
}

//=======================================================================
void startNTP() {
  struct tm timeinfo;
//  time_t local_time;
  DebugTln(F("Starting NTP"));
  configTime(0, 0, "pool.ntp.org");    // First connect to NTP server, with 0 TZ offset
  if(!getLocalTime(&timeinfo)){
    DebugTln(F("Failed to obtain time"));
    return;
  }
  DebugTln(F("Got the time from NTP"));
  printLocalTime();
  setTimezone("CET-1CEST,M3.5.0,M10.5.0/3");
  printLocalTime();
  setSyncProvider(getNtpTime);
  setSyncInterval(NPT_SYNC_INTERVAL); 
  
} // startNTP()

//=======================================================================
time_t getNtpTime() {
    struct tm timeinfo;
    time_t local_time;
    if ( getLocalTime(&timeinfo) ) {
      time(&local_time);
      DSTactive = timeinfo.tm_isdst;
      sprintf(actTimestamp, "%02d%02d%02d%02d%02d%02d%s\0\0", (timeinfo.tm_year -100 ), timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,DSTactive?"S":"W");
//      Serial.print(("NTP Synced: ")); Serial.println(actTimestamp);
      return local_time;
  } else {
    DebugTln(F("ERROR!!! No NTP server reached!\r\n\r"));
    return 0;
  }
}

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
