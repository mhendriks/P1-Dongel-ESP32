"""Run the production HTTP PV config handlers against an in-memory filesystem."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2] + '\n'


class SolarConnectorApiTest(unittest.TestCase):
    def test_legacy_enable_disable_validation_and_failed_writes(self):
        solar = (ROOT / 'Solar.ino').read_text()
        code = r'''
#include <ArduinoJson.h>
#include <string>
#include <map>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <ctime>
class String : public std::string {
public:
  using std::string::string;
  String(const std::string& s) : std::string(s) {}
  bool startsWith(const char* prefix) const { return rfind(prefix, 0) == 0; }
  size_t write(uint8_t c) { push_back(c); return 1; }
  size_t write(const uint8_t* s, size_t n) { append((const char*)s, n); return n; }
};
namespace ArduinoJson {
template <> struct Converter<String> {
  static String fromJson(JsonVariantConst v) { return v.as<std::string>(); }
  static void toJson(const String& s, JsonVariant v) { v.set(static_cast<const std::string&>(s)); }
};
}
using std::max;
enum SolarSource { ENPHASE, SMA, OMNIKSOL };
enum HTTPMethod { HTTP_GET, HTTP_POST, HTTP_DELETE };
struct ApiRequestContext { HTTPMethod method; String pathArg; std::string body; };
struct ApiResponse { int status; String contentType; String body; };
std::map<std::string, std::string> disk;
bool FSmounted = true, skipNetwork = false, failWrite = false, failRename = false;
String _sma_sid;
int queued = 0;
void WorkerEnqueueSolarFetch() { ++queued; }
void apiWsMarkLiveDirty() {}
#define F(x) x
#define DebugVerboseT(x) ((void)0)
#define DebugVerboseLn(x) ((void)0)
#define DebugTln(x) ((void)0)
#define Debugln(x) ((void)0)
struct File {
  std::string path;
  size_t pos = 0;
  operator bool() const { return !path.empty(); }
  int read() { return pos < disk[path].size() ? (uint8_t)disk[path][pos++] : -1; }
  size_t readBytes(char* p, size_t n) {
    n = std::min(n, disk[path].size() - pos); memcpy(p, disk[path].data()+pos, n); pos += n; return n;
  }
  size_t write(uint8_t c) { return write(&c, 1); }
  size_t write(const uint8_t* p, size_t n) {
    if (failWrite) return 0;
    disk[path].append((const char*)p, n); return n;
  }
  void close() {}
};
struct {
  bool exists(const char* p) { return disk.count(p); }
  File open(const std::string& p, const char* mode) {
    if (*mode == 'w') disk[p].clear();
    return {p};
  }
  bool rename(const std::string& a, const std::string& b) {
    if (failRename) return false;
    disk[b] = disk[a]; disk.erase(a); return true;
  }
  void remove(const std::string& p) { disk.erase(p); }
} LittleFS;
'''
        code += solar[:solar.index('// Local dongle day')]
        for signature in ('static void* solarSystemForSource(',
                          'static uint32_t defaultSolarRefreshInterval(',
                          'void ReadSolarConfig(', 'ApiResponse handleSolarConnectorApi('):
            code += function(solar, signature)
        code += 'uint32_t modbusPvWp() { return 1000; }\n'
        code += function(solar, 'uint32_t totalSolarInstalledWp(')
        code += r'''
int main() {
  disk["/sma.json"] = R"({"gateway-url":"http://sma","token":"old-password","wp":3500,"refresh-interval":30})";
  ReadSolarConfig(SMA);
  assert(SMAinv.Available && SMAinv.Wp == 3500 && SMAinv.Interval == 30);
  auto get = handleSolarConnectorApi({HTTP_GET, "sma", ""});
  assert(get.status == 200);
  JsonDocument current; assert(!deserializeJson(current, static_cast<std::string>(get.body)));
  assert(current["enabled"] == true && current["token"] == "old-password");
  SMAinv.Actual = 900; SMAinv.Daily = 4500;
  const std::string disabled = R"({"enabled":false,"gateway-url":"http://sma","token":"old-password","wp":3500,"refresh-interval":15})";
  assert(handleSolarConnectorApi({HTTP_POST,"sma",disabled}).status == 200);
  assert(!SMAinv.Available && SMAinv.Actual == 0 && SMAinv.Daily == 0);
  assert(SMAinv.Wp == 3500 && SMAinv.Token == "old-password" && queued == 1);
  assert(totalSolarInstalledWp() == 1000);
  assert(handleSolarConnectorApi({HTTP_POST,"sma","{}"}).status == 400);
  auto original = disk["/sma.json"];
  const std::string enabled = R"({"enabled":true,"gateway-url":"http://sma","token":"new-password","wp":4000,"refresh-interval":15})";
  failWrite = true;
  assert(handleSolarConnectorApi({HTTP_POST,"sma",enabled}).status == 500);
  assert(disk["/sma.json"] == original && !SMAinv.Available);
  failWrite = false; failRename = true;
  assert(handleSolarConnectorApi({HTTP_POST,"sma",enabled}).status == 500);
  assert(disk["/sma.json"] == original && !disk.count("/sma.json.tmp"));
  failRename = false;
  assert(handleSolarConnectorApi({HTTP_POST,"sma",enabled}).status == 200);
  assert(SMAinv.Available && SMAinv.Token == "new-password" && SMAinv.Interval == 15);
  assert(totalSolarInstalledWp() == 5000);
  assert(handleSolarConnectorApi({HTTP_POST,"enphase",enabled}).status == 400);
  assert(handleSolarConnectorApi({HTTP_GET,"solaredge",""}).status == 404);
  assert(handleSolarConnectorApi({HTTP_DELETE,"sma",""}).status == 405);
  FSmounted = false;
  assert(handleSolarConnectorApi({HTTP_POST,"sma",enabled}).status == 503);
  assert(!disk.count("/enphase.json"));
  assert(!disk.count("/omniksol.json"));
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / 'test.cpp').write_text(code)
            subprocess.run(['c++', '-std=c++17', '-I', str(ROOT.parent / 'libraries/ArduinoJson/src'),
                            str(path / 'test.cpp'), '-o', str(path / 'test')], check=True)
            subprocess.run([str(path / 'test')], check=True)


if __name__ == '__main__':
    unittest.main()
