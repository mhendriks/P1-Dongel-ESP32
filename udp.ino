#ifdef UDP

#include <AsyncUDP.h>

static constexpr uint16_t P1_UDP_PORT = 4343;

static AsyncUDP  udp_4343;
static uint32_t  g_udpSeq = 0;
// static bool udpReady = false;

template <typename T>
static float valOrZero(bool present, const T& v) {
  return present ? (float)v : 0.0f;
}

// Call once after WiFi is connected
bool UdpBegin() {
  if (skipNetwork) return false;
  // 1) Universele broadcast:
  IPAddress bcast(255, 255, 255, 255);

  // 2) Subnet broadcast:
  // IPAddress bcast = calcBroadcastIP();

  if (!udp_4343.connect(bcast, P1_UDP_PORT)) {
    return false;
  }
  return true;
}

// Zend 1 UDP JSON bericht (compact keys: meta: p/v/d/fw/u/s/t, data: p/u/i)
bool udpSendP1Json() {
  if (skipNetwork) return false;
  if (WiFi.status() != WL_CONNECTED) return false;

  // Signed power (delivered - returned)
  const float p1 = DSMRdata.power_delivered_l1_present
                 ? ((float)DSMRdata.power_delivered_l1 - (float)DSMRdata.power_returned_l1)
                 : 0.0f;

  const float p2 = DSMRdata.power_delivered_l2_present
                 ? ((float)DSMRdata.power_delivered_l2 - (float)DSMRdata.power_returned_l2)
                 : 0.0f;

  const float p3 = DSMRdata.power_delivered_l3_present
                 ? ((float)DSMRdata.power_delivered_l3 - (float)DSMRdata.power_returned_l3)
                 : 0.0f;

  const float pT = p1 + p2 + p3;

  JsonDocument doc;
  doc.reserve(850); // compact payload, 850 is meestal genoeg

  // ---- meta ----
  JsonObject meta = doc["meta"].to<JsonObject>();
  meta["p"]  = "p1-udp";                 // proto
  meta["v"]  = "1.0";                    // version
  meta["d"]  = HWTypeNames[HardwareType]; // device/hw type string
  // meta["id"] = ...;                    // optioneel device-id/serial
  meta["fw"] = _VERSION_ONLY;
  meta["u"]  = (uint32_t)uptimeSEC();
  meta["s"]  = g_udpSeq++;
  meta["t"]  = (uint32_t)actT;           // epoch seconds (UTC)

  // ---- data ----
  JsonObject data = doc["data"].to<JsonObject>();

  JsonObject power = data["p"].to<JsonObject>();
  power["l1"]    = p1;
  power["l2"]    = p2;
  power["l3"]    = p3;
  power["total"] = pT;

  JsonObject volt = data["u"].to<JsonObject>();
  volt["l1"] = valOrZero(DSMRdata.voltage_l1_present, DSMRdata.voltage_l1);
  volt["l2"] = valOrZero(DSMRdata.voltage_l2_present, DSMRdata.voltage_l2);
  volt["l3"] = valOrZero(DSMRdata.voltage_l3_present, DSMRdata.voltage_l3);

  JsonObject curr = data["i"].to<JsonObject>();
  curr["l1"] = valOrZero(DSMRdata.current_l1_present, DSMRdata.current_l1);
  curr["l2"] = valOrZero(DSMRdata.current_l2_present, DSMRdata.current_l2);
  curr["l3"] = valOrZero(DSMRdata.current_l3_present, DSMRdata.current_l3);

  // Serialize
  char out[1024];
  size_t n = serializeJson(doc, out, sizeof(out));
  if (n == 0) return false;

  // Send (broadcast)
  // write() retourneert aantal bytes geschreven
  return udp_4343.write((const uint8_t*)out, n) == n;
}

#else  // UDP not enabled

bool UdpBegin() { return false; }
bool udpSendP1Json() { return false; }

#endif
