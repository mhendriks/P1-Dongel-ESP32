#include <HTTPUpdate.h>
bool manifestBootCheckDone = false;
bool manifestCheckPending = false;
uint32_t manifestLastDailyCheckKey = 0;
uint32_t manifestScheduleNextCheckMs = 0;
static char remoteUpdateState[16] = "idle";
static char remoteUpdateDetail[64] = "";
static uint8_t remoteUpdateProgress = 0;
static uint32_t remoteUpdateQueuedAt = 0;

static void setRemoteUpdateStatus(const char* state, const char* detail = "") {
  strlcpy(remoteUpdateState, state ? state : "idle", sizeof(remoteUpdateState));
  strlcpy(remoteUpdateDetail, detail ? detail : "", sizeof(remoteUpdateDetail));
}

void AppendRemoteUpdateStatus(JsonDocument& doc) {
  JsonObject ota = doc["ota"].to<JsonObject>();
  ota["state"] = remoteUpdateState;
  ota["progress"] = remoteUpdateProgress;
  ota["detail"] = remoteUpdateDetail;
}

void handleRemoteUpdate(){
  if (UpdateRequested && millis() - remoteUpdateQueuedAt < 1500) return;
  if (UpdateRequested) RemoteUpdate(UpdateVersion,bUpdateSketch);
}

void RemoteUpdate() {
  if (!httpServer.hasArg("version")) {
    LogFile("OTA ERROR: missing version argument", true);
    httpServer.sendHeader("Location", "/#UpdateStart?error=missing_argument", true);
    httpServer.send(303, "text/html", "");
    setRemoteUpdateStatus("error", "missing_argument");
    return;
  }

  const String requestedVersion = httpServer.arg("version");
  String errorDetail;
  if (!RemoteUpdateAvailable(requestedVersion.c_str(), &errorDetail)) {
    httpServer.sendHeader("Location", "/#UpdateStart?error=" + errorDetail, true);
    httpServer.send(303, "text/html", "");
    setRemoteUpdateStatus("error", errorDetail.c_str());
    return;
  }

  if (!QueueRemoteUpdate(requestedVersion.c_str(), true)) {
    httpServer.sendHeader("Location", "/#UpdateStart?error=failed", true);
    httpServer.send(303, "text/html", "");
    setRemoteUpdateStatus("error", "failed");
    return;
  }

  httpServer.sendHeader("Location", "/#UpdateStart", true);
  httpServer.send(303, "text/html", "");
}

bool QueueRemoteUpdate(const char* versie, bool sketch) {
  if (!versie || !strlen(versie)) {
    LogFile("OTA ERROR: missing version argument", true);
    return false;
  }

  if (UpdateRequested) {
    LogFile("OTA ERROR: update already queued", true);
    return false;
  }

  strlcpy(UpdateVersion, versie, sizeof(UpdateVersion));
  bUpdateSketch = sketch;
  UpdateRequested = true;
  remoteUpdateQueuedAt = millis();
  remoteUpdateProgress = 0;
  setRemoteUpdateStatus("queued", versie);
  return true;
}

static bool BuildRemoteUpdatePath(const char* versie, String& path, String& otaFile) {
  if (!versie || !strlen(versie)) return false;

  const int flashSize = (ESP.getFlashChipSize() / 1024.0 / 1024.0);
  otaFile = strcmp(versie, "4-sketch-latest") == 0 ? "" : "DSMR-API-V";
  otaFile += String(versie) + "_" + flashSize + "Mb.bin";
  path = String(BaseOTAurl) + otaFile;
  return true;
}

bool RemoteUpdateAvailable(const char* versie, String* errorDetail) {
  String path;
  String otaFile;
  if (!BuildRemoteUpdatePath(versie, path, otaFile)) {
    if (errorDetail) *errorDetail = "missing_argument";
    return false;
  }

  WiFiClient client;
  HTTPClient http;
  if (!http.begin(client, path.c_str())) {
    if (errorDetail) *errorDetail = "begin_failed";
    return false;
  }

  http.setConnectTimeout(4000);
  http.setTimeout(5000);
  const int httpResponseCode = http.sendRequest("HEAD");
  http.end();

  if (httpResponseCode == HTTP_CODE_OK) return true;

  if (errorDetail) {
    if (httpResponseCode == HTTP_CODE_NOT_FOUND) *errorDetail = "File Not Found (404)";
    else if (httpResponseCode > 0) *errorDetail = "HTTP " + String(httpResponseCode);
    else *errorDetail = "update_unreachable";
  }
  return false;
}

bool RemoteUpdateNow(const char* versie, bool sketch, String* errorDetail) {
  if (!versie || !strlen(versie)) {
    LogFile("OTA ERROR: missing version argument", true);
    if (errorDetail) *errorDetail = "missing_argument";
    return false;
  }

  WiFiClient client;
  String path, otaFile, _versie = versie;
  t_httpUpdate_return ret;

  Debugln(F("\n!!! OTA UPDATE !!!"));

  if (!BuildRemoteUpdatePath(versie, path, otaFile)) {
    if (errorDetail) *errorDetail = "missing_argument";
    return false;
  }

  vTaskSuspend(tP1Reader);
  esp_task_wdt_delete(tP1Reader);
  
  Debugf("OTA versie: %s | flashsize: %i Mb\n", _versie.c_str(), (ESP.getFlashChipSize() / 1024.0 / 1024.0));
  Debugln("OTA path: " + path);

  httpUpdate.onStart(update_started);
  httpUpdate.onEnd(update_finished);
  httpUpdate.onProgress(update_progress);
  httpUpdate.onError(update_error);
  httpUpdate.rebootOnUpdate(false); 
  
  ret = httpUpdate.update(client, path.c_str());
  bool ok = false;
  switch (ret) {
      case HTTP_UPDATE_FAILED:
        Debugf("OTA ERROR: (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
        if (errorDetail) *errorDetail = httpUpdate.getLastErrorString();
        break;

      case HTTP_UPDATE_NO_UPDATES:
        Debugln("OTA ERROR: HTTP_UPDATE_NO_UPDATES");
        if (errorDetail) *errorDetail = "no_updates";
        break;

      case HTTP_UPDATE_OK:
        ok = true;
        if ( RemoveIndexAfterUpdate ) LittleFS.remove("/DSMRindexEDGE.html");
        LogFile("reboot: after update OK",false);
        P1Reboot();
        break;
    }
  Debugln();
  esp_task_wdt_add(tP1Reader);
  vTaskResume(tP1Reader);
  return ok;
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

void ManifestCheckFromWorker() {
  if (skipNetwork || netw_state == NW_NONE || UpdateRequested) {
    manifestCheckPending = false;
    return;
  }

  if (!CheckNewVersion()) {
    manifestCheckPending = false;
    return;
  }

  if (!bAutoUpdate) {
    LogFile("AutoUpdate: update available (auto mode disabled)", true);
    manifestCheckPending = false;
    return;
  }

  LogFile("AutoUpdate: starting OTA install", true);
  QueueRemoteUpdate(UpdateVersion, true);
  manifestCheckPending = false;
}

void handleManifestCheckSchedule(bool runBootCheckNow) {
  if (skipNetwork || netw_state == NW_NONE || UpdateRequested || manifestCheckPending) return;

  const uint32_t nowMs = millis();
  if (!runBootCheckNow && nowMs < manifestScheduleNextCheckMs) return;
  manifestScheduleNextCheckMs = nowMs + (5UL * 60UL * 1000UL); // check planning every 5 minutes

  const bool doBootCheck = (!manifestBootCheckDone);

  bool doDailyCheck = false;
  uint32_t dayKey = 0;
  if (now() > 1700000000) { // only if clock is sane
    const int h = hour();
    dayKey = (uint32_t)year() * 10000UL + (uint32_t)month() * 100UL + (uint32_t)day();
    if ((h >= 1 && h < 6) && (manifestLastDailyCheckKey != dayKey)) {
      doDailyCheck = true;
    }
  }

  if (!doBootCheck && !doDailyCheck) return;

  manifestCheckPending = true;
  if (!WorkerEnqueueSimple(WORKER_JOB_MANIFEST_CHECK, WORKER_PRIO_NORMAL)) {
    manifestCheckPending = false;
    return;
  }

  if (doBootCheck) manifestBootCheckDone = true;
  if (doDailyCheck) manifestLastDailyCheckKey = dayKey;
}

void update_finished() {
  LogFile("OTA UPDATE succesfull", true);
  remoteUpdateProgress = 100;
  setRemoteUpdateStatus("done", "success");
}

void update_started() {
  LogFile("OTA UPDATE started", true);
  if (remoteUpdateProgress < 1) remoteUpdateProgress = 1;
  setRemoteUpdateStatus("running", UpdateVersion);
}

void update_progress(int cur, int total) {
  Debugf( "HTTP update process at %d of %d bytes = %d%%\r", cur, total, (cur * 100) / total );
  if (total > 0) {
    uint8_t actualProgress = (uint8_t)((cur * 90) / total);
    remoteUpdateProgress = actualProgress > 90 ? 90 : actualProgress;
  }
  setRemoteUpdateStatus("running", UpdateVersion);
  esp_task_wdt_reset();
}

void update_error(int err) {
  Debugf("HTTP update fatal error code %d | %s\n", err, httpUpdate.getLastErrorString().c_str());
  LogFile("OTA ERROR: no update",false);
  setRemoteUpdateStatus("error", httpUpdate.getLastErrorString().c_str());
}

//---------------
void RemoteUpdate(const char* versie, bool sketch){
  (void)sketch;
  String ignored;
  RemoteUpdateNow(versie, sketch, &ignored);
  UpdateRequested = false;
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
