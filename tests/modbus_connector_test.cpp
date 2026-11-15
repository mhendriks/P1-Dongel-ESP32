#include <cassert>
#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "../ConnectorModel.h"

// Exercise the production polling state machine with deterministic TCP replies.
static uint32_t nowMs = 100;
uint32_t millis() { return nowMs; }
using portMUX_TYPE = int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
class WiFiClient {};
class IPAddress { public: bool fromString(const char* ip) { return std::string(ip) == "192.168.1.2"; } };
enum Error { SUCCESS, FAILURE };
static constexpr uint8_t READ_HOLD_REGISTER = 3;
class ModbusMessage {
 public:
  std::vector<uint8_t> bytes;
  size_t size() const { return bytes.size(); }
  uint8_t operator[](size_t i) const { return bytes[i]; }
};
class ModbusClientTCP {
 public:
  static ModbusClientTCP* instance;
  std::function<void(ModbusMessage,uint32_t)> data;
  std::function<void(Error,uint32_t)> error;
  uint32_t token = 0;
  uint16_t reg = 0;
  uint8_t count = 0, id = 0;
  int requests = 0;
  Error queueResult = SUCCESS;
  ModbusClientTCP(WiFiClient&, int) { instance = this; }
  void onDataHandler(decltype(data) f) { data = f; }
  void onErrorHandler(decltype(error) f) { error = f; }
  void setTimeout(int,int) {}
  void begin() {}
  void setTarget(IPAddress,uint16_t) {}
  Error addRequest(uint32_t t,uint8_t unit,uint8_t,uint16_t address,uint8_t words) {
    token = t; id = unit; reg = address; count = words; requests++; return queueResult;
  }
  void reply(uint16_t first,uint16_t second = 0) {
    ModbusMessage msg;
    msg.bytes = {id, 3, uint8_t(count*2), uint8_t(first>>8), uint8_t(first)};
    if (count == 2) { msg.bytes.push_back(uint8_t(second>>8)); msg.bytes.push_back(uint8_t(second)); }
    data(msg, token);
  }
};
ModbusClientTCP* ModbusClientTCP::instance = nullptr;
#include "../ModbusConnectorClient.h"

int main() {
  double value;
  ConnectorField signedPower {842, CONNECTOR_S16, 0.1f, false};
  assert(decodeConnectorValue(signedPower, uint16_t(-1234), 0, value));
  assert(std::abs(value + 123.4) < 0.001);
  ConnectorField u32 {40093, CONNECTOR_U32, 1, false};
  assert(decodeConnectorValue(u32, 0xffff, 0xfffe, value) && value == 4294967294.0);
  u32.swap = true;
  assert(decodeConnectorValue(u32, 0x5678, 0x1234, value) && value == 0x12345678);
  ConnectorField f32 {1, CONNECTOR_F32, 1, false};
  assert(decodeConnectorValue(f32, 0x3fc0, 0, value) && value == 1.5);
  assert(!decodeConnectorValue(f32, 0x7fc0, 0, value)); // NaN
  assert(!applyConnectorExponent(value, 0x8000));
  value = 1000;
  assert(applyConnectorExponent(value, uint16_t(-1)) && value == 100);

  PvDailyBaseline baseline;
  assert(baseline.observe(100000, 20261114, 1));
  assert(!baseline.observe(100500, 20261114, 1) && baseline.yieldWh(100500) == 500);
  PvDailyBaseline restored = baseline; // Same record restored after a reboot.
  assert(!restored.observe(100700, 20261114, 1) && restored.yieldWh(100700) == 700);
  assert(restored.observe(100800, 20261115, 1) && restored.yieldWh(100800) == 0);
  assert(restored.observe(42, 20261115, 1) && restored.yieldWh(42) == 0); // Counter reset.
  assert(restored.observe(200, 20261115, 2) && restored.yieldWh(200) == 0); // Another inverter.

  ModbusConnectorClient connector;
  auto& tcp = *ModbusClientTCP::instance;
  BatteryConnectorConfig battery;
  ConnectorField fields[] = {battery.power, battery.soc, battery.state};
  double output[3] = {};
  connector.configure(battery, fields, 3, false);
  assert(!connector.poll(true, output) && tcp.requests == 0);
  battery.enabled = true;
  strcpy(battery.ip, "192.168.1.2");
  connector.configure(battery, fields, 3, false);
  assert(!connector.poll(false, output) && tcp.requests == 0);
  assert(!connector.poll(true, output) && tcp.reg == 842);
  const uint32_t oldToken = tcp.token;
  tcp.reply(uint16_t(-1200));
  assert(!connector.poll(true, output) && tcp.reg == 843);
  tcp.reply(75);
  assert(!connector.poll(true, output) && tcp.reg == 844);
  tcp.reply(2);
  assert(connector.poll(true, output));
  assert(output[0] == -1200 && output[1] == 75 && output[2] == 2);
  int requests = tcp.requests;
  assert(!connector.poll(true, output) && tcp.requests == requests);
  nowMs += 5000;
  connector.poll(true, output);
  tcp.error(FAILURE, tcp.token);
  assert(!connector.poll(true, output)); // Never publish a partial cycle.

  PvConnectorConfig pv;
  pv.enabled = true;
  strcpy(pv.ip, battery.ip);
  ConnectorField pvFields[] = {pv.power, pv.energy};
  connector.configure(pv, pvFields, 2, true);
  tcp.error(FAILURE, oldToken); // Response from the previous configuration is ignored.
  assert(!connector.poll(true, output) && tcp.reg == 40083);
  tcp.reply(1234);
  assert(!connector.poll(true, output) && tcp.reg == 40084);
  tcp.reply(uint16_t(-1));
  assert(!connector.poll(true, output) && tcp.reg == 40093 && tcp.count == 2);
  tcp.reply(0x1234, 0x5678);
  assert(!connector.poll(true, output) && tcp.reg == 40095);
  tcp.reply(0);
  assert(connector.poll(true, output));
  assert(std::abs(output[0] - 123.4) < 0.001 && output[1] == 0x12345678);
  nowMs += 5000;
  connector.poll(true, output);
  tcp.reply(0x8000); // SunSpec unavailable power.
  assert(!connector.poll(true, output));
  nowMs += 5000;
  connector.poll(true, output);
  tcp.reply(100);
  connector.poll(true, output);
  tcp.reply(0x8000); // SunSpec unavailable exponent.
  assert(!connector.poll(true, output));

  fields[0].reg = 0; // Address zero is a valid Modbus register.
  connector.configure(battery, fields, 3, false);
  connector.poll(true, output);
  assert(tcp.reg == 0);

  // Diagnostics are opt-in, bounded, and never change the polling contract.
  ModbusConnectorClient diagnosed;
  auto& link = *ModbusClientTCP::instance;
  ConnectorField batteryFields[] = {battery.power, battery.soc, battery.state};
  diagnosed.configure(battery, batteryFields, 3, false);
  assert(!diagnosed.diagnosticSnapshot().enabled);
  diagnosed.poll(true, output);
  link.error(FAILURE, link.token);
  diagnosed.poll(true, output);
  assert(diagnosed.diagnosticSnapshot().count == 0);
  assert(diagnosed.diagnosticSnapshot().failures == 0);
  diagnosed.setDiagnosticsEnabled(true);
  assert(std::string(diagnosed.diagnosticStatus(false)) == "no_network");
  nowMs += 5000;
  diagnosed.poll(true, output);
  link.error(static_cast<Error>(0xE0), link.token);
  diagnosed.poll(true, output);
  auto& diagnostic = diagnosed.diagnosticSnapshot();
  assert(diagnostic.lastError == 0xE0 && diagnostic.lastErrorRegister == 842);
  assert(diagnostic.failures == 1 && std::string(diagnosed.diagnosticStatus(true)) == "error");
  nowMs += 5000;
  diagnosed.poll(true, output);
  link.reply(1200); diagnosed.poll(true, output);
  link.reply(75); diagnosed.poll(true, output);
  link.reply(1); assert(diagnosed.poll(true, output));
  diagnosed.sampleAccepted();
  assert(diagnostic.hasSuccess && diagnostic.cycles == 1);
  assert(diagnostic.lastError == 0xE0); // Keep the previous error after recovery.
  assert(std::string(diagnosed.diagnosticStatus(true)) == "ok");
  nowMs += 21000;
  assert(std::string(diagnosed.diagnosticStatus(true)) == "stale");
  for (int i = 0; i < 20; ++i) diagnosed.sampleRejected(1);
  assert(diagnostic.count == ConnectorDiagnostics::capacity);
  assert(diagnostic.lastError == 0x103 && diagnostic.lastErrorRegister == 843);
  diagnosed.setDiagnosticsEnabled(false);
  assert(diagnostic.count == 0 && !diagnostic.hasSuccess && !diagnostic.hasError);
  assert(std::string(diagnosed.diagnosticStatus(true)) == "off");

  diagnosed.configure(pv, pvFields, 2, true);
  diagnosed.setDiagnosticsEnabled(true);
  diagnosed.poll(true, output);
  const uint32_t obsolete = link.token;
  link.reply(1000); diagnosed.poll(true, output);
  link.reply(uint16_t(-1)); diagnosed.poll(true, output);
  link.reply(1, 34464); diagnosed.poll(true, output); // 100000 Wh before scaling.
  link.reply(0); assert(diagnosed.poll(true, output));
  diagnosed.sampleAccepted();
  const auto& pvDiagnostic = diagnosed.diagnosticSnapshot();
  assert(pvDiagnostic.count == 4);
  assert(pvDiagnostic.entries[1].exponent && pvDiagnostic.entries[1].reg == 40084);
  assert(pvDiagnostic.entries[1].raw[0] == uint16_t(-1));
  assert(pvDiagnostic.entries[1].hasValue && pvDiagnostic.entries[1].value == 100);
  assert(pvDiagnostic.entries[3].value == 100000);
  nowMs += 5000;
  diagnosed.poll(true, output);
  link.error(static_cast<Error>(2), obsolete); // Old callbacks must not corrupt diagnostics.
  assert(pvDiagnostic.failures == 0);
  link.error(static_cast<Error>(2), link.token);
  diagnosed.poll(true, output);
  assert(pvDiagnostic.lastError == 2 && pvDiagnostic.lastErrorRegister == 40083);
  nowMs += 5000;
  diagnosed.poll(true, output);
  link.reply(100); diagnosed.poll(true, output);
  link.reply(0x8000); diagnosed.poll(true, output);
  assert(pvDiagnostic.lastError == 0x102 && pvDiagnostic.lastErrorRegister == 40084);
  assert(!pvDiagnostic.entries[(pvDiagnostic.head + ConnectorDiagnostics::capacity - 1) % ConnectorDiagnostics::capacity].hasValue);
  diagnosed.configure(pv, pvFields, 2, true);
  assert(pvDiagnostic.enabled && pvDiagnostic.count == 0); // Reset capture on mapping changes.

  nowMs += 5000;
  link.queueResult = static_cast<Error>(0xE8);
  diagnosed.poll(true, output);
  assert(pvDiagnostic.lastError == 0xE8 && pvDiagnostic.lastErrorRegister == 40083);
  link.queueResult = SUCCESS;
  nowMs += 5000;
  diagnosed.poll(true, output);
  ModbusMessage malformed;
  malformed.bytes = {uint8_t(link.id + 1), 3, 2, 0, 100};
  link.data(malformed, link.token);
  assert(!diagnosed.poll(true, output));
  assert(pvDiagnostic.lastError == 0x100 && pvDiagnostic.lastErrorRegister == 40083);
  const auto& malformedEntry = pvDiagnostic.entries[(pvDiagnostic.head + ConnectorDiagnostics::capacity - 1) % ConnectorDiagnostics::capacity];
  assert(!malformedEntry.hasRaw && !malformedEntry.hasValue);

}
