/*
***************************************************************************  
**  Program  : P1-Dongel-ESP32
**  Copyright (c) 2023 Martijn Hendriks / based on DSMR Api Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*   
*   
************************************************************************************
Arduino-IDE settings for P1 Dongle hardware ESP32:
  - Board: "ESP32 Dev Module"
  - Flash mode: "QIO"
  - Flash size: "4MB (32Mb)"
  - CorenDebug Level: "None"
  - Flash Frequency: "80MHz"
  - CPU Frequency: "240MHz"
  - Upload Speed: "961600"                                                                                                                                                                                                                                         
  - Port: <select correct port>

// 2.0.13 SDK

*/
/******************** compiler options  ********************************************/
//#define SHOW_PASSWRDS   // well .. show the PSK key and MQTT password, what else?     
//#define SE_VERSION
//#define USE_NTP_TIME              
//#define ETHERNET
//#define STUB            //test only : first draft
//#define HEATLINK        //first draft
//#define INSIGHT         
//#define AP_ONLY
#define EVERGI

/******************** don't change anything below this comment **********************/
#include "DSMRloggerAPI.h"

//===========================================================================================
void setup() {
  SerialOut.begin(115200); //debug stream
  
#ifdef ARDUINO_ESP32C3_DEV
  USBSerial.begin(115200); //cdc stream
#endif

  P1StatusBegin(); //leest laatste opgeslagen status & rebootcounter + 1

  pinMode(DTR_IO, OUTPUT);
  pinMode(LED, OUTPUT);
  
  if ( P1Status.dev_type == PRO_BRIDGE ) {
    pinMode(PRT_LED, OUTPUT);
    digitalWrite(PRT_LED, true); //default on
  }

  if ( (P1Status.dev_type == PRO_BRIDGE) || (P1Status.dev_type == PRO_ETH) ){
    RGBLED.begin();           
    RGBLED.clear();
    RGBLED.show();            
    RGBLED.setBrightness(BRIGHTNESS);
  }
  // sign of life = ON during setup
  SwitchLED( LED_ON, BLUE );
  delay(2000);
  SwitchLED( LED_OFF, BLUE );

  lastReset = getResetReason();
  Debug("\n\n ----> BOOTING....[" _VERSION "] <-----\n\n");
  DebugTln("The board name is: " ARDUINO_BOARD);
  DebugT(F("Last reset reason: ")); Debugln(lastReset);
  
//================ File System =====================================
  if (LittleFS.begin(true)) 
  {
    DebugTln(F("File System Mount succesfull\r"));
    FSmounted = true;     
  } else DebugTln(F("!!!! File System Mount failed\r"));   // Serious problem with File System 
  
//================ Status update ===================================
  actT = epoch(actTimestamp, strlen(actTimestamp), true); // set the time to actTimestamp!
  P1StatusWrite();
  Log2File("", true); // write reboot status to file
  readSettings(true);
  
//=============start Networkstuff ==================================
#ifndef ETHERNET
  #ifndef AP_ONLY
    startWiFi(settingHostname, 240);  // timeout 4 minuten
  #else 
    startAP();
  #endif
#else
  startETH();
#endif
  delay(100);
  startTelnet();
#ifndef AP_ONLY
  startMDNS(settingHostname);
  startNTP();
  MQTTclient.setBufferSize(MQTT_BUFF_MAX);
//================ Start MQTT  ======================================
//  connectMQTT(); //not needed ... will be initiated on first mqtt message
#endif
//================ Check necessary files ============================
  if (!DSMRfileExist(settingIndexPage, false) ) {
    DebugTln(F("Oeps! Index file not pressent, try to download it!\r"));
    GetFile(settingIndexPage); //download file from cdn
    if (!DSMRfileExist(settingIndexPage, false) ) { //check again
      DebugTln(F("Index file still not pressent!\r"));
      FSNotPopulated = true;
      }
  }
  if (!FSNotPopulated) {
    DebugTln(F("FS correct populated -> normal operation!\r"));
    httpServer.serveStatic("/", LittleFS, settingIndexPage);
  }
 
  if (!DSMRfileExist("/Frontend.json", false) ) {
    DebugTln(F("Frontend.json not pressent, try to download it!"));
    GetFile("/Frontend.json");
  }
  setupFSexplorer();

  esp_register_shutdown_handler(ShutDownHandler);

#ifdef EVERGI
  EvergiMQTTSettings();
#endif 

  setupWater();
  setupAuxButton(); //esp32c3 only

  if (EnableHistory) CheckRingExists();

//================ Start Slimme Meter ===============================
  
  SetupSMRport();
  
//create a task that will be executed in the fP1Reader() function, with priority 2
  if( xTaskCreate( fP1Reader, "p1-reader", 30000, NULL, 2, &tP1Reader ) == pdPASS ) DebugTln(F("Task tP1Reader succesfully created"));
  
  DebugTf("Startup complete! actTimestamp[%s]\r\n", actTimestamp);  
  
} // setup()


//P1 reader task
void fP1Reader( void * pvParameters ){
    DebugTln(F("Enable slimme meter..."));
    slimmeMeter.enable(false);
    while(true) {
      handleSlimmemeter();
      vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

//test MQTT 
//DECLARE_TIMER_SEC(mqtttest,          3);
//const char* pp = "{'timestamp':'230402232030S','energy_delivered_tariff1':3321.77,'energy_delivered_tariff2':3943.237,'energy_returned_tariff1':0,'energy_returned_tariff2':0,'electricity_tariff':'0001','power_delivered':0.689,'power_returned':0,'voltage_l1':232.2,'voltage_l2':233,'voltage_l3':233.1,'current_l1':0,'current_l2':0,'current_l3':3,'power_delivered_l1':0.003,'power_delivered_l2':0.073,'power_delivered_l3':0.613,'power_returned_l1':0,'power_returned_l2':0,'power_returned_l3':0}";

void loop () { 
        
        httpServer.handleClient();              
//        if (DUE(mqtttest)) sendMQTTDataEV_test();
        MQTTclient.loop();
        yield();
      
       if ( DUE(StatusTimer) && (telegramCount > 2) ) { 
          P1StatusWrite();
          StaticInfoSend = false;
#ifdef EVERGI
          MQTTSentStaticInfoEvergi();
#else 
          MQTTSentStaticInfo();          
#endif          
          CHANGE_INTERVAL_MIN(StatusTimer, 30);
       }
       
       handleKeyInput();
       handleButton();
       handleWater();
       handleUpdate();
  
} // loop()


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
