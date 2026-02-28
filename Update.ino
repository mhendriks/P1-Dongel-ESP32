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
bool autoUpdateBootCheckDone = false;
uint32_t autoUpdateLastDailyCheckKey = 0;
uint32_t autoUpdateNextCheckMs = 0;

void handleRemoteUpdate(){
  if (UpdateRequested) RemoteUpdate(UpdateVersion,bUpdateSketch);
}

bool ReadManifest(JsonDocument &manifest, const char* link) {
  HTTPClient http;
  String manifestUrl;
  if (link && strlen(link)) {
    manifestUrl = "http://ota.smart-stuff.nl/manifest/" + String(link) + "/manifest.json";
  } else {
    manifestUrl = String(BaseOTAurl) + "version-manifest.json";
  }

  Debugln(manifestUrl);
  http.begin(wifiClient, manifestUrl.c_str());
  http.setConnectTimeout(4000);
  http.setTimeout(5000);

  int httpResponseCode = http.GET();
  if (httpResponseCode != HTTP_CODE_OK) {
    Debugf("Manifest fetch failed: HTTP %d\n\r", httpResponseCode);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  if (deserializeJson(manifest, payload)) {
    Debugln("Manifest parse failed");
    return false;
  }
  return true;
}

void ReadManifest() {
  JsonDocument manifest;
  if (!ReadManifest(manifest, nullptr)) return;
  Debugln(F("Version Manifest"));
  String out;
  serializeJson(manifest, out);
  Debugln(out);
}

bool CheckNewVersion() {
  bool bNewVersionAvailable = false;
  char manifestVersion[15] = "";

  JsonDocument manifest;
  if (!ReadManifest(manifest, nullptr)) return false;

  int maj = manifest["major"] | -1;
  int min = manifest["minor"] | -1;
  int fix = manifest["fix"] | -1;
  if (maj < 0 || min < 0 || fix < 0) return false;

  snprintf(manifestVersion, sizeof(manifestVersion), "%d.%d.%d", maj, min, fix);
  Debugf("Manifest version : %s\n\r", manifestVersion);
  Debugf("Current version: %d.%d.%d\n\r", _VERSION_MAJOR, _VERSION_MINOR, _VERSION_PATCH);

  if (maj > _VERSION_MAJOR) bNewVersionAvailable = true;
  else if ((maj == _VERSION_MAJOR) && (min > _VERSION_MINOR)) bNewVersionAvailable = true;
  else if ((maj == _VERSION_MAJOR) && (min == _VERSION_MINOR) && (fix > _VERSION_PATCH)) bNewVersionAvailable = true;

  if (bNewVersionAvailable) {
    strlcpy(UpdateVersion, manifestVersion, sizeof(UpdateVersion));
    LogFile("AutoUpdate: NEW stable version available", true);
  } else {
    LogFile("AutoUpdate: NO new stable version available", true);
  }
  return bNewVersionAvailable;
}

void handleAutoUpdate(bool runBootCheckNow) {
  if (skipNetwork || netw_state == NW_NONE || UpdateRequested || bWebUpdate) return;

  const uint32_t nowMs = millis();
  if (!runBootCheckNow && nowMs < autoUpdateNextCheckMs) return;
  autoUpdateNextCheckMs = nowMs + (5UL * 60UL * 1000UL); // check planning every 5 minutes

  const bool doBootCheck = (!autoUpdateBootCheckDone);

  bool doDailyCheck = false;
  if (now() > 1700000000) { // only if clock is sane
    const int h = hour();
    const uint32_t dayKey = (uint32_t)year() * 10000UL + (uint32_t)month() * 100UL + (uint32_t)day();
    if ((h >= 1 && h < 6) && (autoUpdateLastDailyCheckKey != dayKey)) {
      doDailyCheck = true;
      autoUpdateLastDailyCheckKey = dayKey;
    }
  }

  if (!doBootCheck && !doDailyCheck) return;

  if (doBootCheck) autoUpdateBootCheckDone = true;

  if (!CheckNewVersion()) return;
  if (!bAutoUpdate) {
    LogFile("AutoUpdate: update available (auto mode disabled)", true);
    return;
  }

  LogFile("AutoUpdate: starting OTA install", true);
  RemoteUpdate(UpdateVersion, true);
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
  esp_task_wdt_reset();
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

  //TEST esphome install via webinterface
  // if ( _versie == "esphome") path = "http://ota.smart-stuff.nl/esphome/nrgd.factory.bin";
  
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
        if ( RemoveIndexAfterUpdate ) LittleFS.remove("/DSMRindexEDGE.html");
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
