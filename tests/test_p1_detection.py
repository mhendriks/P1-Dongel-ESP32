"""Prevent empty/garbled frames from locking P1 auto-baud or publishing zeros."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2] + '\n'


class P1DetectionTest(unittest.TestCase):
    def test_uart_settings_for_han_and_p1(self):
        p1 = (ROOT / 'P1.ino').read_text()
        source = r'''
#include <cstdint>
#include <cassert>
#define DebugT(...) ((void)0)
#define Debugf(...) ((void)0)
#define SERIAL_8N1 1
#define SERIAL_7E1 2
#define portTICK_PERIOD_MS 1
void vTaskDelay(int) {}
uint32_t esp_log_timestamp() { return 123; }
bool bPre40 = false;
int8_t RxP1 = 1, HanIO = 0, TxO1 = 4;
uint32_t telegramCount = 0, telegramErrors = 0, SerialLastChecked = 0;
struct SerialMock {
  uint32_t baud = 0, config = 0;
  int rx = -1, tx = -1;
  bool inverted = false;
  void end() {}
  void setRxBufferSize(int) {}
  void begin(uint32_t b, uint32_t c, int r, int t) { baud = b; config = c; rx = r; tx = t; }
  void setRxInvert(bool value) { inverted = value; }
  int available() { return 0; }
  int read() { return -1; }
} Serial1;
struct MeterMock {
  bool han = false;
  bool isHan() { return han; }
  void doChecksum(bool) {}
  void clearAll() {}
} smartMeter;
'''
        source += function(p1, 'void SetupP1In(')
        source += r'''
int main() {
  for (int old = 0; old <= 1; ++old) {
    bPre40 = old;
    smartMeter.han = true;
    SetupP1In();
#ifdef HAN_READER
    assert(Serial1.baud == 2400 && Serial1.config == SERIAL_8N1);
    assert(Serial1.rx == 0 && !Serial1.inverted);
#else
    assert(Serial1.baud == (old ? 9600 : 115200));
    assert(Serial1.rx == 1 && Serial1.inverted);
#endif
    smartMeter.han = false;
    SetupP1In();
    assert(Serial1.baud == (old ? 9600 : 115200));
    assert(Serial1.config == (old ? SERIAL_7E1 : SERIAL_8N1));
    assert(Serial1.rx == 1 && Serial1.inverted);
    assert(Serial1.tx == 4);
  }
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            cpp = Path(tmp) / 'uart.cpp'
            binary = Path(tmp) / 'uart'
            cpp.write_text(source)
            for flags in ([], ['-DHAN_READER']):
                subprocess.run(['c++', '-std=c++11', *flags, str(cpp), '-o', str(binary)], check=True)
                subprocess.run([str(binary)], check=True)

    def test_real_parser_noise_fallback_and_supported_meters(self):
        library = Path(os.environ.get('DSMR3LIB_PATH', Path.home() / 'Documents/Arduino/libraries/dsmr3Lib'))
        if not (library / 'tests/support/Arduino.h').exists():
            self.skipTest('Set DSMR3LIB_PATH to a dsmr3Lib checkout with native test support')
        p1 = (ROOT / 'P1.ino').read_text()
        header = (ROOT / 'DSMRloggerAPI.h').read_text()
        source = r'''
#include <dsmr3.h>
#include <cassert>
#include <cstdio>
#include <string>
#include "fixtures/p1_generator_telegrams.h"
#define DebugTln(...) ((void)0)
#define DebugTf(...) ((void)0)
#define Debugf(...) ((void)0)
#define Debugln(...) ((void)0)
#define LED_ON 0
#define LED_OFF 1
uint32_t telegramCount = 0, telegramErrors = 0, SerialLastChecked = 0;
uint32_t clockMs = 0;
bool bPre40 = true, bRawPort = false, bRawPortTelegramPending = false;
bool Out1Avail = false, showRaw = false, bHideP1Log = true, Verbose2 = false;
bool bP1offline = false;
unsigned P1error_cnt_sequence = 0, applied = 0, rejected = 0;
int baud = 9600;
String CapTelegram;
uint32_t esp_log_timestamp() { return clockMs; }
void ToggleLED(int) {}
void logP1FieldWarning(const P1FieldWarning&) {}
void RecordP1ParseError(bool, size_t, const String&) { ++rejected; }
void debugP1ParseError(bool, size_t, const String&) {}
void SetupP1In() {
  baud = bPre40 ? 9600 : 115200;
  telegramCount = telegramErrors = 0;
  SerialLastChecked = clockMs;
}
'''
        start = header.index('using MyData = ParsedData<')
        source += header[start:header.index('>;', start) + 2] + '\n'
        source += 'void applyParsedSmartMeterData(MyData&, bool) { ++applied; }\n'
        source += function(p1, 'static bool hasP1Measurements(')
        source += function(p1, 'static void handleParsedMeter(P1FixedReader<2500>&')
        source += function(p1, 'void CheckP1Signal(')
        source += r'''
class MemoryStream : public Stream {
public:
  std::string input;
  size_t pos = 0;
  explicit MemoryStream(const std::string& frame) : input(frame + "\r\n") {}
  int available() override { return input.size() - pos; }
  int read() override { return pos < input.size() ? static_cast<unsigned char>(input[pos++]) : -1; }
  int peek() override { return pos < input.size() ? static_cast<unsigned char>(input[pos]) : -1; }
};
void receive(const char* frame) {
  MemoryStream stream(frame);
  P1FixedReader<2500> reader(&stream, 0);
  reader.enable(false);
  reader.loop();
  assert(reader.available());
  handleParsedMeter(reader, false);
}
int main() {
  const char* noise = "/garbage\r\nnot-an-OBIS-field\r\n!";
  MyData empty = {};
  auto result = P1Parser::parse(&empty, noise, strlen(noise));
  assert(!result.err); // Reproduce: library skips bad fields without failing the frame.
  assert(!hasP1Measurements(empty));
  receive(noise);
  assert(telegramCount == 1 && telegramErrors == 1 && rejected == 1 && applied == 0);
  receive("/TEST\r\n1-0:1.7.0(not-a-number*kW)\r\n!");
  assert(telegramCount == 2 && telegramErrors == 2 && applied == 0);
  receive("/TEST\r\n0-0:1.0.0(261031093000W)\r\n!");
  assert(telegramErrors == 3 && applied == 0); // Metadata alone is not a meter reading.
  // Saved pre40 is respected initially, but empty frames must not stop fallback.
  clockMs = 11500; CheckP1Signal(); assert(bPre40 && baud == 9600);
  clockMs = 11501; CheckP1Signal(); assert(!bPre40 && baud == 115200);
  receive(p1_generator_fixtures::SMR5_550E);
  assert(applied == 1 && telegramCount == 1 && telegramErrors == 0);
  clockMs += 20000; CheckP1Signal(); assert(baud == 115200);
  // Existing electricity/Warmtelink fixtures remain usable without requiring CRC or a P1 version field.
  for (const auto& fixture : p1_generator_fixtures::all) {
    MyData data = {};
    auto parsed = P1Parser::parse(&data, fixture.telegram, strlen(fixture.telegram));
    if (parsed.err || !hasP1Measurements(data)) {
      fprintf(stderr, "Rejected supported fixture: %s\n", fixture.name);
      return 1;
    }
  }
  MyData zero = {};
  const char* zeroPower = "/TEST\r\n1-0:1.7.0(00.000*kW)\r\n!";
  assert(!P1Parser::parse(&zero, zeroPower, strlen(zeroPower)).err);
  assert(hasP1Measurements(zero)); // A genuine zero is valid; missing fields are not.
  MyData mbusOnly = {};
  const char* mbus = "/TEST\r\n0-1:24.2.1(261031093000W)(00001.000*m3)\r\n!";
  assert(!P1Parser::parse(&mbusOnly, mbus, strlen(mbus)).err);
  assert(mbusOnly.mbus1_delivered_present && hasP1Measurements(mbusOnly));
  const char* partial = "/TEST\r\n1-0:32.7.0(invalid*V)\r\n1-0:1.7.0(00.123*kW)\r\n!";
  MyData data = {};
  assert(!P1Parser::parse(&data, partial, strlen(partial)).err);
  assert(hasP1Measurements(data)); // Retain tolerance of individual broken optional fields.
  // Saved old-meter mode and an actual old meter still lock at 9600.
  bPre40 = true; SetupP1In();
  receive(p1_generator_fixtures::P1_2_2);
  assert(telegramCount > telegramErrors);
  clockMs += 20000; CheckP1Signal(); assert(bPre40 && baud == 9600);
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            cpp = Path(tmp) / 'p1_detection.cpp'
            binary = Path(tmp) / 'p1_detection'
            cpp.write_text(source)
            subprocess.run(['c++', '-std=c++11', '-Wno-vla-cxx-extension',
                            '-I', str(library / 'tests/support'), '-I', str(library / 'src'),
                            '-I', str(library / 'tests'), str(cpp), str(library / 'src/dsmr3/fields3.cpp'),
                            '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    unittest.main()
