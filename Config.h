#pragma once

// Device Types
#define PRO         0
#define PRO_BRIDGE  1
#define PRO_ETH     2
#define PRO_H20_B   3
#define PRO_H20_2   4

#define APIURL              "http://api.smart-stuff.nl/v1/register.php"
// #define LED_ON              0
// #define LED_OFF             1
#define SETTINGS_FILE       "/DSMRsettings.json"
#define HOST_DATA_FILES     "cdn.jsdelivr.net"
#define PATH_DATA_FILES     "https://cdn.jsdelivr.net/gh/mhendriks/P1-Dongel-ESP32@latest/data"

//LED STUFF
#define LED_BLUE  0x07
#define LED_RED   0x070000
#define LED_GREEN 0x0700
#define LED_BLACK 0x0  
uint32_t R_value = 0, B_value = 0, G_value = 0;

//PROFILES
#ifdef ULTRA
  #ifndef ETHERNET 
      #define ETHERNET
  #endif    
  #define VIRTUAL_P1
  #define MBUS
  #define ESPNOW
  #include "hw_profile_ultra.h"
#else
  #ifdef ETHERNET
    #include "hw_profile_p1_eth.h"
  #else
    #include "hw_profile_p1_pro.h"
  #endif
#endif

//FETAURES
#ifdef EID
  bool    bEID_enabled = true;
  enum E_eid_states : uint8_t { EID_IDLE, EID_CLAIMING, EID_ENROLLED };
  #undef MQTT_DISABLE
  #define MQTT_DISABLE
#endif //EID

#ifdef DEBUG
  #define OPT1  "[DEBUG]"
#else
  #define OPT1
#endif

#ifdef MBUS
  #define OPT2  "[MODBUS]"
#else
  #define OPT2  
#endif
 
#ifdef ESPNOW
  #define OPT3  "[ESPNOW]"  
#else
  #define OPT3  
#endif

#ifdef EID
  #define OPT4  "[EID]" 
#else
  #define OPT4
#endif

#ifdef POST_TELEGRAM
  #define OPT5  "[POST]" 
#else
  #define OPT5
#endif

#ifdef SMQTT
  #define OPT6  "[SMQTT]" 
#else
  #define OPT6
#endif

#ifdef SE_VERSION
  #define OPT7 "[SE]"
#else 
  #define OPT7
#endif

#ifdef VIRTUAL_P1
  #define OPT8  "[VIRT_P1]" 
#else
  #define OPT8
#endif

#define ALL_OPTIONS PROFILE OPT1 OPT2 OPT3 OPT4 OPT5 OPT6 OPT7 OPT8
#define _DEFAULT_MQTT_TOPIC _DEFAULT_HOSTNAME "/"

#ifdef AP_ONLY  
  #define _DEFAULT_HOMEPAGE  "/DSMRindexLOCAL.html"
#else
  #define _DEFAULT_HOMEPAGE  "/DSMRindexEDGE.html"
#endif
