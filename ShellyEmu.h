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
  static uint16_t localPortForPacket(AsyncUDPPacket& packet) {
    return packet.isBroadcast() ? 0 : packet.localPort();
  }

  static String makeId() {
    String s = "shellypro3em-";
    s += macID;
    return s;
  }

  void logUdpEvent(AsyncUDPPacket& packet, const char* prefix, const String& details) {
    String log = prefix;
    log += " from ";
    log += packet.remoteIP().toString();
    log += ":";
    log += String(packet.remotePort());
    log += " on ";
    log += String(localPortForPacket(packet));
    if (details.length()) {
      log += " ";
      log += details;
    }
    LogFile(log.c_str(), true);
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

  void fillAstraEmStatus(JsonDocument& doc, int requestId) {
    const float phaseA = isfinite(P1::pL1()) ? P1::pL1() : 0.0f;
    const float phaseB = isfinite(P1::pL2()) ? P1::pL2() : 0.0f;
    const float phaseC = isfinite(P1::pL3()) ? P1::pL3() : 0.0f;

    doc["id"] = requestId;
    doc["src"] = _id.length() ? _id : makeId();
    doc["dst"] = "unknown";

    JsonObject result = doc["result"].to<JsonObject>();
    result["a_act_power"] = astraDecimalEnforcedPower(phaseA);
    result["b_act_power"] = astraDecimalEnforcedPower(phaseB);
    result["c_act_power"] = astraDecimalEnforcedPower(phaseC);
    result["total_act_power"] = astraTotalPower(phaseA + phaseB + phaseC);
  }

  void fillAstraEm1Status(JsonDocument& doc, int requestId) {
    const float phaseA = isfinite(P1::pL1()) ? P1::pL1() : 0.0f;
    const float phaseB = isfinite(P1::pL2()) ? P1::pL2() : 0.0f;
    const float phaseC = isfinite(P1::pL3()) ? P1::pL3() : 0.0f;

    doc["id"] = requestId;
    doc["src"] = _id.length() ? _id : makeId();
    doc["dst"] = "unknown";

    JsonObject result = doc["result"].to<JsonObject>();
    result["act_power"] = astraTotalPower(phaseA + phaseB + phaseC);
  }

  void handlePacket(AsyncUDPPacket packet) {
    JsonDocument req;  // ArduinoJson 7 dynamisch
    DeserializationError e = deserializeJson(req, packet.data(), packet.length());

    if (e) {
      logUdpEvent(packet, "Shelly UDP invalid JSON", "");
      return;
    }

    if (!req["params"]["id"].is<int>()) {
      String details = "ignored missing params.id";
      if (!req["method"].isNull()) {
        details += " method=";
        details += req["method"].as<String>();
      }
      logUdpEvent(packet, "Shelly UDP", details);
      return;
    }

    const char* mraw = req["method"];
    String method = mraw ? String(mraw) : String();
    method.trim();

    logUdpEvent(packet, "Shelly UDP recv", "method=" + method);

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

    // Onbekende method -> minimaal deviceinfo als result
    logUdpEvent(packet, "Shelly UDP unsupported", "method=" + method);
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
