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
  static String makeId() {
    String s = "shellypro3em-";
    s += macID;
    return s;
  }

  static float astraDecimalEnforcedPower(float power) {
    const float decimalPointEnforcer = 0.001f;
    if (fabsf(power) < 0.1f) {
      return decimalPointEnforcer;
    }
    const float rounded = roundf(power);
    return roundf((power + ((power == rounded || power == 0.0f) ? decimalPointEnforcer : 0.0f)) * 10.0f) / 10.0f;
  }

  static float astraTotalPower(float totalPower) {
    const float rounded = roundf(totalPower);
    totalPower = roundf(totalPower * 1000.0f) / 1000.0f;
    if (totalPower == rounded || totalPower == 0.0f) {
      totalPower += 0.001f;
    }
    return totalPower;
  }

  static void setJsonNumber(JsonObject& obj, const char* key, float value, uint8_t decimals) {
    char buf[24];
    dtostrf(value, 0, decimals, buf);
    char* start = buf;
    while (*start == ' ') start++;
    obj[key] = serialized(String(start));
  }

  void fillAstraEmStatus(JsonDocument& doc, int requestId) {
    const float phaseA = isfinite(P1::pL1()) ? P1::pL1() : 0.0f;
    const float phaseB = isfinite(P1::pL2()) ? P1::pL2() : 0.0f;
    const float phaseC = isfinite(P1::pL3()) ? P1::pL3() : 0.0f;
    const float voltA = P1::uL1();
    const float voltB = P1::uL2();
    const float voltC = P1::uL3();
    const float currA = P1::iL1();
    const float currB = P1::iL2();
    const float currC = P1::iL3();

    doc["id"] = requestId;
    doc["src"] = _id.length() ? _id : makeId();
    doc["dst"] = "unknown";

    JsonObject result = doc["result"].to<JsonObject>();
    setJsonNumber(result, "a_act_power", astraDecimalEnforcedPower(phaseA), 1);
    setJsonNumber(result, "b_act_power", astraDecimalEnforcedPower(phaseB), 1);
    setJsonNumber(result, "c_act_power", astraDecimalEnforcedPower(phaseC), 1);
    if (isfinite(voltA)) setJsonNumber(result, "a_voltage", voltA, 1);
    if (isfinite(voltB)) setJsonNumber(result, "b_voltage", voltB, 1);
    if (isfinite(voltC)) setJsonNumber(result, "c_voltage", voltC, 1);
    if (isfinite(currA)) setJsonNumber(result, "a_current", currA, 3);
    if (isfinite(currB)) setJsonNumber(result, "b_current", currB, 3);
    if (isfinite(currC)) setJsonNumber(result, "c_current", currC, 3);
    setJsonNumber(result, "total_current",
                  (isfinite(currA) ? currA : 0.0f)
                + (isfinite(currB) ? currB : 0.0f)
                + (isfinite(currC) ? currC : 0.0f), 3);
    setJsonNumber(result, "total_act_power", astraTotalPower(phaseA + phaseB + phaseC), 3);
  }

  void fillAstraEm1Status(JsonDocument& doc, int requestId) {
    const float phaseA = isfinite(P1::pL1()) ? P1::pL1() : 0.0f;
    const float phaseB = isfinite(P1::pL2()) ? P1::pL2() : 0.0f;
    const float phaseC = isfinite(P1::pL3()) ? P1::pL3() : 0.0f;

    doc["id"] = requestId;
    doc["src"] = _id.length() ? _id : makeId();
    doc["dst"] = "unknown";

    JsonObject result = doc["result"].to<JsonObject>();
    setJsonNumber(result, "act_power", astraTotalPower(phaseA + phaseB + phaseC), 3);
  }

  void handlePacket(AsyncUDPPacket packet) {
    JsonDocument req;  // ArduinoJson 7 dynamisch
    DeserializationError e = deserializeJson(req, packet.data(), packet.length());

    if (e) {
      return;
    }

    if (!req["params"]["id"].is<int>()) {
      return;
    }

    const char* mraw = req["method"];
    String method = mraw ? String(mraw) : String();
    method.trim();

    JsonDocument resp;
    resp["id"]  = req["id"].isNull() ? 1 : req["id"];
    resp["src"] = _id;

    if (method.equals(F("EM.GetStatus"))) {
      fillAstraEmStatus(resp, resp["id"].as<int>());
      String out; serializeJson(resp, out);
      packet.write((const uint8_t*)out.c_str(), out.length());
      return;
    }
    if (method.equals(F("EM1.GetStatus"))) {
      fillAstraEm1Status(resp, resp["id"].as<int>());
      String out; serializeJson(resp, out);
      packet.write((const uint8_t*)out.c_str(), out.length());
      return;
    }

    // Onbekende method -> negeren zoals bij AstraMeter
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
