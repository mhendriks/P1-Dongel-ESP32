/* 
***************************************************************************  
**  Program  : Debug.h, part of DSMRloggerAPI
**  Version  : v4.2.1
**
**  Copyright (c) 2022 Martijn Hendriks / based on DSMR Api Willem Aandewiel
**  Met dank aan Erik
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

/*---- start macro's ------------------------------------------------------------------*/

#define Debug(...)      ({ SerialOut.print(__VA_ARGS__);         \
                           TelnetStream.print(__VA_ARGS__);   \
                        })
#define Debugln(...)    ({ SerialOut.println(__VA_ARGS__);       \
                           TelnetStream.println(__VA_ARGS__); \
                        })
#define Debugf(...)     ({ SerialOut.printf(__VA_ARGS__);        \
                           TelnetStream.printf(__VA_ARGS__);  \
                        })

#define DebugFlush()    ({ SerialOut.flush(); \
                           TelnetStream.flush(); \
                        })


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

// needs #include <TelnetStream.h>       // Version 0.0.1 - https://github.com/jandrassy/TelnetStream

char _bol[128];
void _debugBOL(const char *fn, int line)
{
   
  snprintf(_bol, sizeof(_bol), "%1d [%02d:%02d:%02d][%7u|%6u] %-12.12s(%4d): ", \
                xPortGetCoreID(), hour(), minute(), second(), \
                ESP.getFreeHeap(), ESP.getMaxAllocHeap(),\
                fn, line);
                 
  SerialOut.print (_bol);
  TelnetStream.print (_bol);
}
