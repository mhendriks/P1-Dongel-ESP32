"""Check actuals serialization against parsed meter and calculated currents."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
LIB = Path(os.environ.get('DSMR3_LIB_DIR', ROOT.parent / 'libraries/dsmr3Lib'))
JSON_LIB = ROOT.parent / 'libraries/ArduinoJson/src'


class ActualCurrentsTest(unittest.TestCase):
    def test_actuals_use_selected_current(self):
        header = (ROOT / 'DSMRloggerAPI.h').read_text()
        current_type = re.search(r'struct MeterCurrent \{.*?\n\};', header, re.S).group()
        builder = (ROOT / 'Json.ino').read_text().split('struct buildJson {')[1].split('}; // buildjson{}')[0]
        modbus = (ROOT / '_mbus.ino').read_text()
        enum = re.search(r'enum class MbSource : uint8_t \{.*?\n\};', modbus, re.S).group()
        raw_guard = modbus.split('static float readMbSourceValue(MbSource source) {')[1].split('  switch (source) {')[0]
        current_cases = modbus.split('    case MbSource::current_l1_a: {')[1].split('    case MbSource::gas_timestamp_epoch:')[0]
        modbus_reader = enum + '\nfloat outputCurrent(float value) { return value * ct; }\n' + r'''
float mbSignedCurrent(float current, float returned) { return current * (returned ? -1 : 1); }
float readCurrent(MbSource source) {
''' + raw_guard + '  switch(source) {\n    case MbSource::current_l1_a: {' + current_cases + r'''
    default: return NAN;
  }
}
'''
        udp = (ROOT / '_udp.ino').read_text().split('  // ---- current ----')[1].split('  p.dy.in =')[0]
        udp_reader = r'''
struct { struct { int i1 = 0, i2 = 0, i3 = 0; int pactive1 = 1, pactive2 = 1, pactive3 = 1; } dy; } p;
void updateUdpCurrent() {
''' + udp + '\n}\n'
        source = r'''
#include <dsmr3.h>
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 0
#include <ArduinoJson.h>
#include <cassert>
#include <cmath>
using MyData = ParsedData<voltage_l1, voltage_l2, voltage_l3,
  power_delivered_l1, power_delivered_l2, power_delivered_l3,
  power_returned_l1, power_returned_l2, power_returned_l3,
  current_l1, current_l2, current_l3>;
MyData DSMRdata;
bool try_calc_i = true;
bool onlyIfPresent = true;
unsigned ct = 1;
JsonDocument jsonDoc;
bool isInFieldsArray(const char*) { return true; }
uint32_t outputFactorForField(const char* name) {
  return strncmp(name, "current_", 8) == 0 ? ct : 1;
}
float outputCurrentMilliAmps(uint32_t value) { return value / 1000.0f * ct; }
''' + current_type + '\nMeterCurrent GetTelegramCurrent(uint8_t phase);\n' + (ROOT / 'Derived.ino').read_text() + '\nstruct buildJson {' + builder + '};\n' + modbus_reader + udp_reader + r''' 
void check(const char* field, double expected) {
  assert(std::fabs(jsonDoc[field]["value"].as<double>() - expected) < 0.001);
  assert(jsonDoc[field]["unit"].as<std::string>() == "A");
}
int main() {
  const char telegram[] = "/TEST\r\n"
    "1-0:32.7.0(230.0*V)\r\n"
    "1-0:52.7.0(240.0*V)\r\n"
    "1-0:72.7.0(230.0*V)\r\n"
    "1-0:21.7.0(00.345*kW)\r\n"
    "1-0:42.7.0(00.600*kW)\r\n"
    "1-0:61.7.0(00.000*kW)\r\n"
    "1-0:31.7.0(001*A)\r\n"
    "1-0:51.7.0(002*A)\r\n"
    "1-0:71.7.0(003*A)\r\n!";
  assert(!P1Parser::parse(&DSMRdata, telegram, sizeof(telegram) - 1).err);
  UpdateCalculatedCurrents();
  DSMRdata.applyEach(buildJson());
  check("current_l1", 1.5);
  check("current_l2", 2.5);
  assert(readCurrent(MbSource::current_l1_a) == 1.5f);
  assert(readCurrent(MbSource::current_total_a) == 7.0f);
  assert(readCurrent(MbSource::signed_current_l2_a) == -2.5f);
  updateUdpCurrent();
  assert(p.dy.i1 == 1500 && p.dy.i2 == 2500 && p.dy.i3 == 3000);
  check("current_l3", 3); // zero power retains the meter's current
  assert(DSMRdata.current_l1.int_val() == 1000);
  assert(GetTelegramCurrent(1).milliAmps == 1000);
  assert(!GetTelegramCurrent(1).calculated);
  assert(GetMeterCurrent(1).calculated);
  try_calc_i = false;
  UpdateCalculatedCurrents();
  DSMRdata.applyEach(buildJson());
  check("current_l1", 1);
  assert(readCurrent(MbSource::current_l1_a) == 1.0f);
  assert(readCurrent(MbSource::current_l2_a) == 2.0f);
  assert(readCurrent(MbSource::signed_current_l2_a) == -2.0f);
  assert(readCurrent(MbSource::current_total_a) == 6.0f);
  updateUdpCurrent();
  assert(p.dy.i1 == 1000);
  try_calc_i = true;
  ct = 2;
  DSMRdata.applyEach(buildJson());
  check("current_l1", 3);
  assert(GetTelegramCurrent(1).milliAmps == 1000);
  // A derived current is available even if the telegram lacks that current.
  DSMRdata.current_l1_present = false;
  DSMRdata.applyEach(buildJson());
  check("current_l1", 3);
  try_calc_i = false;
  jsonDoc.clear();
  DSMRdata.applyEach(buildJson());
  assert(jsonDoc["current_l1"].isNull());
  onlyIfPresent = false;
  DSMRdata.applyEach(buildJson());
  assert(jsonDoc["current_l1"]["value"].as<std::string>() == "-");
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'test.cpp'
            path.write_text(source)
            binary = Path(tmp) / 'test'
            subprocess.run(['c++', '-std=c++17', '-I' + str(LIB / 'tests/support'),
                            '-I' + str(LIB / 'src'), '-I' + str(JSON_LIB), str(path),
                            str(LIB / 'src/dsmr3/fields3.cpp'), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    unittest.main()
