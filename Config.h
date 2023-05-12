// Device Types
#define PRO         0
#define PRO_BRIDGE  1
#define PRO_ETH     2
#define PRO_H20_B   3

#define BASE_OPTIONS "[CORE]"
 
#ifdef ESP32
  #define MBUS_TYPE 3
  #define _DEFAULT_HOSTNAME   "P1-Dongle-Pro"
  
#ifdef ARDUINO_ESP32C3_DEV
  #define OTAURL              "http://ota.smart-stuff.nl/v5/"
  #define AUX_BUTTON          9 //download knop bij startup - multifunctional tijdens runtime
  #define LED_ON              LOW
  #define LED_OFF             HIGH
  #define SerialOut           Serial //normal use USB_CDC_ON_BOOT = Disabled
  //#define SerialOut           USBSerial //use USB_CDC_ON_BOOT = Enabled --> log to CDC
  volatile unsigned long      Tpressed = 0;
  volatile byte               pressed = 0;
#ifdef ETHERNET
    #warning Using ESP32C3-ETHERNET
    #define ALL_OPTIONS BASE_OPTIONS "[ETHERNET]"
    #define LED                 3 // n/a
    #define DTR_IO              0 // n/a not used poort / -1 = error
    #define RXP1                7
    #define TXP1                -1 //no txp1
    #define IO_WATER_SENSOR     -1 // n/a  
    #undef OTAURL
    #define OTAURL              "http://ota.smart-stuff.nl/eth/"
    #undef _DEFAULT_HOSTNAME
    #define _DEFAULT_HOSTNAME   "Eth-Dongle-Pro" 
#else
    #warning Using ESP32C3
    #define LED                 7
    #define DTR_IO              4 // nr = IO pulse = N/A
    #define RXP1                10
    #define TXP1               -1 //disable
    #define IO_WATER_SENSOR     5
#endif //ethernet
#endif //ARDUINO_ESP32C3_DEV
#else
  #error This code is intended to run on ESP32 platform! Please check your Tools->Board setting.
#endif //ESP32

#ifdef HEATLINK
  #define ALL_OPTIONS BASE_OPTIONS "[Q]"

  #undef MBUS_TYPE
  #define MBUS_TYPE 3
  
  #undef _DEFAULT_HOSTNAME
  #define _DEFAULT_HOSTNAME   "Q-Dongle-Pro" 
  
  #undef OTAURL
  #define OTAURL              "http://ota.smart-stuff.nl/v5-q/"
#endif  

#ifdef SE_VERSION
  #define ALL_OPTIONS BASE_OPTIONS "[SE]"
#endif

#ifndef ALL_OPTIONS
 #define ALL_OPTIONS BASE_OPTIONS
#endif

  #define _DEFAULT_MQTT_TOPIC _DEFAULT_HOSTNAME "/"

  #define RGBLED_PIN        8
  #define PRT_LED           0
  #include <Adafruit_NeoPixel.h>
  Adafruit_NeoPixel RGBLED(1, RGBLED_PIN, NEO_GRB + NEO_KHZ800);
  #define BRIGHTNESS 50 // Set BRIGHTNESS to about 1/5 (max = 255)
  //LED COLORS
  #define BLUE  3
  #define RED   1
  #define GREEN 2
  byte R_value = 0, B_value = 0, G_value = 0;

#ifdef AP_ONLY  
  #define _DEFAULT_HOMEPAGE  "/DSMRindexLOCAL.html"
#else
  #define _DEFAULT_HOMEPAGE  "/DSMRindexEDGE.html"
#endif
#define SETTINGS_FILE      "/DSMRsettings.json"
#define HOST_DATA_FILES    "cdn.jsdelivr.net"
#define PATH_DATA_FILES    "https://cdn.jsdelivr.net/gh/mhendriks/P1-Dongel-ESP32@latest/data"
