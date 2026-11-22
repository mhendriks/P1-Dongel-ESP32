"""Exercise the production Envoy night pause, validation and automatic ring writer."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2] + '\n'


class EnphaseDailyTest(unittest.TestCase):
    def test_invalid_readings_resets_and_persisted_history(self):
        library = Path(os.environ.get('ARDUINOJSON_LIB_DIR',
                       ROOT.parent / 'libraries' / 'ArduinoJson'))
        rng = (ROOT / 'RNG.ino').read_text()
        solar = (ROOT / 'Solar.ino').read_text()
        source = r'''
#include <ArduinoJson.h>
#include <cstdint>
#include <cmath>
#include <ctime>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <algorithm>
using String = std::string;
constexpr time_t SECS_PER_DAY = 86400;
time_t clockTime = 10 * SECS_PER_DAY + 12 * 3600;
time_t now() { return clockTime; }
int year(time_t) { return 2026; }
int hour(time_t t) { return t % SECS_PER_DAY / 3600; }
struct GatewayUrl {
  bool nested = false;
  int indexOf(const char*) const { return -1; }
  bool endsWith(const char*) const { return nested; }
};
struct SolarPwrSystems { GatewayUrl Url; uint32_t Actual=0, Daily=0; } Enphase;
enum SolarSource { ENPHASE };
uint32_t enphaseDailyDay = 0;
void noteSolarFetchDuration(uint32_t) {}
bool changeDayDuringRequest = false;
struct Http {
  String payload;
  String getString() {
    if (changeDayDuringRequest) clockTime += SECS_PER_DAY;
    return payload;
  }
  void end() {}
};
enum E_ringfiletype { RINGHOURS, RINGDAYS, RINGMONTHS };
constexpr int RNG_DAYS_VALUE_COUNT = 7, DATA_RECLEN_7 = 109;
constexpr int JSON_HEADER_LEN = 23, SeekSet = 0;
struct { const char* filename; int slots, recordLen, valueCount, f_len; } RingFiles[] = {
  {"hours", 2, 98, 6, 220}, {"days", 2, 109, 7, 242}, {"months", 2, 98, 6, 220}
};
bool EnableHistory = true, FSmounted = true, failRead = false;
std::string disk;
int16_t rngHeaderActSlot[3] = {-1, -1, -1};
size_t strlcpy(char* dst, const char* src, size_t n) {
  snprintf(dst, n, "%s", src); return strlen(src);
}
#define F(x) x
#define DebugT(x) ((void)0)
#define DebugTln(x) ((void)0)
#define Debugln(x) ((void)0)
#define DebugTf(...) ((void)0)
struct File {
  size_t pos = 0;
  operator bool() const { return true; }
  size_t size() const { return disk.size(); }
  bool seek(size_t n, int) { pos = n; return n <= disk.size(); }
  size_t readBytes(char* dest, size_t n) {
    if (failRead) return 0;
    n = std::min(n, disk.size() - pos); memcpy(dest, disk.data()+pos, n); pos += n; return n;
  }
  size_t write(const uint8_t* src, size_t n) {
    disk.replace(pos, n, reinterpret_cast<const char*>(src), n); pos += n; return n;
  }
  void print(const char* s) { write(reinterpret_cast<const uint8_t*>(s), strlen(s)); }
  void close() {}
};
struct { File open(const char*, const char*) { return File(); } } LittleFS;
uint8_t CalcSlotAt(E_ringfiletype, time_t, bool) { return 0; }
'''
        for signature in (
            'static uint8_t ringValueCount(', 'static uint16_t ringRecordLen(',
            'static bool rngShouldWriteHeader(', 'static void rngRememberHeader(',
            'static void rngInvalidateHeader(', 'static int formatRingRecord(',
            'static size_t finishRingRecord(', 'static bool writeRingValuesAtSlot('):
            source += function(rng, signature)
        source += function(solar, 'static uint32_t enphaseDailyAt(')
        source += 'void fetch(int httpResponseCode, const char* body) {\n'
        source += 'Http http{body}; SolarPwrSystems* solarSystem=&Enphase;\n'
        source += 'SolarSource src=ENPHASE; bool bSolis=false; uint32_t fetchStartMs=0;\n'
        start = solar.index('  const time_t fetchTime = now();', solar.index('void GetSolarData('))
        source += solar[start:solar.index('  CrashLogMark(', start)]
        start = solar.index('  if (httpResponseCode != 200)', solar.index('void GetSolarData('))
        source += solar[start:solar.index('\n  if (bSolis) {', start)]
        source += '}\n'
        source += r'''
void seed(const char* date, float pv) {
  float values[7] = {10, 20, 30, 40, 50, 60, pv};
  char buf[DATA_RECLEN_7];
  int len = formatRingRecord(RINGDAYS, buf, sizeof(buf), date, values);
  assert(len == DATA_RECLEN_7-2);
  disk = std::string(JSON_HEADER_LEN, ' ');
  disk.append(buf, len); disk += ",\n";
  disk.resize(RingFiles[RINGDAYS].f_len, ' ');
  rngHeaderActSlot[RINGDAYS] = -1;
}
JsonDocument stored() {
  JsonDocument doc;
  assert(!deserializeJson(doc, disk.substr(JSON_HEADER_LEN, DATA_RECLEN_7-2)));
  return doc;
}
int main() {
  const char* valid = R"({"wattsNow":50,"wattHoursToday":1800})";
  fetch(200, valid);
  assert(Enphase.Actual==50 && Enphase.Daily==1800);
  assert(enphaseDailyAt(clockTime)==1800);
  for (int status : {401, 403, 500, -1}) {
    fetch(status, valid);
    assert(Enphase.Actual==50 && Enphase.Daily==1800);
  }
  for (const char* invalid : {
      "{broken", R"({"error":"unauthorized"})", R"({"wattsNow":0})",
      R"({"wattsNow":0,"wattHoursToday":null})",
      R"({"wattsNow":0,"wattHoursToday":"0"})",
      R"({"wattsNow":0,"wattHoursToday":false})",
      R"({"wattsNow":0,"wattHoursToday":-1})",
      R"({"wattsNow":0,"wattHoursToday":4294967296})",
      R"({"wattsNow":{},"wattHoursToday":100})"}) {
    fetch(200, invalid);
    assert(Enphase.Actual==50 && Enphase.Daily==1800);
  }
  Enphase.Url.nested = true;
  fetch(200, R"({"production":{"pcu":{"wattsNow":-2,"wattHoursToday":1900}}})");
  assert(Enphase.Actual==0 && Enphase.Daily==1900);
  changeDayDuringRequest = true;
  fetch(200, R"({"production":{"pcu":{"wattsNow":0,"wattHoursToday":0}}})");
  assert(Enphase.Daily==1900); // Clock/date changed while HTTP was in flight.
  changeDayDuringRequest = false;
  clockTime = 10 * SECS_PER_DAY + 23 * 3600;
  fetch(200, R"({"production":{"pcu":{"wattsNow":0,"wattHoursToday":0}}})");
  assert(Enphase.Daily==1900); // No reading after 23:00, even forced fetches.
  clockTime = 11 * SECS_PER_DAY;
  fetch(200, "{}");
  assert(Enphase.Daily==1900 && enphaseDailyAt(clockTime)==0);
  assert(enphaseDailyAt(clockTime-1)==1900); // Midnight snapshot keeps yesterday.
  clockTime += 5 * 3600;
  fetch(200, R"({"production":{"pcu":{"wattsNow":0,"wattHoursToday":0}}})");
  assert(Enphase.Daily==0 && enphaseDailyAt(clockTime)==0); // Resume at 05:00.

  // Use the production fixed-width writer, with a pre-existing record on disk.
  seed("26102523", 1.6f);
  float incoming[7] = {11, 21, 31, 41, 51, 61, 0};
  assert(writeRingValuesAtSlot(RINGDAYS, 0, 0, "26102523", incoming));
  assert(fabs(stored()["values"][6].as<float>()-1.6f)<.0001f);
  assert(stored()["values"][0].as<float>()==11); // Other meter counters still advance.
  Enphase.Daily = 0; enphaseDailyDay = 0; // Simulated reboot; history remains on disk.
  rngHeaderActSlot[RINGDAYS] = -1;
  incoming[6] = .651f;
  assert(writeRingValuesAtSlot(RINGDAYS, 0, 0, "26102500", incoming));
  assert(fabs(stored()["values"][6].as<float>()-1.6f)<.0001f); // Compare dates, not record hour.
  incoming[6] = 1.8f;
  assert(writeRingValuesAtSlot(RINGDAYS, 0, 0, "26102523", incoming));
  assert(fabs(stored()["values"][6].as<float>()-1.8f)<.0001f);
  incoming[6] = 0;
  assert(writeRingValuesAtSlot(RINGDAYS, 0, 0, "26112600", incoming));
  assert(stored()["values"][6].as<float>()==0); // Ring reuse must not inherit another day.
  const std::string saved = disk;
  failRead = true;
  assert(!writeRingValuesAtSlot(RINGDAYS, 0, 0, "26112601", incoming));
  assert(disk==saved);
  failRead = false;
  EnableHistory = false;
  assert(!writeRingValuesAtSlot(RINGDAYS, 0, 0, "26112601", incoming));
  assert(disk==saved);
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            cpp = Path(tmp) / 'enphase.cpp'
            binary = Path(tmp) / 'enphase'
            cpp.write_text(source)
            subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra',
                            '-I'+str(ROOT), '-I'+str(library / 'src'),
                            str(cpp), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    unittest.main()
