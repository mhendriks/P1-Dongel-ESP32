// Boardsconfig

// Device Types
#define PRO         0
#define PRO_BRIDGE  1
#define PRO_ETH     2
#define PRO_H20_B   3
#define PRO_H20_2   4

#define APIURL "http://api.smart-stuff.nl/v1/register.php"
#define SETTINGS_FILE      "/DSMRsettings.json"
#define HOST_DATA_FILES    "cdn.jsdelivr.net"
#define PATH_DATA_FILES    "https://cdn.jsdelivr.net/gh/mhendriks/P1-Dongel-ESP32@latest/data"
#define MBUS_TYPE 3
#define LED_ON              LOW
#define LED_OFF             HIGH

#ifdef ETHERNET
  #include "hw_profile_p1_eth.h"
#elif defined(ULTRA) 
  #include "hw_profile_ultra.h"
#else
 #include "hw_profile_p1_pro.h"
#endif //ethernet

#ifdef HEATLINK
  #define ALL_OPTIONS PROFILE "[Q]"

  #undef MBUS_TYPE
  #define MBUS_TYPE 4

  #undef _DEFAULT_HOSTNAME
  #define _DEFAULT_HOSTNAME   "Q-Dongle-Pro" 
  
  #undef OTAURL
  #define OTAURL              "http://ota.smart-stuff.nl/v5-q/"
#endif  

#ifdef EID
  #define ALL_OPTIONS PROFILE "[EnergyID]"
  #undef MQTT_DISABLE
  #define MQTT_DISABLE
  enum E_eid_states : uint8_t { EID_IDLE, EID_CLAIMING, EID_ENROLLED };
  bool    bEID_enabled = false;
#endif

#ifdef SE_VERSION
  #define ALL_OPTIONS PROFILE "[SE]"
#endif

#ifndef ALL_OPTIONS
 #define ALL_OPTIONS PROFILE
#endif

#define _DEFAULT_MQTT_TOPIC _DEFAULT_HOSTNAME "/"
 
#ifdef AP_ONLY  
  #define _DEFAULT_HOMEPAGE  "/DSMRindexLOCAL.html"
#else
  #define _DEFAULT_HOMEPAGE  "/DSMRindexEDGE.html"
#endif
