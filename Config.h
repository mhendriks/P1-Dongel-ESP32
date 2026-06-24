// Device Types
#define APIURL              "http://api.smart-stuff.nl/v1/register.php"
#define LED_ON              LOW
#define LED_OFF             HIGH
#define SETTINGS_FILE       "/DSMRsettings.json"
#define HOST_DATA_FILES     "cdn.jsdelivr.net"
// #define PATH_DATA_FILES     "https://cdn.jsdelivr.net/gh/mhendriks/P1-Dongel-ESP32@latest/data"
#define PATH_DATA_FILES     "https://cdn.jsdelivr.net/gh/mhendriks/P1-Dongel-ESP32@" P1_STR(_VERSION_MAJOR) "." P1_STR(_VERSION_MINOR) "/data"
#define URL_INDEX_FALLBACK  "https://cdn.jsdelivr.net/gh/mhendriks/P1-Dongel-ESP32@latest/data"

#ifndef ENABLE_MIMICS
  #define ENABLE_MIMICS 1
#endif

#ifndef ENABLE_CRASH_BREADCRUMBS
  #define ENABLE_CRASH_BREADCRUMBS 0
#endif
#ifndef ENABLE_CRASH_COREDUMP_SUMMARY
  #define ENABLE_CRASH_COREDUMP_SUMMARY ENABLE_CRASH_BREADCRUMBS
#endif

// Direct AP closed-network mode.
// Optional local customer settings can be placed in ../../_secrets/direct_ap.h.
#if __has_include("./../../_secrets/direct_ap.h")
  #include "./../../_secrets/direct_ap.h"
#endif

#ifndef DIRECT_AP_CONNECT
  #define DIRECT_AP_CONNECT 0
#endif
#ifndef DIRECT_AP_SSID_PREFIX
  #define DIRECT_AP_SSID_PREFIX ""
#endif
#ifndef DIRECT_AP_TARGET_SERIAL
  #define DIRECT_AP_TARGET_SERIAL ""
#endif
#ifndef DIRECT_AP_CONNECT_TIMEOUT_MS
  #define DIRECT_AP_CONNECT_TIMEOUT_MS 45000
#endif
#ifndef DIRECT_AP_SCAN_INTERVAL_MS
  #define DIRECT_AP_SCAN_INTERVAL_MS 5000
#endif
#ifndef DIRECT_AP_ENABLE_LOCAL_LOGS
  #define DIRECT_AP_ENABLE_LOCAL_LOGS 0
#endif
#ifndef DIRECT_AP_OTAURL_PREFIX
  #define DIRECT_AP_OTAURL_PREFIX "direct-ap/"
#endif

//LED STUFF
#define LED_BLUE  0x07
#define LED_RED   0x070000
#define LED_GREEN 0x0700
#define LED_BLACK 0x0  
uint32_t R_value = 0, B_value = 0, G_value = 0;

//PROFILES
#include "profile.h"

#define WDT_FEED() do { esp_task_wdt_reset(); delay(0); } while(0)

// config MODULES
struct mod_io {
    int8_t sense1, sense2, mb_rx, mb_tx, mb_rts, wtr_s0, in, out;
};

struct mod_conf {
    int8_t mod_ports;
    mod_io io_conf[2];
};

mod_conf module_config[] = {
//       S1  S2  RX  TX  RTS WTR IN OUT      S1  S2  RX  TX  RTS WTR IN OUT
 { 1, {{  4, 21,  5,  7,  6,  6, -1, -1 }, { -1, -1, -1, -1, -1, -1, -1, -1 }} }, /* NRGD     */
 { 2, {{ 42, 45, 41, 44, 43, 43, -1, -1 }, { 37, 40, 36, 39, 38, 38, -1, -1 }} }, /* ULTRA V2 */
 { 2, {{ 37, 40, 36, 39, 38, 38, -1, -1 }, { 42, 45, 41, 44, 43, 43, -1, -1 }} },  /* ULTRA X2 */
 { 1, {{ -1, -1, 10,  0,  1,  4, -1, -1 }, { 42, 45, 41, 44, 43, 43, -1, -1 }} }  /* D1MC */
};

struct dev_conf {
    const char* default_hostname;

    int8_t button;
    int8_t rgb;
    int8_t led;
    int8_t water;

    int8_t p1_in_rx;
    int8_t p1_in_dtr;
    int8_t han_io;
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

// enum HWtype { UNDETECTED, P1P, NRGD, P1E, P1EP, P1UM, P1U, NRGM, P1S, P1UX2, NRGDH, D1MC };

dev_conf device_config[] = {
  // -- GENERAL --  ----------- P1/HAN ------------  -------- ETH --------- 
  { _DEFAULT_HOSTNAME,-1, -1, -1, -1,  -1, -1, -1,  -1, -1, -1,  -1, -1, -1, -1, -1, -1 }, // UNDETECTED
  { "P1-Dongle-Pro",   9, -1,  7,  5,  10,  6, -1,  -1,  1,  0,  -1, -1, -1, -1, -1, -1 }, // P1P (ESP32C3 - P1 Dongle Pro)
  { "NRG-Dongle-Pro",  9, -1,  3, -1,  20, -1, -1,  10,  1,  0,  -1, -1, -1, -1, -1, -1 }, // NRGD (ESP32C3 - NRG Dongle Pro)
  { "Eth-Dongle-Pro",  9,  8,  3, -1,   7,  0, -1,  -1, -1, -1,   1,  5,  6,  4, 10, -1 }, // P1E (ETH)
  { "Eth-Dongle-Pro",  9, -1,  8, -1,   0, -1, -1,   7,  3, -1,   6,  4,  5, 10,  1, -1 }, // P1EP (ETH)
  { "Ultra-Dongle",    0, 42, -1, 46,  18, -1, -1,  21, 15, 16,  14, 13, 11, 12, 10, -1 }, // P1UM (Ultra V1 and Mini)
  { "Ultra-Dongle",    0,  9, -1, -1,  18, 17, -1,  21, 15, 16,  14, 13, 11, 12, 10, -1 }, // P1U V2 (Ultra V2)
  { _DEFAULT_HOSTNAME,-1, -1, -1, -1,  -1, -1, -1,  -1, -1, -1,  -1, -1, -1, -1, -1, -1 }, // NRGM (onbekend in jouw snippets -> alles -1)
  { _DEFAULT_HOSTNAME,-1, -1, -1, -1,  -1, -1, -1,  -1, -1, -1,  -1, -1, -1, -1, -1, -1 }, // P1S (Splitter Pro)
  { "Ultra-Dongle",    0,  9, -1, -1,  12, -1, -1,  21, 10, 11,  18, 15, 16, 14, 13, 17 }, // P1UX2 (Ultra X2 - aangepaste ETH pinout)
  { "nrg-gateway",     9,  8, -1, -1,  20, -1, 10,   4,  1, -1,  -1, -1, -1, -1, -1, -1 }, // NRGDH
  { "nrg-gateway",     9,  8, -1, -1,   7, -1,  5,  -1, -1, -1,  -1, -1, -1, -1, -1, -1 }, // D1MC
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

#ifdef HAN_READER
  #define OPT3A "[HAN]"
#else
  #define OPT3A
#endif

#ifdef MBUS
  #define OPT4  "[MODBUS]"
#else
  #define OPT4  
#endif

#ifdef UDP_BCAST
  #define OPT5  "[UDP]"
#else
  #define OPT5  
#endif

#define OPT6

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

#define ALL_OPTIONS PROFILE OPT1 OPT2 OPT3 OPT3A OPT4 OPT5 OPT6 OPT7 OPT8
#define _DEFAULT_MQTT_TOPIC _DEFAULT_HOSTNAME "/"
#define _DEFAULT_HOMEPAGE  "/DSMRindexEDGE.html"
