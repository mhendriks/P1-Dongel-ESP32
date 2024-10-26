    /*
***************************************************************************  
**  Program  : DSMRloggerAPI.h - definitions for DSMRloggerAPI
**
**  Copyright (c) 2023 Martijn Hendriks / based on DSMR Api Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************     
*/

#ifndef _DSMRAPI_H
#define _DSMRAPI_H

#include "Config.h"

#if ARDUINO_USB_CDC_ON_BOOT
  #define USBSerial HWCDCSerial
#else
  HWCDC USBSerial;
#endif

// water sensor
volatile byte        WtrFactor      = 1;
volatile time_t      WtrTimeBetween = 0;
byte                 debounces      = 0;
volatile time_t      WtrPrevReading = 0;
bool                 WtrMtr         = false;
#define              DEBOUNCETIMER 1700

#include <WiFi.h>
#include <WiFiClientSecure.h>  
#include <WebServer.h>
#include <TimeLib.h>            // https://github.com/PaulStoffregen/Time
#include <TelnetStream.h>       // https://github.com/jandrassy/TelnetStream
#include "safeTimers.h"
#include "vers.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <dsmr2.h>               // https://github.com/mhendriks/dsmr2Lib
#include <esp_now.h>             //https://randomnerdtutorials.com/esp-now-auto-pairing-esp32-esp8266/
#include "esp_chip_info.h"
#include "rgb.h"

#ifdef MBUS
#include "ModbusServerWiFi.h"
#endif

#define JSON_BUFF_MAX     255
#define MQTT_BUFF_MAX     1024
#define MQTT_RECONNECT_DEFAULT_TIME 10 //seconds

P1Reader    slimmeMeter(&Serial1, DTR_IO);

#ifdef VIRTUAL_P1
  char virtual_p1_ip[20] ="";
#endif

void LogFile(const char* payload, bool toDebug = false);
void P1Reboot();
void SwitchLED( byte mode, uint32_t color);
String MAC_Address();
String  IP_Address();

WebServer httpServer(80);
NetServer ws_raw(82);

time_t tWifiLost        = 0;
byte  WifiReconnect     = 0;

TaskHandle_t tP1Reader; //  own proces for P1 reading

enum  { PERIOD_UNKNOWN, HOURS, DAYS, MONTHS, YEARS };
enum  E_ringfiletype {RINGHOURS, RINGDAYS, RINGMONTHS, RINGVOLTAGE};
enum  SolarSource { ENPHASE, SOLAR_EDGE };

// connect NRG Monitor via ESPNOW
bool bPairingmode = false;

enum  { PEER_PARING, PEER_ACTUALS, PEER_TARIFS, PEER_DEVICE, PEER_WIFI };

struct {
  // uint32_t  reboots;
  uint8_t   mac[6];
  uint8_t   peers;      
} Pref;

typedef struct { 
  uint32_t lastUpdDay;
  uint32_t t1;
  uint32_t t2;
  uint32_t t1r;
  uint32_t t2r;
  uint32_t gas; //and heat
  uint32_t water;
} sUsage;

typedef struct {
  uint8_t   type;       // PEER_DEVICE
  uint8_t   ver_major;
  uint8_t   ver_minor;
  uint8_t   ver_build;
} sDevice;

typedef struct {
  uint8_t   type;      // PEER_ACTUALS
  time_t    epoch;      // sec from 1/1/1970
  uint32_t  e_in;      // Wh
  uint32_t  e_out;     // Wh
  uint32_t  Power;     // W
  uint32_t  Gas;       // dm3/Liter
  uint32_t  Water;     // dm3/Liter
  uint32_t  Production; // W  
} sActual;

typedef struct {
    char filename[17];
    uint16_t slots;
    unsigned int seconds;
    int f_len;
  } S_ringfile;

sUsage dataYesterday;

#ifdef VOLTAGE_MON
//Store over Voltage situations
#define     VmaxSlots 100
//                       ts       ,L1 ,L2 ,L3 ,MaxV, Overß
#define DATA_FORMAT_V    "%-12.12s,%4d,%4d,%4d,%3d,%c"
#define DATA_RECLEN_V    34  //total length incl comma and new line

//sMaxV consist of 12 + 3x2 + 2 = 20 bytes
struct sMaxV {
  char      ts[12];
  uint16_t     L1;
  uint16_t     L2;
  uint16_t     L3;
  uint16_t     MaxV;
  char         Over;
  } MaxVarr[VmaxSlots]; //50 x 20 = 1.000 bytes 

//Voltage
uint16_t    MaxVoltage = 253;
bool        bMaxV = false;
uint8_t     Vcount = 0;

#endif

#define JSON_HEADER_LEN   23  //total length incl new line
#define DATA_CLOSE        2   //length last row of datafile
#define DATA_FORMAT      "{\"date\":\"%-8.8s\",\"values\":[%10.3f,%10.3f,%10.3f,%10.3f,%10.3f,%10.3f]}"
#define DATA_RECLEN      98  //total length incl comma and new line

const S_ringfile RingFiles[3] = {{"/RNGhours.json", 48+1,SECS_PER_HOUR, 4826}, {"/RNGdays.json",14+1,SECS_PER_DAY, (14+1)*(DATA_RECLEN)+DATA_CLOSE+JSON_HEADER_LEN-1 },{"/RNGmonths.json",24+1,0,2474}}; //+1 voor de vergelijking, laatste record wordt niet getoond 

#include "Debug.h"

  /**
  * Define the DSMRdata we're interested in, as well as the DSMRdatastructure to
  * hold the parsed DSMRdata. This list shows all supported fields, remove
  * any fields you are not using from the below list to make the parsing
  * and printing code smaller.
  * Each template argument below results in a field of the same name.
  * 
  */
 
using MyData = ParsedData<
  /* String */                 identification
  /* String */                ,p1_version
  /* String */                ,p1_version_be
  /* FixedValue */            ,peak_pwr_last_q
  /* TimestampedFixedValue */ ,highest_peak_pwr
  /* String */                ,highest_peak_pwr_13mnd
  /* String */                ,timestamp
  /* String */                ,equipment_id
  /* FixedValue */            ,energy_delivered_tariff1
  /* FixedValue */            ,energy_delivered_tariff2
  /* FixedValue */            ,energy_returned_tariff1
  /* FixedValue */            ,energy_returned_tariff2
  /* FixedValue */            ,energy_returned_total
  /* String */                ,electricity_tariff
  /* FixedValue */            ,power_delivered
  /* FixedValue */            ,power_returned
//  /* FixedValue */            ,electricity_threshold
//  /* uint8_t */               ,electricity_switch_position
//  /* uint32_t */              ,electricity_failures
//  /* uint32_t */              ,electricity_long_failures
//  /* String */                ,electricity_failure_log
//  /* uint32_t */              ,electricity_sags_l1
//  /* uint32_t */              ,electricity_sags_l2
//  /* uint32_t */              ,electricity_sags_l3
//  /* uint32_t */              ,electricity_swells_l1
//  /* uint32_t */              ,electricity_swells_l2
//  /* uint32_t */              ,electricity_swells_l3
//  /* String */                ,message_short
//  /* String */                ,message_long
  /* FixedValue */            ,voltage_l1
  /* FixedValue */            ,voltage_l2
  /* FixedValue */            ,voltage_l3
  /* FixedValue */            ,current_l1
  /* FixedValue */            ,current_l2
  /* FixedValue */            ,current_l3
  /* FixedValue */            ,power_delivered_l1
  /* FixedValue */            ,power_delivered_l2
  /* FixedValue */            ,power_delivered_l3
  /* FixedValue */            ,power_returned_l1
  /* FixedValue */            ,power_returned_l2
  /* FixedValue */            ,power_returned_l3
  /* uint16_t */              ,mbus1_device_type
  /* String */                ,mbus1_equipment_id_tc
  /* String */                ,mbus1_equipment_id_ntc
  /* uint8_t */               ,mbus1_valve_position
  /* TimestampedFixedValue */ ,mbus1_delivered
  /* TimestampedFixedValue */ ,mbus1_delivered_ntc
  /* TimestampedFixedValue */ ,mbus1_delivered_dbl
  /* uint16_t */              ,mbus2_device_type
  /* String */                ,mbus2_equipment_id_tc
  /* String */                ,mbus2_equipment_id_ntc
  /* uint8_t */               ,mbus2_valve_position
  /* TimestampedFixedValue */ ,mbus2_delivered
  /* TimestampedFixedValue */ ,mbus2_delivered_ntc
  /* TimestampedFixedValue */ ,mbus2_delivered_dbl
  /* uint16_t */              ,mbus3_device_type
  /* String */                ,mbus3_equipment_id_tc
  /* String */                ,mbus3_equipment_id_ntc
  /* uint8_t */               ,mbus3_valve_position
  /* TimestampedFixedValue */ ,mbus3_delivered
  /* TimestampedFixedValue */ ,mbus3_delivered_ntc
  /* TimestampedFixedValue */ ,mbus3_delivered_dbl
  /* uint16_t */              ,mbus4_device_type
  /* String */                ,mbus4_equipment_id_tc
  /* String */                ,mbus4_equipment_id_ntc
  /* uint8_t */               ,mbus4_valve_position
  /* TimestampedFixedValue */ ,mbus4_delivered
  /* TimestampedFixedValue */ ,mbus4_delivered_ntc
  /* TimestampedFixedValue */ ,mbus4_delivered_dbl
>;

const PROGMEM char *flashMode[]    { "QIO", "QOUT", "DIO", "DOUT", "Unknown" };

//===========================prototype's=======================================
int strcicmp(const char *a, const char *b);
void delayms(unsigned long);
void SetConfig();

//===========================GLOBAL VAR'S======================================
#ifdef SMQTT
  WiFiClientSecure wifiClient;
#else
  WiFiClient wifiClient;
#endif
#ifndef MQTT_DISABLE 
  #include <PubSubClient.h>           // MQTT client publish and subscribe functionality
  PubSubClient MQTTclient(wifiClient);
#endif

//config + button
int8_t IOWater = 0;
bool UseRGB = false; 
volatile unsigned long      Tpressed = 0;
volatile bool bButtonPressed = false;

struct Status {
   uint32_t           reboots;
   uint32_t           sloterrors; //deprecated
   char               timestamp[14];
   uint32_t           wtr_m3;
   uint16_t           wtr_l;
   uint16_t           dev_type;   
   bool               FirstUse;
#ifdef EID   
   E_eid_states       eid_state;
#endif   
} P1Status;
  
MyData      DSMRdata;
struct tm   tm;
bool        DSTactive;
time_t      actT, newT;
char        actTimestamp[20] = "";
char        newTimestamp[20] = "";
uint32_t    telegramCount = 0, telegramErrors = 0, mqttCount = 0;
bool        showRaw = false;
bool        LEDenabled    = true;
bool        DSMR_NL       = true;
bool        EnableHAdiscovery = true;
bool        bHideP1Log = false;
char        bAuthUser[25]="", bAuthPW[25]="";
bool        EnableHistory = true;
bool        bPre40 = false;
bool        bWarmteLink = false;
bool        bActJsonMQTT = false;
bool        bRawPort = false;
bool        bLED_PRT = true;
bool        P1Out = false;
byte        RxP1 = RXP1;
//bool        bWriteFiles = false;

//vitals
char      macStr[18] = { 0 };
char      macID[13];

String      CapTelegram;
char        cMsg[150];
String      lastReset           = "";
bool        FSNotPopulated      = false;
bool        Verbose1 = false, Verbose2 = false;
bool        FSmounted           = false; 

uint32_t    unixTimestamp;

IPAddress ipDNS, ipGateWay, ipSubnet;
float     settingEDT1 = 0.1, settingEDT2 = 0.2, settingERT1 = 0.3, settingERT2 = 0.4, settingGDT = 0.5, settingWDT = 1.04;
float     settingENBK = 29.62, settingGNBK = 17.30,settingWNBK = 55.05;
uint8_t   settingSmHasFaseInfo = 1;
char      settingHostname[30] = _DEFAULT_HOSTNAME;
char      settingIndexPage[50] = _DEFAULT_HOMEPAGE;

//MQTT
#ifndef MQTTKB
  uint32_t   settingMQTTinterval = 0;
  char      settingMQTTbroker[101], settingMQTTuser[75], settingMQTTpasswd[160], settingMQTTtopTopic[50] = _DEFAULT_MQTT_TOPIC;
#else
  #define NO_HA_AUTODISCOVERY
  #include "_mqtt_kb.h"
  #define OTAURL "http://ota.smart-stuff.nl/p1e/kb/"
  uint32_t   settingMQTTinterval     = MQTT_INTERVAL;
  char      settingMQTTbroker[101]  = MQTT_BROKER;
  char      settingMQTTuser[75]     = MQTT_USER;
  char      settingMQTTpasswd[160]  = MQTT_PASSWD;
  char      settingMQTTtopTopic[50] = _DEFAULT_MQTT_TOPIC;
#endif 

//update
char      BaseOTAurl[45] = OTAURL;
char      UpdateVersion[25] = "";
bool      bUpdateSketch = true;
bool      bAutoUpdate = false;

uint32_t   settingMQTTbrokerPort = 1883;
float     gasDelivered;
String    gasDeliveredTimestamp;
bool      UpdateRequested = false;
byte      mbusGas = 0;
byte      mbusWater = 0;
float     waterDelivered;
String    waterDeliveredTimestamp;
String    mbusDeliveredTimestamp;
String    smID;
bool      StaticInfoSend = false;
bool      bSendMQTT = false;

//Post_Telegram
#ifdef POST_TELEGRAM
  time_t TelegramLastPost = 0;
  uint16_t pt_port = 80;
  uint16_t pt_interval = 60;
  char pt_end_point[60];
#endif

#include <ESPmDNS.h>        
#include <Update.h>
#include <WiFiManager.h>        // https://github.com/tzapu/WiFiManager
#include <HTTPClient.h>
#include "NetTypes.h"
#include "_Button.h"
// #include "Network.h"

//===========================================================================================
// setup timers 
DECLARE_TIMER_SEC(synchrNTP,          30);
DECLARE_TIMER_SEC(reconnectMQTTtimer,  MQTT_RECONNECT_DEFAULT_TIME); // try reconnecting cyclus timer
DECLARE_TIMER_SEC(publishMQTTtimer,   60, SKIP_MISSED_TICKS); // interval time between MQTT messages  
DECLARE_TIMER_MS(WaterTimer,          DEBOUNCETIMER);
DECLARE_TIMER_SEC(StatusTimer,        10); //first time = 10 sec usual 30min (see loop)

#endif
/***************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the
* following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
* 
***************************************************************************/
