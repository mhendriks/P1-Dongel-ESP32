// Device Types
#define APIURL              "http://api.smart-stuff.nl/v1/register.php"
#define LED_ON              LOW
#define LED_OFF             HIGH
#define SETTINGS_FILE       "/DSMRsettings.json"
#define HOST_DATA_FILES     "cdn.jsdelivr.net"
// #define PATH_DATA_FILES     "https://cdn.jsdelivr.net/gh/mhendriks/P1-Dongel-ESP32@latest/data"
#define PATH_DATA_FILES     "https://cdn.jsdelivr.net/gh/mhendriks/P1-Dongel-ESP32@" STR(_VERSION_MAJOR) "." STR(_VERSION_MINOR) "/data"
#define URL_INDEX_FALLBACK  "https://cdn.jsdelivr.net/gh/mhendriks/P1-Dongel-ESP32@latest/data"

//LED STUFF
#define LED_BLUE  0x07
#define LED_RED   0x070000
#define LED_GREEN 0x0700
#define LED_BLACK 0x0  
uint32_t R_value = 0, B_value = 0, G_value = 0;

//PROFILES
#ifdef ULTRA
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

#define WDT_FEED() do { esp_task_wdt_reset(); delay(0); } while(0)

// config MODULES
struct mod_io {
    int8_t sense1, sense2, mb_rx, mb_tx, mb_rts, wtr_s0, in, out;
};

struct mod_conf {
    int8_t mod_ports;
    mod_io io_conf[2];
};

mod_conf module_config[3] = {
//       S1  S2  RX  TX  RTS WTR IN OUT      S1  S2  RX  TX  RTS WTR IN OUT
 { 1, {{  4, 21,  5,  7,  6,  6, -1, -1 }, { -1, -1, -1, -1, -1, -1, -1, -1 }} }, /* NRGD     */
 { 2, {{ 42, 45, 41, 44, 43, 43, -1, -1 }, { 37, 40, 36, 39, 38, 38, -1, -1 }} }, /* ULTRA V2 */
 { 2, {{ 37, 40, 36, 39, 38, 38, -1, -1 }, { 42, 45, 41, 44, 43, 43, -1, -1 }} }  /* ULTRA X2 */
};

struct dev_conf {
    int8_t button;
    int8_t rgb;
    int8_t led;
    int8_t water;

    int8_t p1_in_rx;
    int8_t p1_in_dtr;
    int8_t p1_out_tx;
    int8_t p1_out_dtr;
    int8_t p1_out_led;

    int8_t eth_int;
    int8_t eth_miso;
    int8_t eth_mosi;
    int8_t eth_sck;
    int8_t eth_cs;
    int8_t eth_rst;
};

// enum HWtype { UNDETECTED, P1P, NRGD, P1E, P1EP, P1UM, P1U, NRGM, P1S, P1UX2 };

dev_conf device_config[] = {

  // UNDETECTED
  // -- GENERAL --  -------- P1 ---------  -------- ETH ---------
  { -1, -1, -1, -1,  -1, -1,  -1, -1, -1,  -1, -1, -1, -1, -1, -1 },

  // P1P (ESP32C3 - P1 Dongle Pro)
  {  9,  8,  7,  5,  10,  6,  -1,  1,  0,  -1, -1, -1, -1, -1, -1 },

  // NRGD (ESP32C3 - NRG Dongle Pro)
  {  9, -1,  3, -1,  20, -1,  10,  1,  0,  -1, -1, -1, -1, -1, -1 },

  // P1E (ETH)
  {  9,  8,  3, -1,   7,  0,  -1, -1, -1,   1,  5,  6,  4, 10, -1 },

  // P1EP (ETH)
  {  9, -1,  8, -1,   0, -1,   7,  3, -1,   6,  4,  5, 10,  1, -1 },

  // P1UM (Ultra V1 and Mini)
  {  0, 42, -1, 46,  18, -1,  21, 15, 16,  14, 13, 11, 12, 10, -1 },

  // P1U V2 (Ultra V2)
  {  0,  9, -1, -1,  18, 17,  21, 15, 16,  14, 13, 11, 12, 10, -1 },

  // NRGM (onbekend in jouw snippets → alles -1)
  { -1, -1, -1, -1,  -1, -1,  -1, -1, -1,  -1, -1, -1, -1, -1, -1 },

  // P1S (Splitter Pro)
  { -1, -1, -1, -1,  -1, -1,  -1, -1, -1,  -1, -1, -1, -1, -1, -1 },

  // P1UX2 (Ultra X2 – aangepaste ETH pinout)
  {  0, 9, -1, -1,  12, -1,  21, 10, 11, 18, 15, 16, 14, 13, 17 },
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
