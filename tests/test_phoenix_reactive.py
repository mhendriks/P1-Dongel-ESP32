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
