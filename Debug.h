#ifndef _DEBUG_H
#define _DEBUG_H
/* 
***************************************************************************  
**  Program  : Debug.h, part of DSMRloggerAPI
**
**  Copyright (c) 2023 Martijn Hendriks / based on DSMR Api Willem Aandewiel
**  Met dank aan Erik
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

/*---- start macro's ------------------------------------------------------------------*/
#ifdef DEBUG
  //DEBUG MODE
  #define Debug(...)      ({ USBSerial.print(__VA_ARGS__);\
                             TelnetStream.print(__VA_ARGS__);\
                          })
  #define Debugln(...)    ({ USBSerial.println(__VA_ARGS__); \
                             TelnetStream.println(__VA_ARGS__); \
                          })
  #define Debugf(...)     ({ USBSerial.printf(__VA_ARGS__); \
                             TelnetStream.printf(__VA_ARGS__); \
                          })
  
  #define DebugFlush()    ({ USBSerial.flush(); \
                             TelnetStream.flush(); \
                          })

  #define USBPrint(...)   ({ Debug(__VA_ARGS__);})
  #define USBPrintf(...)  ({ Debugf(__VA_ARGS__);})
  #define USBPrintln(...) ({ Debugln(__VA_ARGS__);})                       

#else
  //NORMAL MODE
  #define Debug(...)      ({ TelnetStream.print(__VA_ARGS__); })
  #define Debugln(...)    ({ TelnetStream.println(__VA_ARGS__); })
  #define Debugf(...)     ({ TelnetStream.printf(__VA_ARGS__); })
  #define DebugFlush()    ({ TelnetStream.flush(); })
  #define USBPrint(...)   ({ if (USBSerial.isConnected() && USBSerial.isPlugged() ) USBSerial.print(__VA_ARGS__);})
  #define USBPrintf(...)  ({ if (USBSerial.isConnected() && USBSerial.isPlugged() ) USBSerial.printf(__VA_ARGS__);})
  #define USBPrintln(...) ({ if (USBSerial.isConnected() && USBSerial.isPlugged() ) USBSerial.println(__VA_ARGS__);})

#endif

#define DebugBegin(...)({ USBSerial.begin(__VA_ARGS__);while (!USBSerial); })
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
  TelnetStream.print (_bol);
}

#endif
