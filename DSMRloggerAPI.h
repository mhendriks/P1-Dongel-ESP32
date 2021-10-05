/*
***************************************************************************  
**  Program  : DSMRloggerAPI.h - definitions for DSMRloggerAPI
**  Version  : v3.0.0
**
**  Copyright (c) 2021 Martijn Hendriks / based on DSMR Api Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/  

#if !( defined(ESP32) )
  #error This code is intended to run on ESP32 platform! Please check your Tools->Board setting.
#endif

#include <TimeLib.h>            // https://github.com/PaulStoffregen/Time
#include <TelnetStream.h>       // https://github.com/jandrassy/TelnetStream
#include "safeTimers.h"
#include "version.h"
#include <ArduinoJson.h>
#include <LITTLEFS.h>

#ifdef USE_SYSLOGGER
  #include "ESP_SysLogger.h"      // https://github.com/mrWheel/ESP_SysLogger
  ESPSL sysLog;                   // Create instance of the ESPSL object
  #define writeToSysLog(...) ({ sysLog.writeDbg( sysLog.buildD("[%02d:%02d:%02d][%7d][%-12.12s] " \
                                                               , hour(), minute(), second()     \
                                                               , ESP.getFreeHeap()              \
                                                               , __FUNCTION__)                  \
                                                               ,__VA_ARGS__); })
#else
  #define writeToSysLog(...)  // nothing
#endif
//
//#if defined( USE_BELGIUM_PROTOCOL )                           //Belgium
//  //  https://github.com/mrWheel/arduino-dsmr-be.git            //Belgium
//  #include <dsmr-be.h>                                          //Belgium
//#else                                                           //else
//  //  https://github.com/matthijskooijman/arduino-dsmr
//  #include <dsmr.h>               // Version 0.1 - Commit f79c906 on 18 Sep 2018
//#endif

//  https://github.com/mrWheel/dsmr2Lib.git             
#include <dsmr2.h>               // commit 0ed3916813850af43200863853bfb4b26e9655eb on 7 juni 2021

#define _DEFAULT_HOSTNAME  "P1-DONGLE" 
#define _DEFAULT_HOMEPAGE  "/DSMRindexEDGE.html"
#define SETTINGS_FILE      "/DSMRsettings.json"
#define DTR_IO             12 // 0 = always on; nr = IO pulse
#define FLASH_BUTTON        0 
#define JSON_BUFF_MAX     255
#define MQTT_BUFF_MAX     200

P1Reader    slimmeMeter(&Serial1, DTR_IO); 


enum    { PERIOD_UNKNOWN, HOURS, DAYS, MONTHS, YEARS };

enum E_ringfiletype {RINGHOURS, RINGDAYS, RINGMONTHS};

typedef struct {
    char filename[17];
    int8_t slots;
    unsigned int seconds;
    int f_len;
  } S_ringfile;

//+1 voor de vergelijking, laatste record wordt niet getoond 
//onderstaande struct kan niet in PROGMEM opgenomen worden. gaat stuk bij SPIFF.open functie

const S_ringfile RingFiles[3] = {{"/RINGhours.json", 48+1,SECS_PER_HOUR, 4287}, {"/RINGdays.json",14+1,SECS_PER_DAY, 1329},{"/RINGmonths.json",24+1,0,2199}}; 

#define DATA_FORMAT       "{\"date\":\"%-8.8s\",\"values\":[%10.3f,%10.3f,%10.3f,%10.3f,%10.3f]}"
#define DATA_RECLEN       87  //total length incl comma and new line
#define JSON_HEADER_LEN   23  //total length incl new line
#define DATA_CLOSE        2   //length last row of datafile

#include "Debug.h"
#include "Network.h"

/**
 * Define the DSMRdata we're interested in, as well as the DSMRdatastructure to
 * hold the parsed DSMRdata. This list shows all supported fields, remove
 * any fields you are not using from the below list to make the parsing
 * and printing code smaller.
 * Each template argument below results in a field of the same name.
 */
 
//using MyData = ParsedData<
//  /* String */         identification
//  /* String */        ,p1_version
//  /* String */        ,timestamp
//  /* String */        ,equipment_id
//  /* FixedValue */    ,energy_delivered_tariff1
//  /* FixedValue */    ,energy_delivered_tariff2
//  /* FixedValue */    ,energy_returned_tariff1
//  /* FixedValue */    ,energy_returned_tariff2
//  /* String */        ,electricity_tariff
//  /* FixedValue */    ,power_delivered
//  /* FixedValue */    ,power_returned
////  /* FixedValue */    ,electricity_threshold
////  /* uint8_t */       ,electricity_switch_position
////  /* uint32_t */      ,electricity_failures
////  /* uint32_t */      ,electricity_long_failures
////  /* String */        ,electricity_failure_log
////  /* uint32_t */      ,electricity_sags_l1
////  /* uint32_t */      ,electricity_sags_l2
////  /* uint32_t */      ,electricity_sags_l3
////  /* uint32_t */      ,electricity_swells_l1
////  /* uint32_t */      ,electricity_swells_l2
////  /* uint32_t */      ,electricity_swells_l3
////  /* String */        ,message_short
////  /* String */        ,message_long
//  /* FixedValue */    ,voltage_l1
//  /* FixedValue */    ,voltage_l2
//  /* FixedValue */    ,voltage_l3
//  /* FixedValue */    ,current_l1
//  /* FixedValue */    ,current_l2
//  /* FixedValue */    ,current_l3
//  /* FixedValue */    ,power_delivered_l1
//  /* FixedValue */    ,power_delivered_l2
//  /* FixedValue */    ,power_delivered_l3
//  /* FixedValue */    ,power_returned_l1
//  /* FixedValue */    ,power_returned_l2
//  /* FixedValue */    ,power_returned_l3
//  /* uint16_t */      ,gas_device_type
//  /* String */        ,gas_equipment_id
////  /* uint8_t */       ,gas_valve_position
//  /* TimestampedFixedValue */ ,gas_delivered
////  /* uint16_t */      ,thermal_device_type
////  /* String */        ,thermal_equipment_id
////  /* uint8_t */       ,thermal_valve_position
////  /* TimestampedFixedValue */ ,thermal_delivered
////  /* uint16_t */      ,water_device_type
////  /* String */        ,water_equipment_id
////  /* uint8_t */       ,water_valve_position
////  /* TimestampedFixedValue */ ,water_delivered
////  /* uint16_t */      ,slave_device_type
////  /* String */        ,slave_equipment_id
////  /* uint8_t */       ,slave_valve_position
////  /* TimestampedFixedValue */ ,slave_delivered
//>;

/**
 * Define the DSMRdata we're interested in, as well as the DSMRdatastructure to
 * hold the parsed DSMRdata. This list shows all supported fields, remove
 * any fields you are not using from the below list to make the parsing
 * and printing code smaller.
 * Each template argument below results in a field of the same name.
 */

 /***
  * gekozen om niet alle elementen te parsen.
  * aanname is dat voor 99% van de gebruikers de gasmeter op Mbus1 zit. Alleen deze wordt verwerkt.
  */
 
using MyData = ParsedData<
  /* String */                 identification
  /* String */                ,p1_version
  /* String */                ,p1_version_be
  /* String */                ,timestamp
  /* String */                ,equipment_id
  /* FixedValue */            ,energy_delivered_tariff1
  /* FixedValue */            ,energy_delivered_tariff2
  /* FixedValue */            ,energy_returned_tariff1
  /* FixedValue */            ,energy_returned_tariff2
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
  /* String */                ,message_short
  /* String */                ,message_long
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
//  /* uint16_t */              ,mbus2_device_type
//  /* String */                ,mbus2_equipment_id_tc
//  /* String */                ,mbus2_equipment_id_ntc
//  /* uint8_t */               ,mbus2_valve_position
//  /* TimestampedFixedValue */ ,mbus2_delivered
//  /* TimestampedFixedValue */ ,mbus2_delivered_ntc
//  /* TimestampedFixedValue */ ,mbus2_delivered_dbl
//  /* uint16_t */              ,mbus3_device_type
//  /* String */                ,mbus3_equipment_id_tc
//  /* String */                ,mbus3_equipment_id_ntc
//  /* uint8_t */               ,mbus3_valve_position
//  /* TimestampedFixedValue */ ,mbus3_delivered
//  /* TimestampedFixedValue */ ,mbus3_delivered_ntc
//  /* TimestampedFixedValue */ ,mbus3_delivered_dbl
//  /* uint16_t */              ,mbus4_device_type
//  /* String */                ,mbus4_equipment_id_tc
//  /* String */                ,mbus4_equipment_id_ntc
//  /* uint8_t */               ,mbus4_valve_position
//  /* TimestampedFixedValue */ ,mbus4_delivered
//  /* TimestampedFixedValue */ ,mbus4_delivered_ntc
//  /* TimestampedFixedValue */ ,mbus4_delivered_dbl
>;

enum    { TAB_UNKNOWN, TAB_ACTUEEL, TAB_LAST24HOURS, TAB_LAST7DAYS, TAB_LAST24MONTHS, TAB_GRAPHICS, TAB_SYSINFO, TAB_EDITOR };

const PROGMEM char *weekDayName[]  { "Unknown", "Zondag", "Maandag", "Dinsdag", "Woensdag"
                            , "Donderdag", "Vrijdag", "Zaterdag", "Unknown" };
const PROGMEM char *monthName[]    { "00", "Januari", "Februari", "Maart", "April", "Mei", "Juni", "Juli"
                            , "Augustus", "September", "Oktober", "November", "December", "13" };
const PROGMEM char *flashMode[]    { "QIO", "QOUT", "DIO", "DOUT", "Unknown" };

//===========================prototype's=======================================
int strcicmp(const char *a, const char *b);
void delayms(unsigned long);

//===========================GLOBAL VAR'S======================================
  WiFiClient  wifiClient;
  MyData      DSMRdata;
  time_t      actT, newT;
  char        actTimestamp[20] = "";
  char        newTimestamp[20] = "";
  uint32_t    slotErrors = 0;
  uint32_t    nrReboots  = 0;
  uint32_t    telegramCount = 0, telegramErrors = 0;
  bool        showRaw = false;
  int8_t      showRawCount = 0;
  bool        DSMR_NL      = true;


#ifdef USE_MQTT
  #include <PubSubClient.h>           // MQTT client publish and subscribe functionality
  static PubSubClient MQTTclient(wifiClient);
#endif

char      cMsg[150];
String    lastReset           = "";
bool      FSNotPopulated      = false;
bool      mqttIsConnected     = false;
bool      doLog = false, Verbose1 = false, Verbose2 = false;
int8_t    thisHour = -1, prevNtpHour = 0, thisDay = -1, thisMonth = -1, lastMonth, thisYear = 15;
uint32_t  unixTimestamp;

IPAddress ipDNS, ipGateWay, ipSubnet;
float     settingEDT1 = 0.1, settingEDT2 = 0.2, settingERT1 = 0.3, settingERT2 = 0.4, settingGDT = 0.5;
float     settingENBK = 15.15, settingGNBK = 11.11;
uint8_t   settingTelegramInterval = 2; //seconden
uint8_t   settingMbus1Type        = 3;
//  uint8_t   settingMbus2Type     = 0;
//  uint8_t   settingMbus3Type     = 0;
//  uint8_t   settingMbus4Type     = 0;
uint8_t   settingSmHasFaseInfo = 1;
char      settingHostname[30] = _DEFAULT_HOSTNAME;
char      settingIndexPage[50] = _DEFAULT_HOMEPAGE;
char      settingMQTTbroker[101], settingMQTTuser[40], settingMQTTpasswd[30], settingMQTTtopTopic[21] = _DEFAULT_HOSTNAME;
int32_t   settingMQTTinterval = 0, settingMQTTbrokerPort = 1883;
float     gasDelivered;


//===========================================================================================
// setup timers 
DECLARE_TIMER_SEC(reconnectWiFi,      10);
DECLARE_TIMER_SEC(synchrNTP,          30);
DECLARE_TIMER_SEC(nextTelegram,       10);
DECLARE_TIMER_SEC(reconnectMQTTtimer,  5); // try reconnecting cyclus timer
DECLARE_TIMER_SEC(publishMQTTtimer,   60, SKIP_MISSED_TICKS); // interval time between MQTT messages  
DECLARE_TIMER_SEC(antiWearTimer,      301);

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
