/* 
***************************************************************************  
**  Program  : processTelegram, part of DSMRloggerAPI
**
**  Copyright (c) 2023 Martijn Hendriks
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/
#include <HTTPUpdate.h>
bool bWebUpdate = false;

void handleRemoteUpdate(){
  if (UpdateRequested) RemoteUpdate(UpdateVersion,bUpdateSketch);
}

void update_finished() {
  LogFile("OTA UPDATE succesfull", true);
}

void update_started() {
  LogFile("OTA UPDATE started", true);
  if (bWebUpdate) {
    httpServer.sendHeader("Location", "/#UpdateStart");
    httpServer.send ( 303, "text/html", "");
    // //handle redirect otherwise the browser will stay in pending
    int cnt = 0;
    while (cnt++ < 10) {
      httpServer.handleClient();
      esp_task_wdt_reset();
      delay(100);
    }
  }
}

void update_progress(int cur, int total) {
  Debugf( "HTTP update process at %d of %d bytes = %d%%\r", cur, total, (cur * 100) / total );
}

void update_error(int err) {
  Debugf("HTTP update fatal error code %d | %s\n", err, httpUpdate.getLastErrorString().c_str());
  LogFile("OTA ERROR: no update",false);
  // if (bWebUpdate) httpServer.send(500, "text/html", "OTA ERROR: " + err);
}

//---------------
void RemoteUpdate(){
    bWebUpdate = true;
    RemoteUpdate("", true);
}

//---------------
void RemoteUpdate(const char* versie, bool sketch){
/*
 * nodig bij de update:
 * - Flashsize
 * - versienummer + land 
 * voorbeeld aanroep : /remote-update?version=3.1.4
 * voorbeeld : invoer 2.3.7BE -> DMSR-API-V2.3.7BE_<FLASHSIZE>Mb.bin
 * voorbeeld : invoer 2.3.7 -> DMSR-API-V2.3.7_<FLASHSIZE>Mb.bin
 */
  WiFiClient client;
  int flashSize = (ESP.getFlashChipSize() / 1024.0 / 1024.0);
  String path, otaFile, _versie;
  t_httpUpdate_return ret;

  Debugln(F("\n!!! OTA UPDATE !!!"));


  if (bWebUpdate) {
    if (httpServer.argName(0) != "version") {
        // httpServer.send(500, "text/html", "OTA ERROR: No version argument");
        LogFile("OTA ERROR: missing version argument",true );
        httpServer.sendHeader("Location", "/#UpdateStart?error=no_argument");
        httpServer.send ( 303, "text/html", "");
        bWebUpdate = false;
        return;
    }
    _versie = httpServer.arg(0);
  } //bWebUpdate
  else if ( strlen(versie) ) _versie = versie; 
       else {   
              LogFile("OTA ERROR: missing version argument", true);
              // httpServer.send(500, "text/html", "OTA ERROR: missing version argument");
              httpServer.sendHeader("Location", "/#UpdateStart?error=missing_argument");
              httpServer.send ( 303, "text/html", "");
              bWebUpdate = false; 
              return; 
            }
  
  vTaskSuspend(tP1Reader);
  esp_task_wdt_delete(tP1Reader);

  otaFile = strcmp(versie,"4-sketch-latest") == 0 ? "" : "DSMR-API-V";
  otaFile += _versie + "_" + flashSize + "Mb.bin"; 
  
  
#ifdef MQTTKB
  path = String(OTAURL) + otaFile;
#else  
  path = String(BaseOTAurl) + otaFile;
#endif

  Debugf("OTA versie: %s | flashsize: %i Mb\n", _versie.c_str(), flashSize);
  Debugln("OTA path: " + path);

  // Add optional callback notifiers
  httpUpdate.onStart(update_started);
  httpUpdate.onEnd(update_finished);
  httpUpdate.onProgress(update_progress);
  httpUpdate.onError(update_error);
  httpUpdate.rebootOnUpdate(false); 
  
  //start update proces
  ret = httpUpdate.update(client, path.c_str());
  switch (ret) {
      case HTTP_UPDATE_FAILED:
        Debugf("OTA ERROR: (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());

        if (bWebUpdate) {
          httpServer.sendHeader("Location", "/#UpdateStart?error=failed");
          httpServer.send ( 303, "text/html", "");
        }
        break;

      case HTTP_UPDATE_NO_UPDATES:
        Debugln("OTA ERROR: HTTP_UPDATE_NO_UPDATES");
        break;

      case HTTP_UPDATE_OK:
        LittleFS.remove("/DSMRindexEDGE.html");
        LogFile("reboot: after update OK",false);
        P1Reboot();
        break;
    }
  Debugln();
  UpdateRequested = false;
  bWebUpdate = false;
  esp_task_wdt_add(tP1Reader);
  vTaskResume(tP1Reader); //error -> start P1 proces 
} //RemoteUpdate

//---------------

void ReadManifest() {
  HTTPClient http;
  String ota_manifest = BaseOTAurl;
  ota_manifest += "version-manifest.json";
  http.begin(wifiClient, ota_manifest.c_str() );
    
  int httpResponseCode = http.GET();
  if ( httpResponseCode<=0 ) { 
    Debug(F("Error code: "));Debugln(httpResponseCode);
    return; //leave on error
  }
  
  Debugln( F("Version Manifest") );
  Debug(F("HTTP Response code: "));Debugln(httpResponseCode);
  String payload = http.getString();
  Debugln(payload);
  http.end();
    
  // Parse JSON object in response
  JsonDocument manifest;

} //ReadManifest


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
