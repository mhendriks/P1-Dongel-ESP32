"""Check physical interface reporting across hardware types and revisions."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class HardwareFeaturesTest(unittest.TestCase):
    def test_firmware_capabilities(self):
        helper = (ROOT / 'Helper.ino').read_text()
        config = (ROOT / 'Config.h').read_text()
        source = '#include <cstdint>\n#include <cassert>\n#include <cstring>\n#define _DEFAULT_HOSTNAME "test"\n'
        source += helper.split('// ------------------ ENUMS & CONSTANTS ------------------ //')[1].split('// ------------------ GLOBAL VARIABLES')[0]
        source += config.split('// config MODULES')[1].split('//FETAURES')[0]
        source += 'HWtype HardwareType; uint16_t HardwareVersion;\n'
        source += helper[helper.index('static inline const dev_conf& DEVCONF()'):helper.index('#define _getChipId')]
        source += r"""
#define ULTRA
#define INPUT 0
#define OUTPUT 1
#define LOW 0
#define RXPIN 18
#define TXPIN 19
#define RTSPIN 20
int IOWater = -1, mb_rx = -1, mb_tx = -1, mb_rts = -1;
bool WtrMtr = false;
int senseReads = 0;
void pinMode(int, int) {}
int digitalRead(int) { ++senseReads; return 0; }
void digitalWrite(int, int) {}
void MBSetTermination(int) {}
"""
        source += helper[helper.index('enum ModuleType : int8_t'):helper.index('// old device mapping')]
        source += r"""
int main() {
  HardwareType = W1MC;
  for (uint16_t rev : {0, 100, 102, 199, 200, 201, 299, 300, 301, 399}) {
    HardwareVersion = rev;
    const int generation = rev / 100;
    assert(hardwareHasFixedRS485() == (generation == 3));
    assert(hardwareHasHanNve() == (generation != 2));
    assert(hardwareSlotCount() == (generation == 3 ? 0 : 1));
  }
  for (int type = UNDETECTED; type <= W1MC; ++type) {
    HardwareType = static_cast<HWtype>(type);
    HardwareVersion = 102;
    assert(hardwareHasFixedRS485() == (type == NRGDH || type == D1MC));
    assert(hardwareHasHanNve() == (type == NRGDH || type == D1MC || type == W1MC));
    const int slots = type == NRGD || type == W1MC ? 1 : type == P1U || type == P1UX2 ? 2 : 0;
    assert(hardwareSlotCount() == slots);
  }
  // A fixed RS485 has no module sense inputs; use the board's bus pins directly.
  HardwareType = W1MC;
  HardwareVersion = 300;
  DetectModule();
  assert(senseReads == 0);
  assert(modType[0] == MOD_RS485);
  assert(mb_rx == 7 && mb_tx == 5 && mb_rts == 6);
  HardwareVersion = 200;
  DetectModule();
  assert(senseReads == 2);
  assert(modType[0] == MOD_NONE);
  HardwareType = P1UX2;
  DetectModule();
  assert(senseReads == 6);
  assert(modType[0] == MOD_NONE && modType[1] == MOD_NONE);
  assert(!strcmp(modName(MOD_UNKNOWN), "UNKNOWN"));
  assert(!strcmp(modName(MOD_NONE), "NONE"));
  assert(!strcmp(modName(MOD_IOP), "IO+"));
  assert(!strcmp(modName(MOD_H20), "H2O"));
  assert(!strcmp(modName(MOD_RS485), "RS485"));
}
"""
        source = '#include <initializer_list>\n' + source
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'test.cpp'
            path.write_text(source)
            binary = Path(tmp) / 'test'
            subprocess.run(['c++', '-std=c++11', str(path), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    unittest.main()
