// Board config
#ifndef ESP32
  #error This code is intended to run on ESP32 platform! Please check your Tools->Board setting.
#endif

// Device Types
#define PRO         0
#define PRO_BRIDGE  1
#define PRO_ETH     2
#define PRO_H20_B   3

#define BASE_OPTIONS "[CORE]"
//#define SerialOut           Serial //normal use USB_CDC_ON_BOOT = Disabled
#define SerialOut           USBSerial //use USB_CDC_ON_BOOT = Enabled --> log to CDC
 
#define LED_ON              LOW
#define LED_OFF             HIGH
  
#ifdef ULTRA 
  #include "hw_profile_ultra.h"
#elif ETHERNET
  #include "hw_profile_p1_eth.h"
#else
  #include "hw_profile_p1_pro.h"
#endif

#ifdef HEATLINK
  #define ALL_OPTIONS BASE_OPTIONS "[Q]"
  
  #undef _DEFAULT_HOSTNAME
  #define _DEFAULT_HOSTNAME   "Q-Dongle-Pro" 
  
  #undef OTAURL
  #define OTAURL              "http://ota.smart-stuff.nl/v5-q/"
#endif  

#ifdef EID

  bool    bEID_enabled = false;
  #define ALL_OPTIONS BASE_OPTIONS "[EnergyID]"
  #undef MQTT_DISABLE
  #define MQTT_DISABLE
  #undef OTAURL
  #define OTAURL "http://ota.smart-stuff.nl/v5-eid/"
  enum E_eid_states : uint8_t { EID_IDLE, EID_CLAIMING, EID_ENROLLED };

#endif

#ifndef ALL_OPTIONS
 #define ALL_OPTIONS BASE_OPTIONS
#endif

#define _DEFAULT_MQTT_TOPIC _DEFAULT_HOSTNAME "/"

//LED STUFF
#define LED_BLUE  0x07
#define LED_RED   0x070000
#define LED_GREEN 0x0700
#define LED_BLACK 0x0  
uint32_t R_value = 0, B_value = 0, G_value = 0;

#ifdef AP_ONLY  
  #define _DEFAULT_HOMEPAGE  "/DSMRindexLOCAL.html"
#else
  #define _DEFAULT_HOMEPAGE  "/DSMRindexEDGE.html"
#endif
#define SETTINGS_FILE      "/DSMRsettings.json"
#define HOST_DATA_FILES    "cdn.jsdelivr.net"
#define PATH_DATA_FILES    "https://cdn.jsdelivr.net/gh/mhendriks/P1-Dongel-ESP32@latest/data"
