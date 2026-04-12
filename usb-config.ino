#ifdef USB_CONFIG
#include <esp_system.h>
#include <esp_chip_info.h>
#include <rom/rtc.h>
#include "DSMRloggerAPI.h"

extern Preferences preferences;
static String wifiSSID, wifiPSK;

static const char* KEYS[] = {
  "wifi_ssid",
  "wifi_password"
};

void sendJson(JsonDocument& doc) {
  serializeJson(doc, USBSerial);
  USBSerial.write('\n');
}

void replyError(const char* msg){
  StaticJsonDocument<160> out; 
  out["ok"]=false; 
  out["err"]=msg; 
  sendJson(out);
}

void fillSysInfo(JsonDocument& out){
  auto sys = out.createNestedObject("sys");

  sys["fw"]["name"] = "DSMR-API";
  sys["fw"]["ver"]  = _VERSION_ONLY;
  sys["fw"]["build_date"] = __DATE__;
  sys["fw"]["build_time"] = __TIME__;
  
  sys["wifi"]["connected"] = WiFi.isConnected();
  sys["wifi"]["ip"] = WiFi.localIP().toString();

  sys["hw"]["type"] = HardwareType ? HWTypeNames[HardwareType] : PROFILE;
  // sys["hw"]["module"] = "RS485";

  #ifdef NRGD
    sys["hw"]["module"] = ModTypeNames[modType[0]];
  #else 
    #ifdef ULTRA 
        if ( HardwareType == P1UX2 ||  HardwareType == P1U ) sys["hw"]["module"] = String(ModTypeNames[modType[0]]) + String(ModTypeNames[modType[1]]);
        else sys["hw"]["module"] = "P1U V1 modules";
    #else 
        sys["hw"]["module"] = "-";
    #endif
  #endif

  sys["uptime_ms"]        = uptime();
  sys["reset_reason"]     = getResetReason();

  sys["heap"]["free"]     = (uint32_t)ESP.getFreeHeap();
  sys["heap"]["size"]     = (uint32_t)ESP.getHeapSize();
 
  sys["flash"]["chip_size"]   = (uint32_t)ESP.getFlashChipSize();
  
  sys["mac"]["base"] = macStr;

  sys["p1"]["telegram_ok"]  = telegramCount;   // yourCounter;
  sys["p1"]["telegram_err"] = telegramErrors;    // yourCounterErr;

}

void handleLine(const String& line) {
  JsonDocument in;
  JsonDocument out;

  DeserializationError err = deserializeJson(in, line);
  if (err) { out["ok"]=false; out["err"]=err.c_str(); serializeJson(out, USBSerial); USBSerial.write('\n'); return; }

  const char* cmd = in["cmd"] | "";

  // if (!strcmp(cmd,"ping")) { out["ok"]=true; out["pong"]=millis(); sendJson(out); return; }

  if (!strcmp(cmd,"list")) {
    out["ok"]=true; JsonArray a = out.createNestedArray("keys");
    for (auto k: KEYS) a.add(k);
    sendJson(out); 
    return;
  }

  if (!strcmp(cmd,"get")) {
    const char* k = in["k"] | "";
    if (!*k) return replyError("missing key");

    if (!strcmp(k, "wifi_ssid")) out["v"] = WiFi.SSID(); 
    else if (!strcmp(k, "wifi_password")) out["v"] = WiFi.psk(); 
    else {
      out["v"]=preferences.getString(k, "");
    }
    out["ok"]=true; 
    out["k"]=k; 
    sendJson(out); 
    return;
  }

  if (!strcmp(cmd,"set")) {
    const char* k = in["k"] | "";
    bool ok = true; //saving  true as default
    if (!*k || !in.containsKey("v")) return replyError("missing k/v");
    String v = in["v"].as<String>();
    if (!strcmp(k, "wifi_ssid")) {wifiSSID = v;}
    else if (!strcmp(k, "wifi_password")) {wifiPSK = v;}
    else {
      ok = preferences.putString(k, v) >= 0;
    }
    out["ok"]=ok; out["k"]=k; out["saved"]=ok; 
    sendJson(out); 
    return;
  }

  // if (!strcmp(cmd,"del")) {
  //   const char* k = in["k"] | "";
  //   if (!*k) return replyError("missing key");
  //   bool ok = prefs.remove(k);
  //   out["ok"]=ok; out["k"]=k; out["deleted"]=ok; sendJson(out); return;
  // }

  if (!strcmp(cmd,"wifi")) { 
    out["ok"]=true; 
    sendJson(out);  
    if ( WiFi.isConnected() ) WiFi.disconnect();
    Debugln(">USB - WIFI: start wifi");
    Debugln(wifiSSID);
    Debugln(wifiPSK);
    if ( wifiSSID.length() && wifiPSK.length() ) { Debugln(">USB - WIFI: start wifi");WiFi.begin(wifiSSID.c_str(),wifiPSK.c_str());}
    return; 
  }
  if (!strcmp(cmd,"reboot")) { out["ok"]=true; sendJson(out); delay(100); ESP.restart(); return; }
  // if (!strcmp(cmd,"start_wifi")) { out["ok"]=true; sendJson(out); delay(100); WiFi.begin(ssid,psk); return; }

  if (!strcmp(cmd, "sysinfo")) {
    out["ok"] = true;
    fillSysInfo(out);
    sendJson(out);
    return;
  }

  out["ok"]=false; out["err"]="unknown cmd";
  // serializeJson(out, USBSerial); USBSerial.write('\n');
  sendJson(out);

}

void CheckAllPrefs() {

  // auto ensureDefault = [](const char* key, const char* defVal) {
  //   String v = preferences.getString(key, "");
  //   if (v == "") {
  //     preferences.putString(key, defVal);
  //   }
  // };

  // ensureDefault("wifi_ssid", WiFi.SSID());
  // ensureDefault("wifi_password",  WiFi.psk());

}

void fUSBConfig( void * pvParameters ){
  DebugTln(F("Enable usbconfig..."));
  CheckAllPrefs();
  while(true) {
    USBconfigLoop();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
  LogFile("fUSBConfig: unexpected task exit", true);
  vTaskDelete(NULL);
}

void USBconfigBegin(){
  if( xTaskCreate( fUSBConfig, "usb-config", 1024*4, NULL, 6, NULL ) == pdPASS ) DebugTln(F("Task fUSBConfig succesfully created"));
}

void USBconfigLoop(){
  static String buf;
  if (USBSerial.isConnected() && USBSerial.isPlugged() ){
    while (USBSerial.available()) {
      char c = (char)USBSerial.read();
      if (c=='\n') { handleLine(buf); buf=""; }
      else if (c!='\r') { buf += c; if (buf.length() > 1024) buf = ""; }
    }
  }
}

#else
 void USBconfigLoop(){}
 void USBconfigBegin(){}
#endif