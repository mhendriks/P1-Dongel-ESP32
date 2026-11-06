"""Exercise the firmware's reactive sources with the installed DSMR parser."""
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
LIB = Path(os.environ.get('DSMR3_LIB_DIR', ROOT.parent / 'libraries/dsmr3Lib'))


class PhoenixReactiveTest(unittest.TestCase):
    def test_phoenix_word_order_on_wire(self):
        firmware = (ROOT / '_mbus.ino').read_text()
        selection = re.search(r'activeRecipeLswFirst = \(mappingChoice.*?;', firmware).group()
        serializer = re.search(r'static void addMbU32\(.*?\n\}', firmware, re.S).group()
        em24 = re.search(r'static constexpr int kModbusMappingEm24Tcp = \d+;', firmware).group()
        emodbus = Path(os.environ.get('EMODBUS_LIB_DIR', ROOT.parent / 'libraries/eModbus'))
        header = (emodbus / 'src/ModbusMessage.h').read_text()
        # Use eModbus's real integer serializers, without Arduino networking.
        add_methods = header.split('  template <class T> uint16_t add(T v) {')[1].split('// get()')[0]
        add_methods = '  template <class T> uint16_t add(T v) {' + add_methods
        source = '''#include <cstdint>
#include <cassert>
#include <vector>
#include <type_traits>
struct ModbusMessage {
  std::vector<uint8_t> MM_data;
''' + add_methods + '\n};\n' + em24 + '''
bool activeRecipeLswFirst;
void selectMapping(int mappingChoice) {
''' + selection + '\n}\n' + serializer + '''
int main() {
  for (int mapping = 0; mapping <= 16; ++mapping) {
    selectMapping(mapping);
    ModbusMessage response;
    // Phoenix documentation: FLOAT32 123.456 = 0x42F6E979.
    addMbU32(response, 0x42F6E979u);
    const std::vector<uint8_t> lowFirst = {0xE9, 0x79, 0x42, 0xF6};
    const std::vector<uint8_t> highFirst = {0x42, 0xF6, 0xE9, 0x79};
    assert(response.MM_data == ((mapping == 4 || mapping == 6 || mapping == 16) ? lowFirst : highFirst));
  }
  selectMapping(6);
  ModbusMessage integer;
  addMbU32(integer, 0x12345678u);
  assert((integer.MM_data == std::vector<uint8_t>{0x56, 0x78, 0x12, 0x34}));
  ModbusMessage frequency;
  addMbU32(frequency, 0x42480000u); // FLOAT32 50 Hz at register 32780
  assert((frequency.MM_data == std::vector<uint8_t>{0x00, 0x00, 0x42, 0x48}));
  ModbusMessage singleRegister;
  singleRegister.add((uint16_t)0x1234);
  assert((singleRegister.MM_data == std::vector<uint8_t>{0x12, 0x34}));
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'word_order.cpp'
            path.write_text(source)
            binary = Path(tmp) / 'word_order'
            subprocess.run(['c++', '-std=c++11', str(path), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)

    def test_parsing_and_firmware_sources(self):
        firmware = (ROOT / '_mbus.ino').read_text()
        data_header = (ROOT / 'DSMRloggerAPI.h').read_text()
        enum = re.search(r'enum class MbSource : uint8_t \{.*?\n\};', firmware, re.S).group()
        data_type = re.search(r'using MyData = ParsedData<.*?\n>;', data_header, re.S).group()
        cases = firmware.split('    case MbSource::reactive_energy_import_kvarh:')[1]
        cases = '    case MbSource::reactive_energy_import_kvarh:' + cases.split('    case MbSource::reactive_energy_total_varh:')[0]
        # Compile the actual source branches, so checks cover firmware behavior
        # as well as the library's unit normalization and presence flags.
        source = '''#include <dsmr3.h>
#include <cmath>
#include <cassert>
#include <cstdint>
''' + enum + '\n' + data_type + '''
MyData DSMRdata;
float outputEnergy(float value) { return value; }
float outputPower(float value) { return value; }
float readReactive(MbSource source) {
  switch (source) {
''' + cases + '''    default: return NAN;
  }
}
int main() {
  assert(std::isnan(readReactive(MbSource::reactive_energy_import_kvarh)));
  const char telegram[] = "/TEST\\r\\n"
    "1-0:3.8.0(004.618*kVArh)\\r\\n"
    "1-0:4.8.0(38020.696*kVArh)\\r\\n!";
  auto result = P1Parser::parse(&DSMRdata, telegram, sizeof(telegram) - 1);
  assert(!result.err);
  assert(std::fabs(readReactive(MbSource::reactive_energy_import_kvarh) * 1000 - 4618) < 0.01);
  assert(std::fabs(readReactive(MbSource::reactive_energy_export_kvarh) * 1000 - 38020696) < 4);
  DSMRdata.reactive_energy_total_import._value = 0;
  assert(readReactive(MbSource::reactive_energy_import_kvarh) == 0);
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'test.cpp'
            path.write_text(source)
            binary = Path(tmp) / 'test'
            subprocess.run(['c++', '-std=c++11', '-I' + str(LIB / 'tests/support'), '-I' + str(LIB / 'src'), str(path), str(LIB / 'src/dsmr3/fields3.cpp'), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)

    def test_mapping_units_and_firmware_table(self):
        rows = json.loads((ROOT / 'modbus_mappings/modbus_mx3xx_mapping.json').read_text())
        table = (ROOT / '_mbus_mapping.h').read_text().split('static const ActiveRecipe kMx3xxRecipes[] = {')[1].split('};')[0]
        compiled = re.findall(r'\{(\d+),\s*(-?\d+),\s*\(uint8_t\)MbSource::(\w+),\s*\(uint8_t\)ModbusDataType::(\w+),\s*(-?\d+)\}', table)
        self.assertEqual([(int(a), int(b), c, d.lower(), int(e)) for a, b, c, d, e in compiled], [(r['register'], r['scale'], r['source'], r['type'], r.get('value', 0)) for r in rows])
        by_address = {r['register']: r for r in rows}
        for address, source in {33026: 'reactive_energy_import_kvarh', 33032: 'reactive_energy_export_kvarh'}.items():
            self.assertEqual(by_address[address]['source'], source)
            self.assertEqual(by_address[address]['scale'], 1000)


if __name__ == '__main__':
    unittest.main()
