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
  LogFile("OTA UPDATE started",true);
  if (bWebUpdate) httpServer.send(200, "text/html", "OTA update gestart, duurt ca. 2 - 3 minuten...");
}

void update_progress(int cur, int total) {
  Debugf("HTTP update process at %d of %d bytes...\r", cur, total);
}

void update_error(int err) {
  Debugf("HTTP update fatal error code %d | %s\n", err, httpUpdate.getLastErrorString().c_str());
  LogFile("OTA ERROR: no update",false);
  if (bWebUpdate) httpServer.send(200, "text/html", "OTA ERROR: " + err);
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
  Debugln(sketch ? "Update type: Sketch" : "Update type: File"); 


  if (bWebUpdate) {
    if (httpServer.argName(0) != "version") {
        httpServer.send(200, "text/html", "OTA ERROR: No version argument");
        LogFile("OTA ERROR: missing version argument",true );
        bWebUpdate = false;
        return;
    }
    _versie = httpServer.arg(0);
  } //bWebUpdate
  else if ( strlen(versie) ) _versie = versie; 
       else {   
              LogFile("OTA ERROR: missing version argument", true);
              bWebUpdate = false; 
              return; 
            }
  
  vTaskSuspend(tP1Reader);

  otaFile = strcmp(versie,"4-sketch-latest") == 0 ? "" : "DSMR-API-V";
  otaFile += _versie + "_" + flashSize; 
  otaFile+= sketch ? "Mb.bin" : "Mb.spiffs.bin";
  path = String(BaseOTAurl) + otaFile;
  Debugf("OTA versie: %s | flashsize: %i Mb\n", _versie.c_str(), flashSize);
  Debugln("OTA path: " + path);

  // Add optional callback notifiers
  httpUpdate.onStart(update_started);
  httpUpdate.onEnd(update_finished);
  httpUpdate.onProgress(update_progress);
  httpUpdate.onError(update_error);
  httpUpdate.rebootOnUpdate(false); 
  
  //start update proces
  if ( sketch ) ret = httpUpdate.update(client, path.c_str());
  else ret = httpUpdate.updateSpiffs(client,path.c_str());
  switch (ret) {
      case HTTP_UPDATE_FAILED:
        Debugf("OTA ERROR: (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
        break;

      case HTTP_UPDATE_NO_UPDATES:
        Debugln("OTA ERROR: HTTP_UPDATE_NO_UPDATES");
        break;

      case HTTP_UPDATE_OK:
        P1Reboot();
        break;
    }
  Debugln();
  UpdateRequested = false;
  bWebUpdate = false;
  vTaskResume(tP1Reader); //ook als het hiet goed is gegaan weer starten met lezen
} //RemoteUpdate

//---------------

void ReadManifest() {
  HTTPClient http;
  http.begin(wifiClient, "http://ota.smart-stuff.nl/v5/version-manifest.json");
  
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
  DynamicJsonDocument manifest(256);

  // Parse JSON object
//  DeserializationError error = deserializeJson(manifest, payload);
//  if (error) Debugln(F("Error deserialisation Json"));
//  Debugf("version: %s\n\r",manifest["version"]);
//  Debugf("major: %i\n\r",manifest["major"]);
//  Debugf("minor: %i\n\r",manifest["minor"]);
//  Debug(F("build: "));Debugln(String(manifest["build"]).c_str());
//  Debug(F("notes: "));Debugln(String(manifest["notes"]).c_str());
//  Debug(F("ota_url: "));Debugln(String(manifest["ota_url"]).c_str());
    
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
