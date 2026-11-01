"""Check that a physical network reset preserves provisioned settings and NVS."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class NetworkResetTest(unittest.TestCase):
    def test_button_uses_network_reset(self):
        button = (ROOT / '_Button.h').read_text()
        start = button.index('else if ( btn == BTN_LONG_PRESS )')
        action = button[start:button.index('\n    }', start)]
        self.assertIn('ResetNetwork();', action)
        self.assertNotIn('FacReset', action)

    def test_preservation_and_storage_failures(self):
        include = Path(os.environ.get('ARDUINOJSON_INCLUDE', Path.home() / 'Documents/Arduino/libraries/ArduinoJson/src'))
        if not (include / 'ArduinoJson.h').exists():
            self.skipTest('Set ARDUINOJSON_INCLUDE to the ArduinoJson src directory')
        network = (ROOT / 'Network.ino').read_text()
        start = network.index('static bool clearSkipNetworkForReset()')
        firmware = network[start:network.index('bool loadFixedIPConfig(', start)]
        source = r'''
#include <ArduinoJson.h>
#include <cassert>
#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#define SETTINGS_FILE "/DSMRsettings.json"
bool skipNetwork = false, bFixedIP = true;
bool failRead = false, failWrite = false, failRename = false, failRemove = false;
size_t writeBudget = 100000;
std::map<std::string, std::string> files, nvs;
std::vector<std::string> events;
struct File {
  std::string path;
  size_t pos = 0;
  bool valid = false;
  explicit operator bool() const { return valid; }
  int read() { const auto& data = files[path]; return pos < data.size() ? static_cast<unsigned char>(data[pos++]) : -1; }
  size_t readBytes(char* out, size_t len) {
    const auto& data = files[path]; const size_t n = std::min(len, data.size() - pos);
    memcpy(out, data.data() + pos, n); pos += n; return n;
  }
  size_t write(const uint8_t* bytes, size_t len) {
    const size_t n = std::min(len, writeBudget);
    files[path].append(reinterpret_cast<const char*>(bytes), n); writeBudget -= n; return n;
  }
  size_t write(uint8_t value) { return write(&value, 1); }
  void flush() {}
  void close() {}
};
struct FS {
  bool exists(const char* path) { return files.count(path); }
  File open(const char* path, const char* mode) {
    File f; f.path = path;
    if (!strcmp(mode, "r")) f.valid = !failRead && exists(path);
    else { f.valid = !failWrite; if (f.valid) files[path].clear(); }
    return f;
  }
  bool remove(const char* path) {
    if (failRemove && !strcmp(path, "/fixedip.json")) return false;
    return files.erase(path) != 0;
  }
  bool rename(const char* from, const char* to) {
    if (failRename || !exists(from)) return false;
    files[to] = files[from]; files.erase(from); return true;
  }
} LittleFS;
struct Preferences {
  bool remove(const char* key) { return nvs.erase(key) != 0; }
} preferences;
struct WiFiManager {
  void resetSettings() { nvs.erase("wifi"); events.push_back("wifi-reset"); }
};
namespace WifiIPConfig { bool clear() { nvs.erase("wifi-ip"); return true; } }
void LogFile(const char*, bool) {}
void P1Reboot() { events.push_back("reboot"); }
'''
        source += firmware
        source += r'''
void setup(bool skip) {
  skipNetwork = skip; bFixedIP = true;
  failRead = failWrite = failRename = failRemove = false; writeBudget = 100000;
  events.clear(); files.clear(); nvs.clear();
  files[SETTINGS_FILE] = std::string("{\"skip-network\":") + (skip ? "true" : "false") +
    ",\"MQTTbroker\":\"broker.example\",\"MQTTpasswd\":\"retained\",\"MeentWebId\":\"retained\","
    "\"unknown_preconfig\":{\"key\":\"must survive\"},\"LED\":true}";
  files["/fixedip.json"] = "fixed IP config";
  files["/history.dat"] = "history";
  files["/post.json"] = "pending provisioning";
  nvs = {{"wifi-ip", "static config"}, {"wifi", "credentials"}, {"direct_ap_ssid", "selected AP"},
         {"direct_ap_psk", "cached key"}, {"post", "credentials"},
         {"client_secret", "ownership"}, {"pairing", "peers"}, {"counters", "123"}};
}
int main() {
  for (bool skip : {false, true}) {
    setup(skip);
    const auto oldNvs = nvs;
    const std::string original = files[SETTINGS_FILE];
    JsonDocument expected; assert(!deserializeJson(expected, original));
    ResetNetwork();
    assert(!skipNetwork && !bFixedIP && !LittleFS.exists("/fixedip.json"));
    assert(!LittleFS.exists("/DSMRsettings.reset.tmp"));
    assert(files["/history.dat"] == "history" && files["/post.json"] == "pending provisioning");
    JsonDocument actual; assert(!deserializeJson(actual, files[SETTINGS_FILE]));
    expected["skip-network"] = false;
    assert(actual == expected); // Also checks unknown/nested provisioning fields.
    if (!skip) assert(files[SETTINGS_FILE] == original); // No unnecessary rewrite.
    auto expectedNvs = oldNvs;
    expectedNvs.erase("wifi-ip");
#if !defined(ETHERNET) || defined(ULTRA)
    expectedNvs.erase("wifi");
    assert((events == std::vector<std::string>{"wifi-reset", "reboot"}));
#else
    assert((events == std::vector<std::string>{"reboot"}));
#endif
#if DIRECT_AP_CONNECT
    expectedNvs.erase("direct_ap_ssid"); expectedNvs.erase("direct_ap_psk");
#endif
    assert(nvs == expectedNvs);
  }
  // DHCP already in use: there is no fixed-IP file to remove.
  setup(false); files.erase("/fixedip.json"); ResetNetwork();
  assert(!events.empty() && events.back() == "reboot");
  // If clearing skip-network cannot be saved, leave the original config intact.
  for (int failure = 0; failure < 7; ++failure) {
    setup(true);
    if (failure == 0) failRead = true;
    if (failure == 1) failWrite = true;
    if (failure == 2) failRename = true;
    if (failure == 3) writeBudget = 4;
    if (failure == 4) files[SETTINGS_FILE] = "invalid JSON";
    if (failure == 5) files[SETTINGS_FILE] = "[]";
    if (failure == 6) files.erase(SETTINGS_FILE);
    const auto originalFiles = files;
    const auto originalNvs = nvs;
    ResetNetwork();
    assert(skipNetwork && bFixedIP && events.empty());
    assert(files == originalFiles && nvs == originalNvs);
  }
  setup(false); failRemove = true;
  const auto originalFiles = files;
  const auto originalNvs = nvs;
  ResetNetwork();
  assert(events.empty() && files == originalFiles && nvs == originalNvs);
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            cpp = Path(tmp) / 'network_reset.cpp'
            binary = Path(tmp) / 'network_reset'
            cpp.write_text(source)
            variants = [[], ['-DETHERNET'], ['-DETHERNET', '-DULTRA'],
                        ['-DDIRECT_AP_CONNECT=1'], ['-DETHERNET', '-DDIRECT_AP_CONNECT=1']]
            for flags in variants:
                with self.subTest(flags=flags):
                    subprocess.run(['c++', '-std=c++11', '-I', str(include), *flags, str(cpp), '-o', str(binary)], check=True)
                    subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    unittest.main()
