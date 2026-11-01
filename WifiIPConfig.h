#pragma once

#include <Preferences.h>
#include <IPAddress.h>

// Separate from P1Status and from Ethernet's /fixedip.json. One NVS blob
// prevents a power interruption from mixing fields from two configurations.
namespace WifiIPConfig {
struct Record {
  uint32_t version;
  uint32_t ip;
  uint32_t gateway;
  uint32_t subnet;
  uint32_t dns;
};

inline Record dhcp() { return {1, 0, 0, 0, 0}; }

inline bool valid(const Record& config) {
  if (config.version != 1) return false;
  if (!config.ip) return !config.gateway && !config.subnet && !config.dns;
  if (!config.gateway || !config.subnet) return false;
  const IPAddress mask(config.subnet);
  const uint32_t bits = (uint32_t(mask[0]) << 24) | (uint32_t(mask[1]) << 16) |
                        (uint32_t(mask[2]) << 8) | mask[3];
  const uint32_t inverse = ~bits;
  return bits && inverse && !(inverse & (inverse + 1));
}

inline bool load(Record& config) {
  Preferences storage;
  if (!storage.begin("wifi-ip", true)) return false; // Also normal on first boot.
  Record saved = {};
  const bool loaded = storage.getBytesLength("config") == sizeof(saved) &&
                      storage.getBytes("config", &saved, sizeof(saved)) == sizeof(saved);
  storage.end();
  if (!loaded || !valid(saved)) return false;
  config = saved;
  return true;
}

inline bool save(const Record& config) {
  if (!valid(config)) return false;
  Record previous;
  if (load(previous) && previous.ip == config.ip && previous.gateway == config.gateway &&
      previous.subnet == config.subnet && previous.dns == config.dns) return true;
  Preferences storage;
  if (!storage.begin("wifi-ip", false)) return false;
  const bool saved = storage.putBytes("config", &config, sizeof(config)) == sizeof(config);
  storage.end();
  return saved;
}

inline bool clear() {
  Preferences storage;
  if (!storage.begin("wifi-ip", false)) return false;
  const bool cleared = storage.clear();
  storage.end();
  return cleared;
}
} // namespace WifiIPConfig
