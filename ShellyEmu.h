#pragma once
#ifdef SHELLY_EMU

//Emulates UDP EMG3 Shelly device

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncUDP.h>
#include <ArduinoJson.h>

// ShellyEmuUDP shellyUDP;
namespace P1 {
  inline float powerImportkW()   { return (float) DSMRdata.power_delivered.val(); }  // DSMR 1-0:1.7.0 (kW)
  inline float powerExportkW()   { return DSMRdata.power_returned.val(); }  // DSMR 1-0:2.7.0 (kW)
  inline float importTotalkWh()  { return DSMRdata.energy_delivered_tariff1.val() + DSMRdata.energy_delivered_tariff2.val(); } // DSMR 1-0:1.8.0
  inline float exportTotalkWh()  { return DSMRdata.energy_returned_tariff1.val() + DSMRdata.energy_returned_tariff2.val(); }  // DSMR 1-0:2.8.0

  inline float pL1()             { return DSMRdata.power_delivered_l1_present ? 1.0 * ( DSMRdata.power_delivered_l1.int_val() - DSMRdata.power_returned_l1.int_val()) : NAN; }
  inline float pL2()             { return DSMRdata.power_delivered_l2_present ? 1.0 * ( DSMRdata.power_delivered_l2.int_val() - DSMRdata.power_returned_l2.int_val()) : NAN; }
  inline float pL3()             { return DSMRdata.power_delivered_l3_present ? 1.0 * ( DSMRdata.power_delivered_l3.int_val() - DSMRdata.power_returned_l3.int_val()) : NAN; }
  
  inline float uL1()             { return DSMRdata.voltage_l1_present?DSMRdata.voltage_l1.val():NAN; }    // DSMR 32.7.0 (V)
  inline float iL1()             { return  DSMRdata.current_l1_present?DSMRdata.current_l1.val():NAN; }    // DSMR 31.7.0 (A)
  inline float uL2()             { return DSMRdata.voltage_l2_present?DSMRdata.voltage_l2.val():NAN; }    // DSMR 32.7.0 (V)
  inline float iL2()             { return  DSMRdata.current_l2_present?DSMRdata.current_l2.val():NAN; }    // DSMR 31.7.0 (A)
  inline float uL3()             { return DSMRdata.voltage_l3_present?DSMRdata.voltage_l3.val():NAN; }    // DSMR 32.7.0 (V)
  inline float iL3()             { return  DSMRdata.current_l3_present?DSMRdata.current_l3.val():NAN; }    // DSMR 31.7.0 (A)
}

class ShellyEmuUDP {
public:
  void begin(bool also2220 = true) {
    _id     = makeId();
    // _macStr = WiFi.macAddress();          // "AA:BB:CC:DD:EE:FF"
    // _macHex = macNoColons(_macStr);       // "AABBCCDDEEFF"

    _udp1010.listen(1010);
    _udp1010.onPacket([this](AsyncUDPPacket p){ handlePacket(p); });

    if (also2220) {
      _udp2220.listen(2220);
      _udp2220.onPacket([this](AsyncUDPPacket p){ handlePacket(p); });
    }
  }

private:
  AsyncUDP _udp1010, _udp2220;
  String _id;

  // ====== helpers implementatie ======
  static String macNoColons(const String& mac) {
    String s; s.reserve(12);
    for (size_t i=0;i<mac.length();++i) if (mac[i] != ':') s += mac[i];
    return s;
  }
  static String makeId() {
    String s = "p1-dongle-";
    s += macID;
    return s;
  }
  static bool eqMethod(const char* m, const char* want){
    if(!m) return false;
    String s(m); s.trim();
    return s.equals(want);
  }

  void fillDeviceInfo(JsonObject dst){
    dst["id"]    = _id;
    dst["model"] = "ShellyEMG3";
    dst["mac"]   = macID;  // zonder ':'
    dst["arch"]  = "ESP32";
    dst["fw_id"] = "2025.1.0";
    dst["ver"]   = "3.1.0";
    dst["app"]   = "EM";
  }

inline void fillStatus( JsonObject dst, bool EM_only ){
  // ====== P1 metingen ======
  const float pImp_kW   = P1::powerImportkW();   // DSMR 1-0:1.7.0
  const float pExp_kW   = P1::powerExportkW();   // DSMR 1-0:2.7.0
  const float P1        = P1::pL1();            // W (optioneel aanwezig)
  const float P2        = P1::pL2();            // W (optioneel aanwezig)
  const float P3        = P1::pL3();            // W (optioneel aanwezig)
  const float U1        = P1::uL1();            // V (optioneel aanwezig)
  const float I1        = P1::iL1();            // A (optioneel aanwezig)
  const float U2        = P1::uL2();            // V (optioneel aanwezig)
  const float I2        = P1::iL2();            // A (optioneel aanwezig)
  const float U3        = P1::uL3();            // V (optioneel aanwezig)
  const float I3        = P1::iL3();            // A (optioneel aanwezig)

  // Netto actief vermogen (Shelly: + import, − export), in W
  const float P = 1000.0f * (pImp_kW - pExp_kW);

  // Energiestanden (Wh, Shelly verwacht Wh)
  const long  Ea_Wh = lroundf(1000.0f * P1::importTotalkWh()); // 1.8.0
  const long  Er_Wh = lroundf(1000.0f * P1::exportTotalkWh()); // 2.8.0

  // ====== sys ======
  if ( !EM_only )
  {
    ArduinoJson::JsonObject sys = dst["sys"].to<ArduinoJson::JsonObject>();
    sys["mac"]      = macID;                         // "64E8336A4AC8"
    sys["device"]   = _id;                             // "shellyemg3-64e8336a4ac8"
    sys["uptime"]   = uptime();                 // s

    time_t now = 0;
    #ifdef ARDUINO_ARCH_ESP32
      now = time(nullptr); // 0 als geen NTP — prima
    #endif
    sys["unixtime"] = now;

    sys["cfg_rev"]  = 1;
    sys["fw_id"]    = "2025.1.0";
    sys["fw_ver"]   = "3.1.0";
    sys["app"]      = "EM";
    sys["model"]    = "ShellyEMG3";
  }

  // ====== em ======
  {
    ArduinoJson::JsonObject em;
    if ( EM_only ) em = dst;
    else em = dst["em"].to<ArduinoJson::JsonObject>();
    em["id"]                            = 0;   // kanaal 0
    if (isfinite(P1)) em["a_act_power"] = P1;   // W (+ import / − export)
    if (isfinite(P2)) em["b_act_power"] = P2;   // W (+ import / − export)
    if (isfinite(P3)) em["c_act_power"] = P3;   // W (+ import / − export)

    if (isfinite(U1)) em["a_voltage"] = U1;   // V (optioneel)
    if (isfinite(I1)) em["a_current"] = I1;   // A (optioneel)
    if (isfinite(U2)) em["b_voltage"] = U2;   // V (optioneel)
    if (isfinite(I2)) em["b_current"] = I2;   // A (optioneel)
    if (isfinite(U3)) em["c_voltage"] = U3;   // V (optioneel)
    if (isfinite(I3)) em["c_current"] = I3;   // A (optioneel)
    
    em["freq"]                = 50.0f;      // Hz (constante fallback)
    em["a_total_act_energy"]  = Ea_Wh;      // Wh import
    em["a_total_ret_energy"]  = Er_Wh;      // Wh export
    em["total_act_power"]     = P;  
    em["total_current"]       = I1+I2+I3 ? I1+I2+I3 : 0;
    em["n_current"]           = em["total_current"];
  }

  // ====== wifi ======
  if ( !EM_only )
  {
    ArduinoJson::JsonObject wifi = dst["wifi"].to<ArduinoJson::JsonObject>();
    wifi["sta_ip"] = WiFi.localIP().toString();
    wifi["rssi"]   = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;
    wifi["ssid"]   = WiFi.SSID();
    wifi["status"] = (WiFi.status() == WL_CONNECTED) ? "got ip" : "disconnected";
  }
}
  void handlePacket(AsyncUDPPacket packet) {
    // JSON parsen rechtstreeks uit het UDP-buffer (lengte-aware)
    JsonDocument req;  // ArduinoJson 7 dynamisch
    DeserializationError e = deserializeJson(req, packet.data(), packet.length());

    if (e) {
      // Niet-JSON -> discovery/probe -> los DeviceInfo-object terug
      JsonDocument di;
      fillDeviceInfo(di.to<JsonObject>());
      String out; serializeJson(di, out);
      packet.write((const uint8_t*)out.c_str(), out.length());
      return;
    }

    // Method veilig uitlezen (trimmen)
    const char* mraw = req["method"];
    String method = mraw ? String(mraw) : String();
    method.trim();

    Debug("UDP method: ");Debugln(method);

    // Standaard RPC-reply skelet
    JsonDocument resp;

    if (method.equals(F("EM.GetStatus"))) {
      fillStatus(resp.to<JsonObject>(), true);
      String out; serializeJson(resp, out);
      packet.write((const uint8_t*)out.c_str(), out.length());
      return;
    }
    //only for the other responses
    resp["id"]  = req["id"].isNull() ? 1 : req["id"];
    resp["src"] = "emu";

    if (method.equals(F("Shelly.GetDeviceInfo"))) {
      fillDeviceInfo(resp["result"].to<JsonObject>());
      String out; serializeJson(resp, out);
      packet.write((const uint8_t*)out.c_str(), out.length());
      return;
    }
    if (method.equals(F("Shelly.GetStatus"))) {
      fillStatus(resp["result"].to<JsonObject>(),false);
      String out; serializeJson(resp, out);
      packet.write((const uint8_t*)out.c_str(), out.length());
      return;
    }

    // Onbekende method -> minimaal deviceinfo als result
    //todo log call
    String log = "UDP: unsupported rpc -> ";
    log += method;
    LogFile(log.c_str(),true);
    fillDeviceInfo(resp["result"].to<JsonObject>());
    String out; serializeJson(resp, out);
    packet.write((const uint8_t*)out.c_str(), out.length());
  }
} shellyUDP;

void ShellyEmuBegin(){
  shellyUDP.begin(/*also2220=*/true);  // luistert op 1010 én 2220
}
#else
void ShellyEmuBegin(){}
#endif