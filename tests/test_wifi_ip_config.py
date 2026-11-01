"""Exercise the firmware NVS record with host substitutes for ESP32 APIs."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class WifiIPConfigTest(unittest.TestCase):
    def test_storage_and_portal_transitions(self):
        network = (ROOT / 'Network.ino').read_text()
        callbacks = network[network.index('static WifiIPConfig::Record pendingWifiIP'):network.index('static bool wifiProvisioningConnectionFailed()')]
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            (folder / 'IPAddress.h').write_text(r'''
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
struct String : std::string {
  using std::string::string;
  size_t length() const { return size(); }
};
struct IPAddress {
  uint32_t value;
  IPAddress(uint32_t v = 0) : value(v) {}
  operator uint32_t() const { return value; }
  uint8_t operator[](int i) const { return (value >> (8*i)) & 255; }
  bool fromString(const String& text) {
    unsigned a,b,c,d; char extra;
    if (sscanf(text.c_str(), "%u.%u.%u.%u%c", &a,&b,&c,&d,&extra) != 4 ||
        a>255 || b>255 || c>255 || d>255) return false;
    value = a | (b<<8) | (c<<16) | (d<<24); return true;
  }
};
''')
            (folder / 'Preferences.h').write_text(r'''
#pragma once
#include <cassert>
#include <cstring>
#include <vector>
static std::vector<unsigned char> blob;
static bool failOpen = false, failWrite = false;
static int writes = 0;
struct Preferences {
  bool begin(const char* ns, bool) { assert(!strcmp(ns, "wifi-ip")); return !failOpen; }
  void end() {}
  size_t getBytesLength(const char*) { return blob.size(); }
  size_t getBytes(const char*, void* dest, size_t n) {
    if (blob.size()!=n) return 0; memcpy(dest, blob.data(), n); return n;
  }
  size_t putBytes(const char*, const void* src, size_t n) {
    if (failWrite) return 0;
    const auto* bytes = static_cast<const unsigned char*>(src);
    blob.assign(bytes, bytes+n); ++writes; return n;
  }
  bool clear() { blob.clear(); return true; }
};
''')
            source = r'''
#include "WifiIPConfig.h"
#include <map>
bool bFixedIP = false, wifiProvisioningRetryPrepared = true;
uint32_t wifiProvisioningConnectStartedAt = 0;
uint32_t millis() { return 42; }
void LogFile(const char*, bool) {}
constexpr int WL_CONNECTED = 3;
struct {
  int state = WL_CONNECTED, dhcpCalls = 0;
  int status() { return state; }
  bool config(IPAddress ip, IPAddress, IPAddress) { assert(!uint32_t(ip)); ++dhcpCalls; return true; }
} WiFi;
struct Server {
  std::map<std::string, String> args;
  String arg(const char* name) { return args[name]; }
} server;
struct {
  Server* server = &::server;
  uint32_t ip=0, gw=0, sn=0, dns=0;
  void setSTAStaticIPConfig(IPAddress i, IPAddress g, IPAddress s, IPAddress d) {
    ip=i; gw=g; sn=s; dns=d;
  }
} manageWiFi;
'''
            source += callbacks
            source += r'''
int main() {
  WifiIPConfig::Record loaded;
  assert(!WifiIPConfig::load(loaded)); // Existing DHCP devices need no migration.
  server.args = {{"ip", "192.168.1.50"}, {"gw", "192.168.1.1"},
                 {"sn", "255.255.255.0"}, {"dns", "1.1.1.1"}};
  wifiProvisioningConnectStarted();
  assert(pendingWifiIPValid && manageWiFi.ip && blob.empty());
  WiFi.state = 0; saveWifiIPConfig();
  assert(blob.empty()); // Failed connection must not replace stored settings.
  WiFi.state = WL_CONNECTED; saveWifiIPConfig();
  assert(WifiIPConfig::load(loaded) && loaded.ip == manageWiFi.ip);
  const auto original = blob;
  manageWiFi.ip = manageWiFi.gw = manageWiFi.sn = manageWiFi.dns = 0;
  bFixedIP = true; restoreWifiIPConfig();
  assert(!manageWiFi.ip); // Legacy configuration keeps precedence.
  bFixedIP = false;
  restoreWifiIPConfig(); // Simulate runtime state lost in a reboot.
  assert(manageWiFi.ip == loaded.ip && manageWiFi.dns == loaded.dns);
  const int before = writes;
  assert(WifiIPConfig::save(loaded) && writes == before); // Avoid redundant writes.
  server.args["ip"] = "192.168.1.51";
  wifiProvisioningConnectStarted();
  WiFi.state = 0; saveWifiIPConfig();
  assert(blob == original); // Failed replacement retains the working record.
  WiFi.state = WL_CONNECTED;
  failWrite = true;
  assert(!WifiIPConfig::save(WifiIPConfig::dhcp()) && blob == original);
  failWrite = false;
  server.args["dns"] = "";
  wifiProvisioningConnectStarted();
  assert(pendingWifiIPValid && !manageWiFi.dns); // DNS is optional.
  server.args["sn"] = "255.0.255.0";
  wifiProvisioningConnectStarted(); saveWifiIPConfig();
  assert(blob == original && !pendingWifiIPValid);
  server.args.clear();
  wifiProvisioningConnectStarted();
  assert(pendingWifiIPValid && !manageWiFi.ip && !manageWiFi.dns && WiFi.dhcpCalls == 1);
  saveWifiIPConfig();
  assert(WifiIPConfig::load(loaded) && !loaded.ip && !loaded.gateway && !loaded.dns);
  blob = original; blob.pop_back();
  assert(!WifiIPConfig::load(loaded)); // Truncated/unknown records are ignored.
  blob = original; blob[0] = 99;
  assert(!WifiIPConfig::load(loaded));
  failOpen = true;
  assert(!WifiIPConfig::load(loaded) && !WifiIPConfig::clear());
  failOpen = false;
  assert(WifiIPConfig::clear() && blob.empty());
}
'''
            (folder / 'test.cpp').write_text(source)
            subprocess.run(['c++', '-std=c++11', '-I', tmp, '-I', str(ROOT),
                            str(folder / 'test.cpp'), '-o', str(folder / 'test')], check=True)
            subprocess.run([str(folder / 'test')], check=True)


if __name__ == '__main__':
    unittest.main()
