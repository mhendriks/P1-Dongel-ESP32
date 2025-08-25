// Device Types
#define PRO         0
#define PRO_BRIDGE  1
#define PRO_ETH     2
#define PRO_H20_B   3
#define PRO_H20_2   4
#define _P1EP       5
#define P1NRG       6

#define APIURL              "http://api.smart-stuff.nl/v1/register.php"
#define LED_ON              LOW
#define LED_OFF             HIGH
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
  // #ifndef ETHERNET 
  //     #define ETHERNET
  // #endif    
  // #define VIRTUAL_P1
  // #define NETSWITCH
  // #define MBUS //default
  // #define DEV_PAIRING //default
  #include "hw_profile_ultra.h"
  #undef DEV_PAIRING // 5.1 feature is ESPNOW
#else
  #ifdef ETHERNET
    #ifdef __Az__
      #ifdef ETH_P1EP
        #include "hw_profile_p1ep_az.h"
      #else
        #include "hw_profile_p1_eth_az.h"
      #endif
    #else 
      #define NETSWITCH
      #ifdef ETH_P1EP
        #define VIRTUAL_P1
        #include "hw_profile_p1ep.h"
      #else 
        #include "hw_profile_p1_eth.h"
      #endif  
    #endif
  #else
      #ifdef NRG_DONGLE
        #include "hw_profile_nrgd.h"
      #else
        #include "hw_profile_p1_pro.h"
      #endif
      #ifdef __Az__
        #include "hw_az.h"
      #endif
  #endif
#endif

// config MODULES
struct mod_io {
    int8_t sense1, sense2, mb_rx, mb_tx, mb_rts, wtr_s0, in, out;
};

struct mod_conf {
    int8_t mod_ports;
    mod_io io_conf[2];
};

mod_conf module_config[2] = {
 { 1, {{  4, 21,  5,  7,  6,  6, -1, -1 }, { -1, -1, -1, -1, -1, -1, -1, -1 }} }, /* NRGD     */
 { 2, {{ 42, 45, 41, 44, 43, 43, -1, -1 }, { 37, 40, 36, 39, 38, 38, -1, -1 }} }  /* ULTRA V2 */
};

int8_t modType[2] = {-1,-1};
mod_conf *active_mod_conf = &module_config[0];

//FETAURES
#ifdef ULTRA
  bool    bEID_enabled = true;
#else
  bool    bEID_enabled = false;
#endif  
  enum E_eid_states : uint8_t { EID_IDLE, EID_CLAIMING, EID_ENROLLED };

#ifdef DEBUG
  #define OPT1  "[DEBUG]"
#else
  #define OPT1
#endif

#define OPT2 ""
#define OPT3 "" 

#ifdef MBUS
  #define OPT4  "[MODBUS]"
#else
  #define OPT4  
#endif
 
#ifdef DEV_PAIRING
  #define OPT5  "[PAIR]"  
#else
  #define OPT5  
#endif

#ifdef POST_TELEGRAM
  #define OPT6  "[POST]" 
#else
  #define OPT6
#endif

#ifdef NETSWITCH 
 #define OPT7 "[NETSW]"
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
#define _DEFAULT_HOMEPAGE  "/DSMRindexEDGE.html"
