"""Test the two reset thresholds and complete removal of saved provisioning."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2] + '\n'


class FactoryResetTest(unittest.TestCase):
    def test_release_thresholds_blink_and_provisioning_cleanup(self):
        button = (ROOT / '_Button.h').read_text()
        helper = (ROOT / 'Helper.ino').read_text()
        header = (ROOT / 'DSMRloggerAPI.h').read_text()
        posts = (ROOT / 'POSTS.ino').read_text()
        source = r'''
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#define F(x) x
#define DebugTln(...) ((void)0)
#define DebugT(...) ((void)0)
#define Debugln(...) ((void)0)
#define IRAM_ATTR
#define portENTER_CRITICAL(...) ((void)0)
#define portEXIT_CRITICAL(...) ((void)0)
#define portENTER_CRITICAL_ISR(...) ((void)0)
#define portEXIT_CRITICAL_ISR(...) ((void)0)
#define LED_RED 0x70000
#define LED_GREEN 0x700
#define LED_BLACK 0
#define SETTINGS_FILE "/DSMRsettings.json"
using byte = uint8_t;
uint32_t clockMs = 1000000, Tpressed = 0;
bool bButtonPressed = false, allowSkipNetworkByButton = false, skipNetwork = false;
bool bPairingmode = false, bMeentEnabled = false;
int netw_state = 1, button_io = 0, networkResets = 0, reboots = 0, wifiResets = 0;
int queued = 0, queuedType = 0, queuedPriority = 0;
bool queueAvailable = true;
const int NW_NONE = 0;
struct { int peers = 0; } Pref;
uint32_t millis() { return clockMs; }
int inputLevel = 1;
int digitalRead(int) { return inputLevel; }
void SetNRGMPairingMode(bool value) { bPairingmode = value; }
void LogFile(const char*, bool) {}
void ResetNetwork() { ++networkResets; }
void P1Reboot() { ++reboots; }
void SwitchLED(byte, uint32_t) {}
void OverrideLED(bool, uint32_t) {}
void HandleLED() {}
bool WorkerEnqueueSimple(int type, int priority) {
  if (!queueAvailable) return false;
  ++queued; queuedType = type; queuedPriority = priority; return true;
}
std::map<std::string, std::string> files;
std::map<std::string, std::map<std::string, std::string>> nvs;
std::string failNamespace, failRemove;
struct {
  bool exists(const char* path) { return files.count(path); }
  bool remove(const char* path) {
    if (failRemove == path) return false;
    return files.erase(path) != 0;
  }
} LittleFS;
struct Preferences {
  std::string name = "P1Status";
  bool begin(const char* ns, bool) { name = ns; return name != failNamespace; }
  bool clear() { nvs[name].clear(); return true; }
  void end() {}
  bool isKey(const char* key) { return nvs[name].count(key); }
  bool remove(const char* key) { return nvs[name].erase(key) != 0; }
} preferences;
namespace WifiIPConfig {
  bool clear() { nvs["wifi-ip"].clear(); return true; }
}
struct WiFiManager { void resetSettings() { ++wifiResets; nvs["wifi"].clear(); } };
static constexpr const char* POST_NVS = "http_post";
static constexpr const char* KEMP_NVS_NAMESPACE = "kemp_post";
static constexpr const char* MEENT_CLIENT_SECRET_NVS_KEY = "meent_secret";
char meentClientSecret[65] = "secret";
'''
        source += header[header.index('enum HttpPostProvider'):header.index('uint8_t   settingFuse')]
        worker = (ROOT / 'Worker.h').read_text()
        source += worker[worker.index('enum WorkerPriority'):worker.index('struct WorkerLogPayload')]
        source += button[:button.index('//Aux processor task')] + '\n#endif\n'
        init_start = button.index('  lastState = (btn_states)digitalRead(button_io);', button.index('void fAuxProc'))
        source += 'void initButtonForTest() {\n' + button[init_start:button.index('  attachInterrupt', init_start)] + '}\n'
        source += function(posts, 'bool ClearPostProvisioning(')
        source += function(helper, 'void FacReset(')
        source += function(helper, 'void FactoryResetFromWorker(')
        # Exercise the actual write-entry guards without pulling in unrelated firmware writers.
        for filename, signature, testname in [('OtherFiles.ino', 'void writeSettingsDirect()', 'trySettingsWrite'),
                                               ('Prefs.ino', 'void P1StatusWriteDirect()', 'tryStatusWrite')]:
            text = (ROOT / filename).read_text()
            body = text[text.index('{', text.index(signature)) + 1:]
            guard = body.strip().splitlines()[0]
            self.assertIn('factoryResetPending', guard)
            source += f'void {testname}(int& writes) {{ {guard}\n ++writes; }}\n'
        source += r'''
void release(uint32_t duration) {
  Tpressed = clockMs;
  Treleased = clockMs + duration;
  clockMs = Treleased;
  bButtonPressed = false;
  bButtonReleased = true;
  handleButton();
}
void seed() {
  files = {{SETTINGS_FILE,"customer"}, {"/post.json","pending"},
           {"/fixedip.json","fixed"}, {"/DSMRsettings.reset.tmp","temporary"},
           {"/DSMRindexEDGE.html","frontend"}, {"/RNGdays.json","history"}};
  nvs = {{"http_post",{{"settings","provider/key"}}}, {"kemp_post",{{"api_key","legacy"}}},
         {"wifi-ip",{{"config","static"}}}, {"wifi",{{"ssid","customer"}}},
         {"P1Status",{{"meent_secret","owner"},{"direct_ap_ssid","AP"},{"direct_ap_psk","key"},
                      {"peers","1"},{"mac","paired"},{"eid_state","2"},{"dev_type","12"},{"wtr_l","456"}}}};
  bHttpPostEnabled = bMeentEnabled = true;
  settingHttpPostProvider = HTTP_POST_POWERC;
  reboots = wifiResets = 0;
}
int main() {
  // A button already held at task startup must not use total uptime as its duration.
  inputLevel = 0; clockMs = 30000; initButtonForTest();
  assert(bButtonPressed && Tpressed == 30000);
  Treleased = clockMs = 32000; bButtonPressed = false; bButtonReleased = true;
  handleButton(); assert(queued == 0 && networkResets == 0);
  clockMs = 1000000; s_buttonIgnoreUntilMs = 0; bPairingmode = false;
  // Feedback changes before any destructive action; actions happen only on release.
  assert(buttonFeedbackColor(4999,0) == LED_GREEN);
  assert(buttonFeedbackColor(5000,0) == LED_GREEN);
  assert(buttonFeedbackColor(5001,0) == LED_RED);
  assert(buttonFeedbackColor(15000,0) == LED_RED);
  assert(buttonFeedbackColor(15001,0) == LED_RED);
  assert(buttonFeedbackColor(15251,0) == LED_BLACK);
  assert(buttonFeedbackColor(15501,0) == LED_RED);
  assert(networkResets == 0 && queued == 0);
  release(5000); assert(networkResets == 0 && queued == 0 && bPairingmode);
  release(5001); assert(networkResets == 1 && queued == 0);
  release(15000); assert(networkResets == 2 && queued == 0);
  release(15001);
  assert(networkResets == 2 && queued == 1 && factoryResetPending);
  assert(queuedType == WORKER_JOB_FACTORY_RESET && queuedPriority == WORKER_PRIO_NORMAL);
  FacReset(); assert(queued == 1); // Repeated handling cannot queue another erase.
  int writes = 0; trySettingsWrite(writes); tryStatusWrite(writes);
  assert(writes == 0); // Pending writes cannot restore erased configuration.
  seed(); FactoryResetFromWorker();
  assert(reboots == 1 && factoryResetPending);
  assert(!files.count(SETTINGS_FILE) && !files.count("/post.json") && !files.count("/fixedip.json"));
  assert(files.size() == 2 && files["/DSMRindexEDGE.html"] == "frontend" && files["/RNGdays.json"] == "history");
  assert(nvs["http_post"].empty() && nvs["kemp_post"].empty() && nvs["wifi-ip"].empty());
  assert(nvs["P1Status"].size() == 2);
  assert(nvs["P1Status"]["dev_type"] == "12" && nvs["P1Status"]["wtr_l"] == "456");
  assert(!bHttpPostEnabled && !bMeentEnabled && settingHttpPostProvider == HTTP_POST_GENERIC);
  assert(!isPowerCMode() && localHttpEnabled()); // Production web UI is available on the default profile.
#if !defined(ETHERNET) || defined(ULTRA)
  assert(wifiResets == 1 && nvs["wifi"].empty());
#else
  assert(wifiResets == 0);
#endif
  // Missing keys/files are also a successful reset.
  files.clear(); nvs.clear(); FactoryResetFromWorker(); assert(reboots == 2);
  factoryResetPending = false; queueAvailable = false;
  FacReset(); assert(!factoryResetPending && queued == 1); // No fallback to a partial synchronous erase.
  queueAvailable = true; FacReset(); assert(factoryResetPending && queued == 2);
  seed(); failNamespace = "http_post";
  FactoryResetFromWorker(); assert(!factoryResetPending && reboots == 0);
  failNamespace.clear(); factoryResetPending = true; seed(); failRemove = SETTINGS_FILE;
  FactoryResetFromWorker(); assert(!factoryResetPending && reboots == 0 && files.count(SETTINGS_FILE));
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            cpp = Path(tmp) / 'factory_reset.cpp'
            binary = Path(tmp) / 'factory_reset'
            cpp.write_text(source)
            for flags in [[], ['-DDEBUG'], ['-DETHERNET'], ['-DETHERNET', '-DULTRA']]:
                with self.subTest(flags=flags):
                    subprocess.run(['c++', '-std=c++11', *flags, str(cpp), '-o', str(binary)], check=True)
                    subprocess.run([str(binary)], check=True)

    def test_factory_job_bypasses_pending_writes(self):
        worker = (ROOT / 'Worker.ino').read_text()
        body = function(worker, 'static void workerHandleJob(')
        self.assertIn('factoryResetPending && job.type != WORKER_JOB_FACTORY_RESET', body)
        self.assertIn('FactoryResetFromWorker();', body)


if __name__ == '__main__':
    unittest.main()
