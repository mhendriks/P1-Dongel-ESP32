"""Exercise the production OTA scheduler with a deferred FreeRTOS task."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2] + '\n'


class OtaTaskTest(unittest.TestCase):
    def test_download_is_deferred_and_failure_allows_retry(self):
        ota = (ROOT / 'Update.ino').read_text()
        source = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
uint32_t clockMs = 0, remoteUpdateQueuedAt = 0;
uint32_t millis() { return clockMs; }
bool factoryResetPending = false, bUpdateSketch = true;
volatile bool UpdateRequested = false, remoteUpdateTaskRunning = false;
char UpdateVersion[32] = "", remoteUpdateState[32] = "";
int remoteUpdateProgress = 0, downloads = 0, creates = 0, deletes = 0;
bool createOk = true;
constexpr int pdPASS = 1;
void (*pendingTask)(void*) = nullptr;
void LogFile(const char*, bool) {}
void MQTTSetHAUpdateState(bool, int = 0) {}
void setRemoteUpdateStatus(const char* state, const char* = "") {
  snprintf(remoteUpdateState, sizeof(remoteUpdateState), "%s", state);
}
#define strlcpy mockStrlcpy
void mockStrlcpy(char* dst, const char* src, size_t size) {
  snprintf(dst, size, "%s", src);
}
void RemoteUpdate(const char* version, bool sketch) {
  assert(!strcmp(version, "5.10.7") && sketch);
  ++downloads;
  // Model the failed download returning without a reboot.
  UpdateRequested = false;
}
void vTaskDelete(void*) { ++deletes; }
int xTaskCreate(void (*task)(void*), const char*, int, void*, int, void*) {
  ++creates;
  if (!createOk) return 0;
  pendingTask = task;
  return pdPASS;
}
'''
        source += function(ota, 'static void remoteUpdateTask(')
        source += function(ota, 'void handleRemoteUpdate(')
        source += function(ota, 'bool QueueRemoteUpdate(')
        source += r'''
int main() {
  assert(!QueueRemoteUpdate(nullptr, true));
  assert(!QueueRemoteUpdate("", true));
  assert(QueueRemoteUpdate("5.10.7", true));
  assert(!QueueRemoteUpdate("5.10.6", true));
  clockMs = 1499;
  handleRemoteUpdate();
  assert(creates == 0);
  clockMs = 1500;
  handleRemoteUpdate();
  assert(creates == 1 && downloads == 0 && UpdateRequested);
  // A stalled download does not execute in the loop or create more tasks.
  for (int i = 0; i < 100; ++i) handleRemoteUpdate();
  assert(creates == 1 && downloads == 0);
  pendingTask(nullptr);
  assert(downloads == 1 && deletes == 1 && !UpdateRequested);
  assert(!remoteUpdateTaskRunning);
  assert(QueueRemoteUpdate("5.10.7", true));
  clockMs += 1500;
  createOk = false;
  handleRemoteUpdate();
  assert(!UpdateRequested && !remoteUpdateTaskRunning);
  assert(!strcmp(remoteUpdateState, "error"));
  assert(QueueRemoteUpdate("5.10.7", true));
  clockMs += 1500;
  createOk = true;
  factoryResetPending = true;
  handleRemoteUpdate();
  assert(creates == 2);
  factoryResetPending = false;
  handleRemoteUpdate();
  pendingTask(nullptr);
  assert(downloads == 2 && !UpdateRequested && !remoteUpdateTaskRunning);
}
'''
        with tempfile.TemporaryDirectory() as temp:
            cpp = Path(temp) / 'ota.cpp'
            binary = Path(temp) / 'ota'
            cpp.write_text(source)
            subprocess.run(['c++', '-std=c++11', str(cpp), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    unittest.main()
