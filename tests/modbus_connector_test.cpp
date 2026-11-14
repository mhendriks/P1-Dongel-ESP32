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
}
