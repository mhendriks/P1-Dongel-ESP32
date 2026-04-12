#pragma once
#if ENABLE_MIMICS

// Emulates a Shelly Pro 3EM-style UDP/RPC surface for meter integrations.

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
    String s = "shellypro3em-";
    s += macID;
    return s;
  }
  static bool eqMethod(const char* m, const char* want){
    if(!m) return false;
    String s(m); s.trim();
    return s.equals(want);
  }

  void fillDeviceInfo(JsonObject dst){
    dst["name"]  = nullptr;
    dst["id"]    = _id;
    dst["mac"]   = macID;  // zonder ':'
    dst["slot"]  = 1;
    dst["model"] = "SPEM-003CEBEU";
    dst["gen"]   = 2;
    dst["fw_id"] = "20240819-074548/1.4.2-gc2639da";
    dst["ver"]   = "1.4.2";
    dst["app"]   = "Pro3EM";
    dst["auth_en"] = false;
    dst["auth_domain"] = nullptr;
    dst["profile"] = "triphase";
  }

  static float shareOfPositive(float phase, float total){
    if (total <= 0.0001f || !isfinite(phase) || phase <= 0.0f) return 0.0f;
    return phase / total;
  }

  static float shareOfNegative(float phase, float totalAbs){
    if (totalAbs <= 0.0001f || !isfinite(phase) || phase >= 0.0f) return 0.0f;
    return fabsf(phase) / totalAbs;
  }

  void fillEmDataComponent(JsonObject emdata){
    const float phaseA = P1::pL1();
    const float phaseB = P1::pL2();
    const float phaseC = P1::pL3();
    const float importTotal = P1::importTotalkWh();
    const float exportTotal = P1::exportTotalkWh();

    const float posTotal = (isfinite(phaseA) && phaseA > 0 ? phaseA : 0.0f)
                         + (isfinite(phaseB) && phaseB > 0 ? phaseB : 0.0f)
                         + (isfinite(phaseC) && phaseC > 0 ? phaseC : 0.0f);
    const float negTotalAbs = (isfinite(phaseA) && phaseA < 0 ? fabsf(phaseA) : 0.0f)
                            + (isfinite(phaseB) && phaseB < 0 ? fabsf(phaseB) : 0.0f)
                            + (isfinite(phaseC) && phaseC < 0 ? fabsf(phaseC) : 0.0f);

    const float aImport = importTotal * shareOfPositive(phaseA, posTotal);
    const float bImport = importTotal * shareOfPositive(phaseB, posTotal);
    const float cImport = importTotal * shareOfPositive(phaseC, posTotal);

    const float aExport = exportTotal * shareOfNegative(phaseA, negTotalAbs);
    const float bExport = exportTotal * shareOfNegative(phaseB, negTotalAbs);
    const float cExport = exportTotal * shareOfNegative(phaseC, negTotalAbs);

    emdata["id"] = 0;
    emdata["a_total_act_energy"] = aImport;
    emdata["a_total_act_ret_energy"] = aExport;
    emdata["b_total_act_energy"] = bImport;
    emdata["b_total_act_ret_energy"] = bExport;
    emdata["c_total_act_energy"] = cImport;
    emdata["c_total_act_ret_energy"] = cExport;
    emdata["total_act"] = importTotal;
    emdata["total_act_ret"] = exportTotal;
  }

  void fillSysComponent(JsonObject sys){
    sys["mac"] = macID;
    sys["restart_required"] = false;

    char timeBuf[6] = "--:--";
    if (year() > 2023) snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", hour(), minute());
    sys["time"] = timeBuf;

    time_t now = 0;
    #ifdef ARDUINO_ARCH_ESP32
      now = time(nullptr);
    #endif
    sys["unixtime"] = now;
    sys["uptime"] = uptime();
    sys["ram_size"] = ESP.getHeapSize();
    sys["ram_free"] = ESP.getFreeHeap();
    sys["fs_size"] = 524288;
    sys["fs_free"] = 180224;
    sys["cfg_rev"] = 11;
  }

  void fillWifiComponent(JsonObject wifi){
    wifi["sta_ip"] = WiFi.localIP().toString();
    wifi["rssi"] = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;
    wifi["ssid"] = WiFi.SSID();
    wifi["status"] = (WiFi.status() == WL_CONNECTED) ? "got ip" : "disconnected";
  }

  void fillWifiStaComponent(JsonObject wifi_sta){
    wifi_sta["connected"] = (WiFi.status() == WL_CONNECTED);
    wifi_sta["ssid"] = WiFi.SSID();
    wifi_sta["rssi"] = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;
  }

  void fillEmComponent(JsonObject em){
    const float pImp_kW = P1::powerImportkW();
    const float pExp_kW = P1::powerExportkW();
    const float phaseA = P1::pL1();
    const float phaseB = P1::pL2();
    const float phaseC = P1::pL3();
    const float voltA = P1::uL1();
    const float currA = P1::iL1();
    const float voltB = P1::uL2();
    const float currB = P1::iL2();
    const float voltC = P1::uL3();
    const float currC = P1::iL3();

    const float aprtA = (isfinite(voltA) && isfinite(currA)) ? fabsf(voltA * currA) : NAN;
    const float aprtB = (isfinite(voltB) && isfinite(currB)) ? fabsf(voltB * currB) : NAN;
    const float aprtC = (isfinite(voltC) && isfinite(currC)) ? fabsf(voltC * currC) : NAN;

    const float pfA = (isfinite(phaseA) && isfinite(aprtA) && aprtA > 0.001f) ? constrain(phaseA / aprtA, -1.0f, 1.0f) : 0.0f;
    const float pfB = (isfinite(phaseB) && isfinite(aprtB) && aprtB > 0.001f) ? constrain(phaseB / aprtB, -1.0f, 1.0f) : 0.0f;
    const float pfC = (isfinite(phaseC) && isfinite(aprtC) && aprtC > 0.001f) ? constrain(phaseC / aprtC, -1.0f, 1.0f) : 0.0f;

    const float totalActPower = 1000.0f * (pImp_kW - pExp_kW);
    const float totalAprtPower = (isfinite(aprtA) ? aprtA : 0.0f) + (isfinite(aprtB) ? aprtB : 0.0f) + (isfinite(aprtC) ? aprtC : 0.0f);
    const float totalCurrent = (isfinite(currA) ? currA : 0.0f) + (isfinite(currB) ? currB : 0.0f) + (isfinite(currC) ? currC : 0.0f);

    em["id"] = 0;
    if (isfinite(phaseA)) em["a_act_power"] = phaseA;
    if (isfinite(voltA)) em["a_voltage"] = voltA;
    if (isfinite(currA)) em["a_current"] = currA;
    if (isfinite(aprtA)) em["a_aprt_power"] = aprtA;
    em["a_pf"] = pfA;
    em["a_freq"] = 50.0f;

    if (isfinite(phaseB)) em["b_act_power"] = phaseB;
    if (isfinite(voltB)) em["b_voltage"] = voltB;
    if (isfinite(currB)) em["b_current"] = currB;
    if (isfinite(aprtB)) em["b_aprt_power"] = aprtB;
    em["b_pf"] = pfB;
    em["b_freq"] = 50.0f;

    if (isfinite(phaseC)) em["c_act_power"] = phaseC;
    if (isfinite(voltC)) em["c_voltage"] = voltC;
    if (isfinite(currC)) em["c_current"] = currC;
    if (isfinite(aprtC)) em["c_aprt_power"] = aprtC;
    em["c_pf"] = pfC;
    em["c_freq"] = 50.0f;

    em["total_act_power"] = totalActPower;
    em["total_aprt_power"] = totalAprtPower;
    em["total_current"] = totalCurrent;
    em["n_current"] = nullptr;
    em.createNestedArray("user_calibrated_phase");
    em.createNestedArray("errors");
  }

  void fillShellyStatus(JsonObject dst){
    JsonObject cloud = dst["cloud"].to<JsonObject>();
    cloud["connected"] = false;

    JsonObject mqtt = dst["mqtt"].to<JsonObject>();
    mqtt["connected"] = false;

    JsonObject sys = dst["sys"].to<JsonObject>();
    fillSysComponent(sys);

    JsonObject em = dst["em:0"].to<JsonObject>();
    fillEmComponent(em);

    JsonObject emdata = dst["emdata:0"].to<JsonObject>();
    fillEmDataComponent(emdata);
  }

  void fillRpcStatus(JsonObject dst){
    fillShellyStatus(dst);
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

    resp["id"]  = req["id"].isNull() ? 1 : req["id"];
    resp["src"] = _id;

    if (method.equals(F("EM.GetStatus"))) {
      fillEmComponent(resp["result"].to<JsonObject>());
      String out; serializeJson(resp, out);
      packet.write((const uint8_t*)out.c_str(), out.length());
      return;
    }
    if (method.equals(F("EMData.GetStatus"))) {
      fillEmDataComponent(resp["result"].to<JsonObject>());
      String out; serializeJson(resp, out);
      packet.write((const uint8_t*)out.c_str(), out.length());
      return;
    }
    //only for the other responses

    if (method.equals(F("Shelly.ListMethods"))) {
      JsonArray methods = resp["result"].to<JsonArray>();
      methods.add("EM.GetStatus");
      methods.add("EMData.GetStatus");
      methods.add("Shelly.GetDeviceInfo");
      methods.add("Shelly.GetStatus");
      methods.add("Shelly.ListMethods");
      String out; serializeJson(resp, out);
      packet.write((const uint8_t*)out.c_str(), out.length());
      return;
    }

    if (method.equals(F("Shelly.GetDeviceInfo"))) {
      fillDeviceInfo(resp["result"].to<JsonObject>());
      String out; serializeJson(resp, out);
      packet.write((const uint8_t*)out.c_str(), out.length());
      return;
    }
    if (method.equals(F("Shelly.GetStatus"))) {
      fillRpcStatus(resp["result"].to<JsonObject>());
      String out; serializeJson(resp, out);
      packet.write((const uint8_t*)out.c_str(), out.length());
      return;
    }
    if (method.equals(F("status")) || method.equals(F("Shelly.Status"))) {
      fillShellyStatus(resp.to<JsonObject>());
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
  if ( skipNetwork ) return;
  if ( !isShellyPro3EmMimicSelected() ) return;
  shellyUDP.begin(/*also2220=*/true);  // luistert op 1010 én 2220
}
#else
void ShellyEmuBegin(){}
#endif
