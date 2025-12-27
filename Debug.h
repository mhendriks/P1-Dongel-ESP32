#ifndef _DEBUG_H
#define _DEBUG_H
/* 
***************************************************************************  
**  Program  : Debug.h, part of DSMRloggerAPI
**
**  Copyright (c) 2025 Smartstuff / based on DSMR Api Willem Aandewiel
**  Met dank aan Erik
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

uint64_t uptime();

/*---- start macro's ------------------------------------------------------------------*/
#ifdef DEBUG
  //DEBUG MODE
  #define DebugBegin(...)({ USBSerial.begin(__VA_ARGS__); while (!USBSerial); })
  #define Debug(...)      ({ USBSerial.print(__VA_ARGS__); if (!bHideP1Log ) TelnetStream.print(__VA_ARGS__); })
  #define Debugln(...)    ({ USBSerial.println(__VA_ARGS__); if (!bHideP1Log ) TelnetStream.println(__VA_ARGS__); })
  #define Debugf(...)     ({ USBSerial.printf(__VA_ARGS__); if (!bHideP1Log ) TelnetStream.printf(__VA_ARGS__); })
  #define DebugFlush()    ({ USBSerial.flush(); TelnetStream.flush(); })
  #define USBPrint(...)   ({ Debug(__VA_ARGS__);})
  #define USBPrintf(...)  ({ Debugf(__VA_ARGS__);})
  #define USBPrintln(...) ({ Debugln(__VA_ARGS__);})                      

#else
  //NORMAL MODE
  #define DebugBegin(...)({ USBSerial.begin(__VA_ARGS__); })
  #define Debug(...)      ({ if (!bHideP1Log ) TelnetStream.print(__VA_ARGS__); })
  #define Debugln(...)    ({ if (!bHideP1Log ) TelnetStream.println(__VA_ARGS__); })
  #define Debugf(...)     ({ if (!bHideP1Log ) TelnetStream.printf(__VA_ARGS__); })
  #define DebugFlush()    ({ TelnetStream.flush(); })
  #define USBPrint(...)   ({ if (USBSerial.isConnected() && USBSerial.isPlugged() ) USBSerial.print(__VA_ARGS__);})
  #define USBPrintf(...)  ({ if (USBSerial.isConnected() && USBSerial.isPlugged() ) USBSerial.printf(__VA_ARGS__);})
  #define USBPrintln(...) ({ if (USBSerial.isConnected() && USBSerial.isPlugged() ) USBSerial.println(__VA_ARGS__);})

#endif

#define DebugT(...)     ({ _debugBOL(__FUNCTION__, __LINE__);  \
                           Debug(__VA_ARGS__);                 \
                        })
                        
#define DebugTln(...)   ({ _debugBOL(__FUNCTION__, __LINE__);  \
                           Debugln(__VA_ARGS__);        \
                        })
#define DebugTf(...)    ({ _debugBOL(__FUNCTION__, __LINE__);  \
                           Debugf(__VA_ARGS__);                \
                        })

/*---- einde macro's ------------------------------------------------------------------*/

char _bol[128];
void _debugBOL(const char *fn, int line)
{
   
  snprintf(_bol, sizeof(_bol), "%1d [%02d:%02d:%02d][%7u|%6u] %-12.12s(%4d): ", \
                xPortGetCoreID(), hour(), minute(), second(), \
                ESP.getFreeHeap(), ESP.getMaxAllocHeap(),\
                fn, line);
                 
#ifdef DEBUG
  USBSerial.print (_bol);
#endif  
  if ( !bHideP1Log ) TelnetStream.print (_bol);
}

bool HWMarks[3] = { 0,0,0 };

void PrintHWMark(const int id){
#ifdef XTRA_LOG
      if (uptime() % 10 == 0) 
      {
        if ( !HWMarks[id] ) { 
          Debugf("--HighWaterMark %s: %d - heap: %d\n", pcTaskGetName(NULL), uxTaskGetStackHighWaterMark(NULL),ESP.getFreeHeap());
          HWMarks[id]=true;
        } 
      } else HWMarks[id]=false; //every minut
#endif
}

#endif
