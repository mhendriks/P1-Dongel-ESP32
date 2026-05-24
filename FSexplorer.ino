 /* 
***************************************************************************  
**  Program  : FSexplorer, part of DSMRloggerAPI
**
**  Mostly stolen from https://www.arduinoforum.de/User-Fips
**  For more information visit: https://fipsok.de
**  See also https://www.arduinoforum.de/arduino-Thread-LITTLEFS-DOWNLOAD-UPLOAD-DELETE-Esp8266-NodeMCU
**
***************************************************************************      
  Copyright (c) 2018 Jens Fleischer. All rights reserved.

  This file is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This file is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
*******************************************************************
*/
#include <uri/UriBraces.h>
const PROGMEM char Header[] = "HTTP/1.1 303 OK\r\nLocation:/#FSExplorer\r\nCache-Control: no-cache\r\n";

void sendApiResponse(const ApiResponse& response) {
  httpServer.sendHeader("Access-Control-Allow-Origin", "*");
  if (response.body.length() == 0) {
    httpServer.send(response.status);
    return;
  }
  httpServer.send(response.status, response.contentType, response.body);
}

ApiRequestContext currentApiRequest(const String& pathArg = "") {
  ApiRequestContext request;
  request.method = httpServer.method();
  request.pathArg = pathArg;
  request.body = httpServer.arg(0);
  request.uri = httpServer.uri();
  return request;
}

bool auth() {
  if (strlen(bAuthUser) && !httpServer.authenticate(bAuthUser, bAuthPW)) {
    httpServer.requestAuthentication();
    return false;
  }
  return true;
}

int shellyRpcRequestId() {
  if (!httpServer.hasArg("id")) return 1;
  return httpServer.arg("id").toInt();
}

void serveStaticWithAuth(const char* uri, const char* fileName) {
  httpServer.on(uri, HTTP_GET, [fileName]() {
    if (!auth()) return;
    File file = LittleFS.open(fileName, "r");
    if (file) {
      httpServer.sendHeader("Access-Control-Allow-Origin", "*");
      httpServer.streamFile(file, "application/json");
      file.close();
    } else {
      httpServer.send(404, "text/plain", "File Not Found");
    }
  });
}

static void handleApiDevSettingsGet() {
  if (!auth()) return;
  sendDeviceSettingsJson();
}

static void handleApiV1DataGet() {
  if (!auth()) return;
  sendHWapiJson();
}

static void handleApiSmActualGet() {
  if (!auth()) return;
  sendSmActualJson();
}

static void handleApiSmFieldGet() {
  if (!auth()) return;
  sendSmFieldJson(httpServer.pathArg(0));
}

static void handleRootGet() {
  if (!auth()) return;
  EnsureIndexFilePresent();

  File file = LittleFS.open(settingIndexPage, "r");
  if (file) {
    httpServer.streamFile(file, "text/html");
    file.close();
    return;
  }

  httpServer.send(404, "text/plain", F("FileNotFound\r\n"));
}

static void handleHttpNotFound() {
  if (!auth()) return;

#if DIRECT_AP_CONNECT
  httpServer.send(404, "text/plain", F("FileNotFound\r\n"));
  return;
#endif

  if (Verbose2) DebugTf("in 'onNotFound()'!! [%s] => \r\n", String(httpServer.uri()).c_str());
  DebugTf("next: handleFile(%s)\r\n", String(httpServer.urlDecode(httpServer.uri())).c_str());
  String filename = httpServer.uri();
  DebugT("Filename: ");Debugln(filename);

  bool handle = handleFile(filename.c_str());
  if (filename == "/" && !handle) {
    httpServer.sendHeader("Location", "/#DashTab", true);
    httpServer.send(303, "text/plain", "");
  }
  if (!handle) httpServer.send(404, "text/plain", F("FileNotFound\r\n"));
}

void setupFSexplorer() { 

#if !DIRECT_AP_CONNECT
  if (FSNotPopulated) {
    DebugTln(F("FS not populated -> API operation only\r"));
  } else {
    DebugTln(F("FS correct populated -> normal operation!\r"));
    httpServer.on("/", HTTP_GET, handleRootGet);
    httpServer.serveStatic("/", LittleFS, settingIndexPage);
  }
#else
  DebugTln(F("DirectAP closed-network mode -> API only\r"));
#endif
  
#if !DIRECT_AP_CONNECT
  serveStaticWithAuth("/api/v2/hist/hours", RingFiles[RINGHOURS].filename);
  serveStaticWithAuth("/api/v2/hist/days", RingFiles[RINGDAYS].filename);
  serveStaticWithAuth("/api/v2/hist/months", RingFiles[RINGMONTHS].filename);

  httpServer.on("/api/v2/hist/months", HTTP_POST, [](){
    if (!auth()) return;
    sendApiResponse(historyMonthsApiResponse(httpServer.arg(0)));
  });
#endif

  httpServer.on("/logout", HTTP_GET, []() { httpServer.send(401); });
  httpServer.on("/login",  HTTP_GET, []() { auth(); });

  httpServer.on("/api",             HTTP_GET, []() { if (!auth()) return; sendApiResponse({200, "application/json", HWrootJson()}); });
  httpServer.on("/api/v1/telegram", HTTP_GET, []() { if (!auth()) return; sendApiResponse({200, "text/plain", CapTelegram}); });
  httpServer.on("/api/v1/data",     HTTP_GET, handleApiV1DataGet);
  httpServer.on("/api/v2/stats",    HTTP_GET, []() { if (!auth()) return; sendApiResponse({200, "application/json", apiStatsJson()}); });
  httpServer.on("/api/v2/dash/live", HTTP_GET, []() { if (!auth()) return; sendApiResponse(dashLiveApiResponse()); });
  httpServer.on("/api/v2/dash/hist", HTTP_GET, []() { if (!auth()) return; sendApiResponse(dashHistoryApiResponse()); });

  httpServer.on("/api/v2/dev/settings", HTTP_GET, handleApiDevSettingsGet);
  httpServer.on("/api/v2/sm/actual",    HTTP_GET, handleApiSmActualGet);
  httpServer.on(UriBraces("/api/v2/sm/fields/{}"), handleApiSmFieldGet);
  httpServer.on(UriBraces("/api/v2/dev/{}"), []() {
    if (!auth()) return;
    sendApiResponse(handleDevApi(currentApiRequest(httpServer.pathArg(0))));
  });
  httpServer.on(UriBraces("/api/v2/sm/{}"), []() {
    if (!auth()) return;
    sendApiResponse(handleSmApi(currentApiRequest(httpServer.pathArg(0))));
  });
  httpServer.on("/api/v2/modbus/monitor", HTTP_GET, []() {
    if (!auth()) return;
    sendApiResponse(handleModbusMonitorApi(currentApiRequest()));
  });
  httpServer.on("/api/v2/modbus/monitor", HTTP_POST, []() {
    if (!auth()) return;
    sendApiResponse(handleModbusMonitorApi(currentApiRequest()));
  });

  httpServer.on("/eid/getclaim", []() {
    if (!auth()) return;
    String action = httpServer.hasArg("action") ? httpServer.arg("action") : "";
    sendApiResponse(EIDGetClaimApiResponse(action));
  });
  httpServer.on("/eid/planner", []() {
    if (!auth()) return;
    sendApiResponse(JsonEIDplanner());
  });

#if !DIRECT_AP_CONNECT
  httpServer.on("/api/listfiles", HTTP_GET, []() {
    if (!auth()) return;
    sendApiResponse(listFilesApiResponse());
  });
#endif
  httpServer.on("/api/v2/gen", HTTP_GET, []() {
    if (!auth()) return;
    sendApiResponse(solarApiResponse());
  });
  httpServer.on("/api/v2/accu", HTTP_GET, []() {
    if (!auth()) return;
    sendApiResponse(accuApiResponse());
  });
#if !DIRECT_AP_CONNECT
  httpServer.on("/FSformat", []() {
    if (!auth()) return;
    formatFS();
  });
  httpServer.on("/upload", HTTP_POST, []() {
    if (!auth()) return;
  }, handleFileUpload);
#endif
  httpServer.on("/ReBoot", []() {
    if (!auth()) return;
    reBootESP();
  });
  httpServer.on("/ResetWifi", []() {
    if (!auth()) return;
    resetWifi();
  });
  httpServer.on("/remote-update", []() { if (!auth()) return; RemoteUpdate(); });
  httpServer.onNotFound(handleHttpNotFound);
  httpServer.begin();
  DebugTln(F("HTTP server started\r"));
  
} // setupFSexplorer()

ApiResponse listFilesApiResponse()
{   
  typedef struct _fileMeta {
    char    Name[30];     
    int32_t Size;
  } fileMeta;

  _fileMeta dirMap[30];
  int fileNr = 0;
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  while (file && (fileNr < 30))  
  {
    dirMap[fileNr].Name[0] = '\0';
    strlcpy(dirMap[fileNr].Name, file.name(), sizeof(dirMap[fileNr].Name));
    dirMap[fileNr].Size = file.size();
    fileNr++;
    file = root.openNextFile();
  }

  for (int8_t y = 0; y < fileNr; y++) {
    yield();
    for (int8_t x = y + 1; x < fileNr; x++)  {
      if (compare(String(dirMap[x].Name), String(dirMap[y].Name)))  
      {
        fileMeta temp = dirMap[y];
        dirMap[y] = dirMap[x];
        dirMap[x] = temp;
      }
    }
  }

  for (int8_t x = 0; x < fileNr; x++)  
  {
    DebugTln(dirMap[x].Name);
  }

  JsonDocument doc;
  JsonArray files = doc.to<JsonArray>();
  for (int f = 0; f < fileNr; f++)
  {
    JsonObject item = files.add<JsonObject>();
    item["name"] = dirMap[f].Name;
    item["size"] = formatBytes(dirMap[f].Size);
  }
  JsonObject fsStats = files.add<JsonObject>();
  fsStats["usedBytes"] = formatBytes(LittleFS.usedBytes() * 1.05);
  fsStats["totalBytes"] = formatBytes(LittleFS.totalBytes());
  fsStats["freeBytes"] = formatBytes(LittleFS.totalBytes() - (LittleFS.usedBytes() * 1.05));
  String body;
  serializeJson(doc, body);
  return {200, "application/json", body};
  
} // listFilesApiResponse()

bool handleFile(String&& path) 
{
  if (!LittleFS.exists(settingIndexPage)) GetFile(settingIndexPage, PATH_DATA_FILES); 
  if (httpServer.hasArg("delete")) 
  {
    DebugTf("Delete -> [%s]\n\r", httpServer.arg("delete").c_str());
    LittleFS.remove(httpServer.arg("delete"));
    httpServer.sendContent(Header);
    return true;
  }
  if (path.endsWith("/")) path += "index.html";
  return LittleFS.exists(path) ? ({File f = LittleFS.open(path, "r"); httpServer.streamFile(f, contentType(path)); f.close(); true;}) : false;

} // handleFile()

void handleFileUpload() 
{
  if (!auth()) return;
  static File fsUploadFile;
  HTTPUpload& upload = httpServer.upload();
  if (upload.status == UPLOAD_FILE_START) 
  {
    if (upload.filename.length() > 30) 
    {
      upload.filename = upload.filename.substring(upload.filename.length() - 30, upload.filename.length());
    }
    Debugln("FileUpload Name: " + upload.filename);
    fsUploadFile = LittleFS.open("/" + httpServer.urlDecode(upload.filename), "w");
  } 
  else if (upload.status == UPLOAD_FILE_WRITE) 
  {
    Debugln("FileUpload Data: " + (String)upload.currentSize);
    if (fsUploadFile) fsUploadFile.write(upload.buf, upload.currentSize);
  } 
  else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) fsUploadFile.close();
    Debugln("FileUpload Size: " + (String)upload.totalSize);
    httpServer.sendContent(Header);
    RngInvalidateHeaderCache(httpServer.urlDecode(upload.filename).c_str());
    if (upload.filename == "DSMRsettings.json") readSettings(false);
    if (upload.filename == "enphase.json" || upload.filename == "solaredge.json") ReadSolarConfigs();
#ifdef NETSWITCH
    if (upload.filename == "netswitch.json") readtriggers();
#endif    
  }
} // handleFileUpload() 

void formatFS() 
{
  if (!LittleFS.exists("/!format")) return;
  DebugTln(F("Format FS"));
  LittleFS.format();
  httpServer.sendContent(Header);
  
} // formatFS()

const String formatBytes(size_t const& bytes) 
{ 
  return (bytes < 1024) ? String(bytes) + " Byte" : (bytes < (1024 * 1024)) ? String(bytes / 1024.0) + " KB" : String(bytes / 1024.0 / 1024.0) + " MB";

} //formatBytes()

const String &contentType(String& filename) 
{       
  if (filename.endsWith(".htm") || filename.endsWith(".html")) filename = "text/html";
  else if (filename.endsWith(".css")) filename = "text/css";
  else if (filename.endsWith(".js")) filename = F("application/javascript");
  else if (filename.endsWith(".json")) filename = F("application/json");
  else if (filename.endsWith(".xml")) filename = F("text/xml");
  else filename = "text/plain";
  return filename;
  
} // &contentType()

bool freeSpace(uint16_t const& printsize) 
{    
  Debugln(formatBytes(LittleFS.totalBytes() - (LittleFS.usedBytes() * 1.05)) + " in SPIFF free");
  return (LittleFS.totalBytes() - (LittleFS.usedBytes() * 1.05) > printsize) ? true : false;
  
} // freeSpace()

void resetWifi()
{
  Debugf("\r\nConnect to AP [%s] and go to ip address shown in the AP-name\r\n", settingHostname);
  doRedirect("RebootResetWifi", 500, "/", true, true);
}

void reBootESP()
{
  DebugTln(F("Redirect and ReBoot .."));
  doRedirect("RebootOnly", 500, "/", true, false);

} // reBootESP()

void doRedirect(String msg, int wait, const char* URL, bool reboot, bool resetWifi)
{ 
  DebugTln(msg);
  delay(wait);
  httpServer.sendHeader("Location", "/#Redirect?msg=" + msg, true);
  httpServer.send(303, "text/plain", "");

  if (reboot) 
  {
    for (uint8_t i = 0; i < 100; i++) { 
      delay(50); 
      httpServer.handleClient();
    }
    if (resetWifi) 
    {   
      WiFiManager manageWiFi;
      manageWiFi.resetSettings();
#if DIRECT_AP_CONNECT
      preferences.remove("direct_ap_ssid");
      preferences.remove("direct_ap_psk");
#endif
    }
    LogFile("reboot: doRedirect", false);
    P1Reboot();
  }
  
} // doRedirect()
