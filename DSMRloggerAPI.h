  /*
***************************************************************************  
**  Program  : DSMRloggerAPI.h - definitions for DSMRloggerAPI
**
**  Copyright (c) 2023 Martijn Hendriks / based on DSMR Api Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/  
#include "Config.h"

// water sensor
  volatile byte        WtrFactor      = 1;
  volatile time_t      WtrTimeBetween = 0;
  volatile byte        debounces      = 0;
  volatile time_t      WtrPrevReading = 0;
  bool                 WtrMtr         = false;
  #define              DEBOUNCETIMER 1700
 
#include <TimeLib.h>            // https://github.com/PaulStoffregen/Time
#include <TelnetStream.h>       // https://github.com/jandrassy/TelnetStream
#include "safeTimers.h"
#include "version.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <dsmr2.h>               //  https://github.com/mrWheel/dsmr2Lib.git

#define JSON_BUFF_MAX     255
#define MQTT_BUFF_MAX     800

P1Reader    slimmeMeter(&Serial1, DTR_IO);

TaskHandle_t tP1Reader; //  own proces for P1 reading

enum  { PERIOD_UNKNOWN, HOURS, DAYS, MONTHS, YEARS };
enum  E_ringfiletype {RINGHOURS, RINGDAYS, RINGMONTHS,RINGPROFILE};

typedef struct {
    char filename[17];
    uint8_t slots;
    unsigned int seconds;
    int f_len;
  } S_ringfile;

const S_ringfile RingFiles[3] = {{"/RNGhours.json", 48+1,SECS_PER_HOUR, 4826}, {"/RNGdays.json",14+1,SECS_PER_DAY, 1494},{"/RNGmonths.json",24+1,0,2474}}; //+1 voor de vergelijking, laatste record wordt niet getoond 
#define JSON_HEADER_LEN   23  //total length incl new line
#define DATA_CLOSE        2   //length last row of datafile
#define DATA_FORMAT      "{\"date\":\"%-8.8s\",\"values\":[%10.3f,%10.3f,%10.3f,%10.3f,%10.3f,%10.3f]}"
#define DATA_RECLEN      98  //total length incl comma and new line

#include "Debug.h"

  /**
  * Define the DSMRdata we're interested in, as well as the DSMRdatastructure to
  * hold the parsed DSMRdata. This list shows all supported fields, remove
  * any fields you are not using from the below list to make the parsing
  * and printing code smaller.
  * Each template argument below results in a field of the same name.
  * 
  * gekozen om niet alle elementen te parsen.
  * aanname is dat voor 99% van de gebruikers de gasmeter op Mbus1 zit. Alleen deze wordt verwerkt.
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
#ifndef SE_VERSION
  /* FixedValue */            ,energy_delivered_tariff1
  /* FixedValue */            ,energy_delivered_tariff2
  /* FixedValue */            ,energy_returned_tariff1
  /* FixedValue */            ,energy_returned_tariff2
#else
  /* FixedValue */            ,energy_delivered_total
  /* FixedValue */            ,energy_delivered_tariff2
  /* FixedValue */            ,energy_returned_total
  /* FixedValue */            ,energy_returned_tariff2 
#endif
  /* String */                ,electricity_tariff
  /* FixedValue */            ,power_delivered
  /* FixedValue */            ,power_returned
  /* FixedValue */            ,electricity_threshold
//  /* uint8_t */               ,electricity_switch_position
//  /* uint32_t */              ,electricity_failures
//  /* uint32_t */              ,electricity_long_failures
  /* String */                ,electricity_failure_log
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

//TODO
//in mem houden van de ringfile gegevens voor een snelle oplevering naar de client. Vooral de Day gegevens die worden door het dashboard gebruikt.
//struct P1DataRec {
//  uint32_t  epoch;
//  uint32_t  T1;
//  uint32_t  T2;
//  uint32_t  T1r;
//  uint32_t  T2r;
//  uint32_t  G;
//  uint16_t  Water;
//};
//
//P1DataRec P1_Day[15]; //390 bytes 
//P1DataRec P1_Hour[25]; //650 bytes 
//P1DataRec P1_Month[49]; //1.274 bytes 

//P1DataRec P1_Profile[288]; //7.488

const PROGMEM char *flashMode[]    { "QIO", "QOUT", "DIO", "DOUT", "Unknown" };

//===========================prototype's=======================================
int strcicmp(const char *a, const char *b);
void delayms(unsigned long);

//===========================GLOBAL VAR'S======================================
WiFiClient  wifiClient;
#include <PubSubClient.h>           // MQTT client publish and subscribe functionality
static PubSubClient MQTTclient(wifiClient);

#include "Network.h"

struct Status {
   uint32_t           reboots;
   uint32_t           sloterrors; //deprecated
   char               timestamp[14];
   volatile uint32_t  wtr_m3;
   volatile uint16_t  wtr_l;
   uint16_t           dev_type;
   bool               FirstUse;
} P1Status;
  
MyData      DSMRdata;
struct tm   tm;
bool        DSTactive;
time_t      actT, newT;
char        actTimestamp[20] = "";
char        newTimestamp[20] = "";
uint32_t    telegramCount = 0, telegramErrors = 0;
bool        showRaw = false;
bool        LEDenabled    = true;
bool        DSMR_NL       = true;
bool        EnableHAdiscovery = true;
bool        bHideP1Log = false;
char        bAuthUser[25]="", bAuthPW[25]="";
bool        EnableHistory = true;
bool        bPre40 = false;
bool        bActJsonMQTT = false;
bool        bRawPort = false;
bool        bLED_PRT = true;

String      CapTelegram;
uint16_t    CRCTelegram;
char        cMsg[150];
String      lastReset           = "";
bool      FSNotPopulated      = false;
bool      mqttIsConnected     = false;
bool      Verbose1 = false, Verbose2 = false;
uint32_t  unixTimestamp;

IPAddress ipDNS, ipGateWay, ipSubnet;
float     settingEDT1 = 0.1, settingEDT2 = 0.2, settingERT1 = 0.3, settingERT2 = 0.4, settingGDT = 0.5;
float     settingENBK = 15.15, settingGNBK = 11.11;
uint8_t   settingSmHasFaseInfo = 1;
char      settingHostname[30] = _DEFAULT_HOSTNAME;
char      settingIndexPage[50] = _DEFAULT_HOMEPAGE;

//update
char      BaseOTAurl[35] = OTAURL;
char      UpdateVersion[25] = "";
bool      bUpdateSketch = true;
bool      bAutoUpdate = false;

//MQTT
char      settingMQTTbroker[101], settingMQTTuser[40], settingMQTTpasswd[30], settingMQTTtopTopic[26] = _DEFAULT_MQTT_TOPIC;
int32_t   settingMQTTinterval = 0, settingMQTTbrokerPort = 1883;
float     gasDelivered;
String    gasDeliveredTimestamp;
bool      UpdateRequested = false;
byte      mbusGas = 0;
bool      StaticInfoSend = false;

//===========================================================================================
// setup timers 
DECLARE_TIMER_SEC(synchrNTP,          30);
DECLARE_TIMER_SEC(reconnectMQTTtimer,  5); // try reconnecting cyclus timer
DECLARE_TIMER_SEC(publishMQTTtimer,   60, SKIP_MISSED_TICKS); // interval time between MQTT messages  
DECLARE_TIMER_MS(WaterTimer,          DEBOUNCETIMER);
DECLARE_TIMER_SEC(StatusTimer,        10); //first time = 10 sec usual 10min (see loop)

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
