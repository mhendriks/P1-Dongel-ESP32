//BIGUP VERSION

/************************************************************************************
Arduino-IDE settings for P1 Dongle hardware ESP32:
  - Board: "ESP32C3 Dev Module"
  - Flash mode: "QIO"
  - Flash size: "4MB (32Mb)"
  - CorenDebug Level: "None"
  - Flash Frequency: "80MHz"
  - CPU Frequency: "160MHz"
  - Upload Speed: "961600"                                                                                                                                                                                                                                                 
  - Port: <select correct port>
*/
/******************** compiler options  ********************************************/
//#define SHOW_PASSWRDS   // well .. show the PSK key and MQTT password, what else?     
//#define SE_VERSION
#define ETHERNET
//#define STUB            //test only
//#define HEATLINK        //first draft
//#define INSIGHT         
//#define AP_ONLY
//#define MBUS
//#define MQTT_DISABLE
//#define NO_STORAGE
//#define VOLTAGE_MON
//#define EID
//#define DEV_PAIRING
//#define DEBUG

#include "DSMRloggerAPI.h"

void setup() 
{
  USBSerial.begin(115200); //cdc stream

  Debug("\n\n ----> BOOTING P1 Dongle Pro [" _VERSION "] <-----\n\n");

  P1StatusBegin(); //leest laatste opgeslagen status & rebootcounter + 1

  pinMode(DTR_IO, OUTPUT);
  pinMode(LED, OUTPUT);
  SetConfig();
  
  if ( IOWater != -1 ) pinMode(IOWater, INPUT_PULLUP); //Setconfig first
  
  if ( P1Status.dev_type == PRO_BRIDGE ) {
    pinMode(PRT_LED, OUTPUT);
    digitalWrite(PRT_LED, true); //default on
  }

  lastReset = getResetReason();
  DebugT(F("Last reset reason: ")); Debugln(lastReset);
  
//================ File System =====================================
  if (LittleFS.begin(true)) 
  {
    DebugTln(F("FS Mount OK\r"));
    FSmounted = true;     
  } else DebugTln(F("!!!! FS Mount ERROR\r"));   // Serious problem with File System 
  
//================ Status update ===================================
  actT = epoch(actTimestamp, strlen(actTimestamp), true); // set the time to actTimestamp!
  P1StatusWrite();
  LogFile("",false); // write reboot status to file
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
  GetMacAddress();
  delay(100);
  startTelnet();
#ifndef AP_ONLY
  startMDNS(settingHostname);
  startNTP();

#ifndef EID  
  MQTTsetServer();
#endif  

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

  setupWater();
//  setupAuxButton(); //esp32c3 only

  if (EnableHistory) CheckRingExists();

//================ Start Slimme Meter ===============================
  
  SetupSMRport();
  
//create a task that will be executed in the fP1Reader() function, with priority 2
  if( xTaskCreate( fP1Reader, "p1-reader", 30000, NULL, 2, &tP1Reader ) == pdPASS ) DebugTln(F("Task tP1Reader succesfully created"));

  DebugTf("Startup complete! actTimestamp[%s]\r\n", actTimestamp);  

#ifdef MBUS
  mbusSetup();
#endif  

} // setup()


//P1 reader task
void fP1Reader( void * pvParameters ){
    DebugTln(F("Enable slimme meter..."));
    SetupP1Out();
    slimmeMeter.enable(false);
    while(true) {
      handleSlimmemeter();
      P1OutBridge();
      vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void loop () { 
        
        httpServer.handleClient();
        handleMQTT();   
        yield();
      
       if ( DUE(StatusTimer) && (telegramCount > 2) ) { 
          P1StatusWrite();
          MQTTSentStaticInfo();
          CHANGE_INTERVAL_MIN(StatusTimer, 30);
       }
#ifndef ETHERNET       
       handleReconnectWifi();
#endif
       handleKeyInput();
       handleRemoteUpdate();
       AuxButton.handler();
       handleWater();
       handleEnergyID();
  
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
