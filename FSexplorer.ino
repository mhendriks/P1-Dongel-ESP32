/**************************************************************************  
**  Program  : FSexplorer, part of DSMRloggerAPI
**
**************************************************************************/

#include <WebAuthentication.h>
ApiResponse listFilesApiResponse();

void sendApiResponse(AsyncWebServerRequest* request, const ApiResponse& response) {
  AsyncWebServerResponse* out;
  if (response.body.length() == 0) out = request->beginResponse(response.status);
  else out = request->beginResponse(response.status, response.contentType, response.body);
  out->addHeader("Access-Control-Allow-Origin", "*");
  request->send(out);
}

static AsyncAuthenticationMiddleware basicAuth;

void setupBasicAuthMiddleware() {
  basicAuth.setAuthType(AsyncAuthType::AUTH_BASIC);
  basicAuth.setRealm("P1 Dongle");
  basicAuth.setAuthFailureMessage("Authentication failed");
  basicAuth.setUsername(bAuthUser);
  if (strlen(bAuthUser) && !strlen(bAuthPW)) basicAuth.setPasswordHash(generateBasicHash(bAuthUser, bAuthPW).c_str());
  else {
    basicAuth.setPassword(bAuthPW);
    basicAuth.generateHash();
  }
}

ApiRequestContext currentApiRequest(AsyncWebServerRequest* request, const String& pathArg = "", const String& body = "") {
  ApiRequestContext context;
  context.method = request->method();
  context.pathArg = pathArg;
  context.body = body;
  context.uri = request->url();
  return context;
}

static ApiResponse handleApiPost(AsyncWebServerRequest* request, const String& body) {
  const String uri = request->url();
  if (uri == "/api/v2/hist/months")     return historyMonthsApiResponse(body);
  if (uri == "/api/v2/modbus/monitor") return handleModbusMonitorApi(currentApiRequest(request, "", body));
  if (uri == "/api/v2/dev/settings")   return handleDevApi(currentApiRequest(request, "settings", body));
  return {404, "text/plain", "Not Found"};
}

static void handleApiPostRequest(AsyncWebServerRequest* request) {
  if (request->contentLength() == 0) {
    sendApiResponse(request, handleApiPost(request, ""));
    return;
  }
  if (!request->_tempObject) request->_tempObject = new String();
}

static void handleApiPostBody(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
  String* body = reinterpret_cast<String*>(request->_tempObject);
  if (!body) {
    body = new String();
    request->_tempObject = body;
  }
  if (index == 0) body->reserve(total);
  body->concat(reinterpret_cast<const char*>(data), len);
  if ((index + len) < total) return;

  String fullBody = *body;
  delete body;
  request->_tempObject = nullptr;
  sendApiResponse(request, handleApiPost(request, fullBody));
}

static bool pathArgFromUri(AsyncWebServerRequest* request, const char* prefix, String& pathArg) {
  pathArg = request->url().substring(strlen(prefix));
  return pathArg.length() > 0 && pathArg.indexOf('/') < 0;
}

static bool hasDeleteParam(AsyncWebServerRequest* request) {
  return request->hasParam("delete");
}

static void handleFileDelete(AsyncWebServerRequest* request) {
  const AsyncWebParameter* deleteParam = request->getParam("delete");
  if (deleteParam && deleteParam->value().length()) {
    DebugTf("Delete -> [%s]\n\r", deleteParam->value().c_str());
    LittleFS.remove(deleteParam->value());
  }
  request->redirect("/#FSExplorer");
}

static bool serveFallbackFile(AsyncWebServerRequest* request, String filename) {
  if (!LittleFS.exists(settingIndexPage)) GetFile(settingIndexPage, PATH_DATA_FILES);
  if (filename.endsWith("/")) filename += "index.html";
  if (!LittleFS.exists(filename)) return false;

  request->send(LittleFS, filename);
  return true;
}

static void handleNotFound(AsyncWebServerRequest* request) {
#if DIRECT_AP_CONNECT
  request->send(404, "text/plain", F("FileNotFound\r\n"));
#else
  if (Verbose2) DebugTf("in 'onNotFound()'!! [%s] => \r\n", request->url().c_str());
  DebugTf("next: handleFile(%s)\r\n", request->url().c_str());

  String filename = request->url();
  DebugT("Filename: "); Debugln(filename);

  bool handled = serveFallbackFile(request, filename);
  if (filename == "/" && !handled) {
    request->redirect("/#DashTab");
    return;
  }
  if (!handled) request->send(404, "text/plain", F("FileNotFound\r\n"));
#endif
}

static String asyncUploadFilename(AsyncWebServerRequest* request, const String& filename) {
  String decoded = request->urlDecode(filename);
  if (decoded.length() > 30) decoded = decoded.substring(decoded.length() - 30);
  return decoded;
}

static void handleUploadedFileConfig(const String& filename) {
  RngInvalidateHeaderCache(filename.c_str());
  if (filename == "DSMRsettings.json") readSettings(false);
  if (filename == "enphase.json" || filename == "solaredge.json") ReadSolarConfigs();
#ifdef NETSWITCH
  if (filename == "netswitch.json") readtriggers();
#endif
}

static void handleAsyncFileUpload(AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {
  if (request->getResponse()) return;

  if (index == 0) {
    String* uploadName = new String(asyncUploadFilename(request, filename));
    request->_tempObject = uploadName;
    Debugln("FileUpload Name: " + *uploadName);
    request->_tempFile = LittleFS.open("/" + *uploadName, "w");
    if (!request->_tempFile) {
      delete uploadName;
      request->_tempObject = nullptr;
      request->send(500, "text/plain", "File not available for writing");
      return;
    }
  }

  if (len && request->_tempFile) {
    Debugln("FileUpload Data: " + String(len));
    request->_tempFile.write(data, len);
  }

  if (final) {
    if (request->_tempFile) request->_tempFile.close();
    String* uploadName = reinterpret_cast<String*>(request->_tempObject);
    if (uploadName) {
      Debugln("FileUpload Size: " + String(index + len));
      handleUploadedFileConfig(*uploadName);
      delete uploadName;
      request->_tempObject = nullptr;
    }
  }
}

void setupFSexplorer() {
  setupBasicAuthMiddleware();

#if !DIRECT_AP_CONNECT
  if (FSNotPopulated) {
    DebugTln(F("FS not populated -> async API operation only\r"));
  } else {
    DebugTln(F("FS correct populated -> async normal operation!\r"));
    httpServer.on(AsyncURIMatcher::prefix("/"), AsyncWebRequestMethod::HTTP_GET, handleFileDelete).setFilter(hasDeleteParam).addMiddleware(&basicAuth);
    httpServer.serveStatic("/", LittleFS, "/").setDefaultFile(settingIndexPage[0] == '/' ? settingIndexPage + 1 : settingIndexPage).addMiddleware(&basicAuth);
    #if !DIRECT_AP_CONNECT
      httpServer.serveStatic("/api/v2/hist/hours",  LittleFS,  RingFiles[RINGHOURS].filename).addMiddleware(&basicAuth);
      httpServer.serveStatic("/api/v2/hist/days" ,  LittleFS,   RingFiles[RINGDAYS].filename).addMiddleware(&basicAuth);
      httpServer.serveStatic("/api/v2/hist/months", LittleFS, RingFiles[RINGMONTHS].filename).addMiddleware(&basicAuth);
      httpServer.on("/api/v2/hist/months", AsyncWebRequestMethod::HTTP_POST, handleApiPostRequest, nullptr, handleApiPostBody).addMiddleware(&basicAuth);
    #endif  
  }
#else
  DebugTln(F("DirectAP closed-network mode -> async API only\r"));
#endif

  httpServer.on("/logout",            AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* request) { request->send(401); });
  httpServer.on("/login",             AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* request) { request->send(200, "text/plain", "OK"); }).addMiddleware(&basicAuth);
  
  //HW api calls
  httpServer.on(AsyncURIMatcher::exact("/api"),               
                                      AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* request) { request->send(200, "application/json", HWrootJson()); }  ).addMiddleware(&basicAuth);
  httpServer.on("/api/v1/telegram",   AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* request) { request->send(200, "text/plain", CapTelegram); }).addMiddleware(&basicAuth);
  httpServer.on("/api/v1/data",       AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* request) { request->send(200, "application/json", HWapiJson()); }).addMiddleware(&basicAuth);
  httpServer.on("/api/v2/stats",      AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* request) { request->send(200, "application/json", apiStatsJson()); }).addMiddleware(&basicAuth);
  
  httpServer.on(AsyncURIMatcher::dir("/api/v2/sm/fields"),  AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* request) { String pathArg; if (!pathArgFromUri(request, "/api/v2/sm/fields/", pathArg)) { request->send(404, "text/plain", "Not Found"); return; } sendApiResponse(request, handleSmApiField(currentApiRequest(request, pathArg))); }).addMiddleware(&basicAuth);
  httpServer.on("/api/v2/sm",         AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* request) { String pathArg; if (!pathArgFromUri(request, "/api/v2/sm/", pathArg)) { request->send(404, "text/plain", "Not Found"); return; } sendApiResponse(request, handleSmApi(currentApiRequest(request, pathArg))); }).addMiddleware(&basicAuth);
  
  httpServer.on("/api/v2/dev",        AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* request) { String pathArg; if (!pathArgFromUri(request, "/api/v2/dev/", pathArg)) { request->send(404, "text/plain", "Not Found"); return; } sendApiResponse(request, handleDevApi(currentApiRequest(request, pathArg))); }).addMiddleware(&basicAuth);
  httpServer.on("/eid/getclaim",      AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* request) { const AsyncWebParameter* actionParam = request->getParam("action"); sendApiResponse(request, EIDGetClaimApiResponse(actionParam ? actionParam->value() : ""));}).addMiddleware(&basicAuth);
  httpServer.on("/eid/planner",       AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* request) { sendApiResponse(request, JsonEIDplanner()); }).addMiddleware(&basicAuth);

#if !DIRECT_AP_CONNECT
  httpServer.on("/api/listfiles",     AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* request) { sendApiResponse(request, listFilesApiResponse()); }).addMiddleware(&basicAuth);
  httpServer.on("/upload",            AsyncWebRequestMethod::HTTP_POST, [](AsyncWebServerRequest* request) { if (request->getResponse()) return; request->redirect("/#FSExplorer"); }, handleAsyncFileUpload).addMiddleware(&basicAuth);
#endif
  httpServer.on("/api/v2/modbus/monitor", AsyncWebRequestMethod::HTTP_POST, handleApiPostRequest, nullptr, handleApiPostBody).addMiddleware(&basicAuth);
  httpServer.on("/api/v2/dev/settings",   AsyncWebRequestMethod::HTTP_POST, handleApiPostRequest, nullptr, handleApiPostBody).addMiddleware(&basicAuth);
  
  httpServer.on("/api/v2/modbus/monitor", 
                                      AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* request) { sendApiResponse(request, handleModbusMonitorApi(ApiRequestContext{})); }).addMiddleware(&basicAuth);
  httpServer.on("/api/v2/gen",        AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* request) { sendApiResponse(request, solarApiResponse()); }).addMiddleware(&basicAuth);
  httpServer.on("/api/v2/accu",       AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* request) { sendApiResponse(request, accuApiResponse()); }).addMiddleware(&basicAuth);
  httpServer.on("/ReBoot",            AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* request) { request->send(200, "text/plain", ""); reBootESP(); }).addMiddleware(&basicAuth);
  httpServer.on("/ResetWifi",         AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* request) { request->send(200, "text/plain", ""); resetWifi(); }).addMiddleware(&basicAuth);
  httpServer.on("/remote-update",     AsyncWebRequestMethod::HTTP_GET, handleRemoteUpdateRequest).addMiddleware(&basicAuth);
  
  httpServer.onNotFound(handleNotFound);
  httpServer.catchAllHandler().addMiddleware(&basicAuth);

  httpServer.begin();
  DebugTln(F("Async HTTP server started\r"));
}

//=====================================================================================
static String formatBytesCommon(size_t bytes)
{
  return (bytes < 1024) ? String(bytes) + " Byte" : (bytes < (1024 * 1024)) ? String(bytes / 1024.0) + " KB" : String(bytes / 1024.0 / 1024.0) + " MB";
}

//=====================================================================================
ApiResponse listFilesApiResponse()             // Senden aller Daten an den Client
{   
  typedef struct _fileMeta {
    char    Name[30];     
    int32_t Size;
  } fileMeta;

  _fileMeta dirMap[30];
  int fileNr = 0;
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  while ( file && ( fileNr < 30 ) )  
  {
    dirMap[fileNr].Name[0] = '\0';
    strlcpy(dirMap[fileNr].Name, file.name(), sizeof(dirMap[fileNr].Name));
    dirMap[fileNr].Size = file.size();
    fileNr++;
    file = root.openNextFile();
  }

  // -- bubble sort dirMap op .Name--
  for (int8_t y = 0; y < fileNr; y++) {
    yield();
    for (int8_t x = y + 1; x < fileNr; x++)  {
      //DebugTf("y[%d], x[%d] => seq[y][%s] / seq[x][%s] ", y, x, dirMap[y].Name, dirMap[x].Name);
      if (compare(String(dirMap[x].Name), String(dirMap[y].Name)))  
      {
        //Debug(" !switch!");
        fileMeta temp = dirMap[y];
        dirMap[y] = dirMap[x];
        dirMap[x] = temp;
      } /* end if */
      //Debugln();
    } /* end for */
  } /* end for */

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
    item["size"] = formatBytesCommon(dirMap[f].Size);
  }
  JsonObject fsStats = files.add<JsonObject>();
  fsStats["usedBytes"] = formatBytesCommon(LittleFS.usedBytes() * 1.05); // +5% veiligheidsmarge
  fsStats["totalBytes"] = formatBytesCommon(LittleFS.totalBytes());
  fsStats["freeBytes"] = formatBytesCommon(LittleFS.totalBytes() - (LittleFS.usedBytes() * 1.05));
  String body;
  serializeJson(doc, body);
  return {200, "application/json", body};
  
} // listFilesApiResponse()

void resetWifi()
{
  Debugf("\r\nConnect to AP [%s] and go to ip address shown in the AP-name\r\n", settingHostname);
  WiFiManager manageWiFi;
  manageWiFi.resetSettings();
#if DIRECT_AP_CONNECT
  preferences.remove("direct_ap_ssid");
  preferences.remove("direct_ap_psk");
#endif
  LogFile("reboot: async resetWifi", false);
  P1Reboot();
}

void reBootESP()
{
  DebugTln(F("Async ReBoot .."));
  LogFile("reboot: async reBootESP", false);
  P1Reboot();
}
