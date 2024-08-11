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
const PROGMEM char Header[] = "HTTP/1.1 303 OK\r\nLocation:/#FileExplorer\r\nCache-Control: no-cache\r\n";

// Function to check authentication
bool auth() {
  if (strlen(bAuthUser) && !httpServer.authenticate(bAuthUser, bAuthPW)) {
    httpServer.requestAuthentication();
    return false;
  }
  return true;
}

// Function to handle static file serving with authentication
void serveStaticWithAuth(const char* uri, const char* fileName) {
  httpServer.on(uri, HTTP_GET, [fileName]() {
    if (auth()) {
      File file = LittleFS.open(fileName, "r");
      if (file) {
        httpServer.streamFile(file, "application/json");
        file.close();
      } else {
        httpServer.send(404, "text/plain", "File Not Found");
      }
    }
  });
}

//=====================================================================================
void setupFSexplorer()
{ 

 // Serve static files with authentication
  serveStaticWithAuth("/api/v2/hist/hours", RingFiles[RINGHOURS].filename);
  serveStaticWithAuth("/api/v2/hist/days", RingFiles[RINGDAYS].filename);
  serveStaticWithAuth("/api/v2/hist/months", RingFiles[RINGMONTHS].filename);

  httpServer.on("/api/v2/hist/months", HTTP_POST, [](){ writeRingFile(RINGMONTHS, httpServer.arg(0).c_str(), false); });

  httpServer.on("/logout", HTTP_GET, []() { httpServer.send(401); });
  httpServer.on("/login", HTTP_GET, []() { auth(); });
  httpServer.on(UriBraces("/api/v2/dev/{}"),[]() { auth(); handleDevApi(); });
  httpServer.on(UriBraces("/api/v2/sm/{}"),[](){ auth(); handleSmApi(); });
  httpServer.on(UriBraces("/api/v2/sm/fields/{}"),[](){ auth(); handleSmApiField(); });
  httpServer.on(UriBraces("/config/{}"), HTTP_POST, [](){auth(); ConfigApi(); });

#ifdef EID
  httpServer.on("/eid/getclaim",[](){ auth(); EIDGetClaim(); });
#endif
#ifdef DEV_PAIRING
  httpServer.on("/pair",[](){ HandlePairing(); });
#endif

  httpServer.on("/api/listfiles", HTTP_GET, [](){ auth(); APIlistFiles(); });
  httpServer.on("/api/v2/gen", HTTP_GET, [](){ auth(); SendSolarJson(); });
  httpServer.on("/FSformat", [](){ auth();formatFS; });
  httpServer.on("/upload", HTTP_POST, []() { auth(); }, handleFileUpload );
  httpServer.on("/ReBoot", [](){ auth();reBootESP(); });
  httpServer.on("/ResetWifi", [](){ auth(); resetWifi() ;});
  httpServer.on("/remote-update", [](){ auth(); RemoteUpdate(); });
  httpServer.onNotFound([]() 
  {
    auth();
    
    if (Verbose2) DebugTf("in 'onNotFound()'!! [%s] => \r\n", String(httpServer.uri()).c_str());
    DebugTf("next: handleFile(%s)\r\n", String(httpServer.urlDecode(httpServer.uri())).c_str());
    String filename = httpServer.uri();
    DebugT("Filename: ");Debugln(filename);
    if ( !handleFile(filename.c_str()) ) httpServer.send(404, "text/plain", F("FileNotFound\r\n"));
    
  });
  httpServer.begin();
  DebugTln( F("HTTP server started\r") );
  
} // setupFSexplorer()

void ConfigApi() {
  String FileName;
  
  if ( !httpServer.args() ) { httpServer.send(400); return;} //missing data
  String payload = httpServer.arg("plain");
  DebugTln(payload);
  DebugTln(httpServer.pathArg(0));
  if (httpServer.pathArg(0) == "enphase" ) FileName = "/enphase.json";
  if (httpServer.pathArg(0) == "solaredge" ) FileName = "/solaredge.json";
 
  if ( FileName.length() ) {
    File file = LittleFS.open(FileName.c_str(), "w");
    if (!file) {
      DebugTln(F("open file FAILED!!!\r\n"));
      httpServer.send(400);
    }  
    else {
      file.print(payload); 
      httpServer.send(200);
      ReadSolarConfigs();
    }  
    file.close();
  }
}  

//=====================================================================================
void APIlistFiles()             // Senden aller Daten an den Client
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
    strncpy( dirMap[fileNr].Name, file.name(), 30 );
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

  String temp = "[";
  for (int f=0; f < fileNr; f++)  
  {
    if (temp != "[") temp += ",";
    temp += R"({"name":")" + String(dirMap[f].Name) + R"(","size":")" + formatBytes(dirMap[f].Size) + R"("})";
  }

 temp += R"(,{"usedBytes":")" + formatBytes(LittleFS.usedBytes() * 1.05) + R"(",)" +       // Berechnet den verwendeten Speicherplatz + 5% Sicherheitsaufschlag
          R"("totalBytes":")" + formatBytes(LittleFS.totalBytes()) + R"(","freeBytes":")" + // Zeigt die Größe des Speichers
          (LittleFS.totalBytes() - (LittleFS.usedBytes() * 1.05)) + R"("}])";               // Berechnet den freien Speicherplatz + 5% Sicherheitsaufschlag

  httpServer.send(200, "application/json", temp);
  
} // APIlistFiles()

//=====================================================================================
bool handleFile(String&& path) 
{
//  Debugln("handleFile");
  if ( !LittleFS.exists(settingIndexPage) ) GetFile(settingIndexPage); 
  if (httpServer.hasArg("delete")) 
  {
    DebugTf("Delete -> [%s]\n\r",  httpServer.arg("delete").c_str());
    LittleFS.remove(httpServer.arg("delete"));    // Datei löschen
    httpServer.sendContent(Header);
    return true;
  }
  if (path.endsWith("/")) path += "index.html";
  return LittleFS.exists(path) ? ({File f = LittleFS.open(path, "r"); httpServer.streamFile(f, contentType(path)); f.close(); true;}) : false;

} // handleFile()

//=====================================================================================
void handleFileUpload() 
{
  static File fsUploadFile;
  HTTPUpload& upload = httpServer.upload();
  if (upload.status == UPLOAD_FILE_START) 
  {
    if (upload.filename.length() > 30) 
    {
      upload.filename = upload.filename.substring(upload.filename.length() - 30, upload.filename.length());  // Dateinamen auf 30 Zeichen kürzen
    }
    Debugln("FileUpload Name: " + upload.filename);
    fsUploadFile = LittleFS.open("/" + httpServer.urlDecode(upload.filename), "w");
  } 
  else if (upload.status == UPLOAD_FILE_WRITE) 
  {
    Debugln("FileUpload Data: " + (String)upload.currentSize);
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } 
  else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile)
      fsUploadFile.close();
    Debugln("FileUpload Size: " + (String)upload.totalSize);
    httpServer.sendContent(Header);
    if (upload.filename == "DSMRsettings.json") readSettings(false);
    if (upload.filename == "enphase.json" || upload.filename == "solaredge.json") ReadSolarConfigs();
  }
} // handleFileUpload() 

//=====================================================================================
void formatFS() 
{       //Formatiert den Speicher
  if (!LittleFS.exists("/!format")) return;
  DebugTln(F("Format FS"));
  LittleFS.format();
  httpServer.sendContent(Header);
  
} // formatFS()

//=====================================================================================
const String formatBytes(size_t const& bytes) 
{ 
  return (bytes < 1024) ? String(bytes) + " Byte" : (bytes < (1024 * 1024)) ? String(bytes / 1024.0) + " KB" : String(bytes / 1024.0 / 1024.0) + " MB";

} //formatBytes()

//=====================================================================================
const String &contentType(String& filename) 
{       
  if (filename.endsWith(".htm") || filename.endsWith(".html")) filename = "text/html";
  else if (filename.endsWith(".css")) filename = "text/css";
  else if (filename.endsWith(".js")) filename = F("application/javascript");
  else if (filename.endsWith(".json")) filename = F("application/json");
//  else if (filename.endsWith(".png")) filename = F("image/png");
//  else if (filename.endsWith(".gif")) filename = F("image/gif");
//  else if (filename.endsWith(".jpg")) filename = F("image/jpeg");
//  else if (filename.endsWith(".ico")) filename = F("image/x-icon");
  else if (filename.endsWith(".xml")) filename = F("text/xml");
//  else if (filename.endsWith(".pdf")) filename = F("application/x-pdf");
//  else if (filename.endsWith(".zip")) filename = F("application/x-zip");
//  else if (filename.endsWith(".gz")) filename = F("application/x-gzip");
  else filename = "text/plain";
  return filename;
  
} // &contentType()

//=====================================================================================
bool freeSpace(uint16_t const& printsize) 
{    
  Debugln(formatBytes(LittleFS.totalBytes() - (LittleFS.usedBytes() * 1.05)) + " in SPIFF free");
  return (LittleFS.totalBytes() - (LittleFS.usedBytes() * 1.05) > printsize) ? true : false;
  
} // freeSpace()

//=====================================================================================
//void updateFirmware()
//{
//  DebugTln(F("Redirect to updateIndex .."));
//  doRedirect("Update", 1, "/updateIndex", false,false);      
//} // updateFirmware()

//=====================================================================================
void resetWifi()
{
//  DebugTln(F("ResetWifi ..."));
  Debugf("\r\nConnect to AP [%s] and go to ip address shown in the AP-name\r\n", settingHostname);
  doRedirect("RebootResetWifi", 500, "/", true, true);
}
//=====================================================================================
void reBootESP()
{
  DebugTln(F("Redirect and ReBoot .."));
  doRedirect("RebootOnly", 500, "/", true,false);

} // reBootESP()

//=====================================================================================
void doRedirect(String msg, int wait, const char* URL, bool reboot, bool resetWifi)
{ 
  DebugTln(msg);
  delay(wait);
  httpServer.sendHeader("Location", "/#Redirect?msg="+msg, true);
  httpServer.send ( 303, "text/plain", "");

  if (reboot) 
  {
    for( uint8_t i = 0; i < 100;i++) { 
      //handle redirect requests first before rebooting so client processing can take place
      delay(50); 
      httpServer.handleClient();
    }
    if (resetWifi) 
   {   
       WiFiManager manageWiFi;
       manageWiFi.resetSettings();
   }
    P1Reboot();
  }
  
} // doRedirect()
