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

//SDK 3.x.x
#if ARDUINO_USB_CDC_ON_BOOT
  #define USBSerial HWCDCSerial
#else
  HWCDC USBSerial;
#endif

#include "Config.h"

// water sensor
volatile byte        WtrFactor      = 1;
volatile time_t      WtrTimeBetween = 0;
volatile byte        debounces      = 0;
volatile time_t      WtrPrevReading = 0;
bool                 WtrMtr         = false;
#define              DEBOUNCETIMER 1700

struct {
  uint8_t map = 0;
  uint8_t id = 1;
  uint16_t baud = 9600;
  uint32_t parity = SERIAL_8E1;
  uint16_t port = 502;
} mb_config;

#include <WiFi.h>        
#include "Insights.h"
#include <WiFiClientSecure.h>        
#include <WebServer.h>
#include <TimeLib.h>            // https://github.com/PaulStoffregen/Time
#include <TelnetStream.h>       // https://github.com/jandrassy/TelnetStream
#include "safeTimers.h"
#include "version.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <dsmr2.h>               // https://github.com/mhendriks/dsmr2Lib
#include "esp_chip_info.h"
#include <esp_now.h>             //https://randomnerdtutorials.com/esp-now-auto-pairing-esp32-esp8266/

#ifdef MBUS
  #include "ModbusServerWiFi.h"
#endif

#define JSON_BUFF_MAX     255
#define MQTT_BUFF_MAX     1024
#define MQTT_RECONNECT_DEFAULT_TIME 10 //seconds

P1Reader    slimmeMeter(&Serial1, DTR_IO);

void LogFile(const char* payload, bool toDebug = false);
void P1Reboot();

WebServer httpServer(80);
NetServer ws_raw(82);

time_t tWifiLost        = 0;
byte  WifiReconnect     = 0;
IPAddress staticIP, gateway, subnet, dns;
bool bFixedIP = false;

#ifdef VIRTUAL_P1
  char virtual_p1_ip[20] ="";
#endif

TaskHandle_t tP1Reader; //  own proces for P1 reading

enum  { PERIOD_UNKNOWN, HOURS, DAYS, MONTHS, YEARS };
enum  E_ringfiletype {RINGHOURS, RINGDAYS, RINGMONTHS, RINGVOLTAGE};
enum  SolarSource { ENPHASE, SOLAR_EDGE };

//test
struct RingRecord {
  char date[9];
  float values[6];
};

RingRecord RNGDayRec[15];

void printRecordArray(const RingRecord* records, int slots, const char* label);
bool loadRingfile(E_ringfiletype type);

typedef struct {
    char filename[17];
    uint16_t slots;
    unsigned int seconds;
    int f_len;
  } S_ringfile;


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
  /* FixedValue */            ,energy_delivered_total
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

/*TODO espnow communicatie

typedef struct struct_pairing {
    uint8_t msgType;     //Pair
    char    ssid[32];    //max 32
    char    pw[63];      //max 63
    char    host[30];    //max 30
    uint8_t ipAddr[4];  //max 4
} struct_pairing;

//6*4 + 8 = 32
typedef struct HistRect {
  time_t    epoch;
  uint32_t  T1;
  uint32_t  T2;
  uint32_t  T1r;
  uint32_t  T2r;
  uint32_t  G;
  uint32_t  W;
};

//1 + 7 * 32  = 225
typedef struct HistData {
  uint8_t   msgType; //HistData
  HistRect  recs[7];
};

// 8 + 8*4 = 40 bytes
struct Actuals {
  uint8_t   msgType; //Actuals
  time_t    epoch; //8
  uint32_t  actEin; //4
  uint32_t  actEout;//4
  uint32_t  actG;//4
  uint32_t  actW;//4
  uint32_t  dailyEin;//4
  uint32_t  dailyEout;//4
  uint32_t  dailyG;//4
  uint32_t  dailyW;//4
};

P1DataRec P1_Day[15]; //390 bytes 
P1DataRec P1_Hour[25]; //650 bytes 
P1DataRec P1_Month[49]; //1.274 bytes 
*/
//P1DataRec P1_Profile[288]; //7.488

const PROGMEM char *flashMode[]    { "QIO", "QOUT", "DIO", "DOUT", "Unknown" };

//===========================prototype's=======================================
int strcicmp(const char *a, const char *b);
void delayms(unsigned long);
void SetConfig();

//===========================GLOBAL VAR'S======================================
#ifndef MQTT_DISABLE 
  #include <PubSubClient.h>           // MQTT client publish and subscribe functionality
#endif
  WiFiClientSecure wifiClientTLS;
  WiFiClient wifiClient;
  PubSubClient MQTTclient(wifiClient);

//config + button
int8_t IOWater = -1;
bool UseRGB = false; 
volatile unsigned long      Tpressed = 0;
volatile bool bButtonPressed = false;
uint8_t SelMap = 0;
uint32_t currentDay = 0;

struct Status {
   uint32_t           reboots;
   uint32_t           sloterrors; //deprecated
   char               timestamp[14];
   volatile uint32_t  wtr_m3;
   volatile uint16_t  wtr_l;
   uint16_t           dev_type;   
   bool               FirstUse;
   E_eid_states       eid_state;
} P1Status;

struct stats{
  uint32_t StartTime = 0;
  uint32_t U1piek = 0;
  uint32_t U2piek = 0;
  uint32_t U3piek = 0;
  uint32_t U1min = 0xFFFFFFFF;
  uint32_t U2min = 0xFFFFFFFF;
  uint32_t U3min = 0xFFFFFFFF;  
  uint32_t TU1over = 0;
  uint32_t TU2over = 0;
  uint32_t TU3over = 0;
  uint32_t I1piek = 0;
  uint32_t I2piek = 0;
  uint32_t I3piek = 0;
  uint32_t Psluip = 0xFFFFFFFF;
  uint32_t P1max  = 0;
  uint32_t P2max  = 0;
  uint32_t P3max  = 0;
  int32_t P1min   = 0xFFFFFFFF;
  int32_t P2min   = 0xFFFFFFFF;
  int32_t P3min   = 0xFFFFFFFF; 
} P1Stats;

MyData      DSMRdata;
struct tm   tm;
bool        DSTactive;
time_t      actT, newT;
char        actTimestamp[20] = "";
char        newTimestamp[20] = "";
uint32_t    telegramCount = 0, telegramErrors = 0, mqttCount = 0;
bool        showRaw = false;
bool        LEDenabled    = true;
// bool        DSMR_NL       = true;
bool        bUseEtotals   = false;
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
byte        TxO1 = TXO1;
//bool        bWriteFiles = false;

//vitals
char      macStr[18] = { 0 };
char      macID[13];

String      CapTelegram;
char        cMsg[150];
String      lastReset           = "";
bool        FSNotPopulated      = false;
bool        Verbose1 = false, Verbose2 = false;
uint32_t    unixTimestamp;

IPAddress ipDNS, ipGateWay, ipSubnet;
float     settingEDT1 = 0.1, settingEDT2 = 0.2, settingERT1 = 0.3, settingERT2 = 0.4, settingGDT = 0.5, settingWDT = 1.04;
float     settingENBK = 29.62, settingGNBK = 17.30,settingWNBK = 55.05;
uint8_t   settingSmHasFaseInfo = 1;
char      settingHostname[30] = _DEFAULT_HOSTNAME;
char      settingIndexPage[50] = _DEFAULT_HOMEPAGE;
enum tNetwState { NW_NONE, NW_WIFI, NW_ETH};
uint8_t netw_state = NW_NONE;
bool      FSmounted = false;
//MQTT
#ifndef MQTTKB
  uint32_t   settingMQTTinterval = 10;
  char      settingMQTTbroker[101], settingMQTTuser[75], settingMQTTpasswd[160], settingMQTTtopTopic[50] = _DEFAULT_MQTT_TOPIC;
#else
  #include "_mqtt_kb.h"
  #define NO_HA_AUTODISCOVERY
  #undef OTAURL
  #define NO_STORAGE
  #define OTAURL "http://ota.smart-stuff.nl/p1e/kb/"
  uint32_t   settingMQTTinterval     = MQTT_INTERVAL;
  char      settingMQTTbroker[101]  = MQTT_BROKER;
  char      settingMQTTuser[75]     = MQTT_USER;
  char      settingMQTTpasswd[160]  = MQTT_PASSWD;
  char      settingMQTTtopTopic[50] = _DEFAULT_MQTT_TOPIC;
#endif 

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
bool      bMQTToverTLS = false;

//update
char      BaseOTAurl[45] = OTAURL;
char      UpdateVersion[25] = "";
bool      bUpdateSketch = true;
bool      bAutoUpdate = false;

//Post_Telegram
#ifdef POST_TELEGRAM
  time_t TelegramLastPost = 0;
  uint16_t pt_port = 80;
  uint16_t pt_interval = 60;
  char pt_end_point[60];
#endif

//modbus
int8_t mb_rx  = -1;
int8_t mb_tx  = -1;
int8_t mb_rts = -1;

#include "Debug.h"
#include <ESPmDNS.h>  
#include <Update.h>
#include <WiFiManager.h>        // https://github.com/tzapu/WiFiManager
#include <HTTPClient.h>
#include "NetTypes.h"        //included in #include "ESPTelnet.h"
#include "_Button.h"

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
