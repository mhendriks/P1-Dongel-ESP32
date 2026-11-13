# Building Notes

This project expects a few local/private headers outside the repo and a compatible `dsmr3Lib` version.

## 1) Local `_secrets` headers (optional but expected by default)

Some source files include headers from:

`./../../_secrets/`

That resolves (relative to this project) to a sibling folder outside the repo. These files are used for local/private overrides and credentials.

### Expected files

1. `../../_secrets/posts.h`
2. `../../_secrets/energyid.h`
3. `../../_secrets/direct_ap.h`

The code now compiles without them (safe defaults are provided), but if you want the related features you should create them.

## 2) Example `posts.h`

Create `../../_secrets/posts.h` with at least:

```cpp
#pragma once

// Optional suffix appended to OTAURL, e.g. "latest/" or ""
#define OTAURL_PREFIX ""

// Optional MEENT endpoint override. DEBUG otherwise uses the MEENT staging
// API and release firmware uses the production API.
#define MEENT_API_BASE_URL "https://meent.dev.muze.nl/api/"
```

Notes:
- `OTAURL_PREFIX` is used in `DSMRloggerAPI.h` to build `BaseOTAurl`.
- HTTP POST is included in every build, but disabled until `/post.json` is
  placed on the dongle. It is consumed once at startup and stored in NVS.
- Supported `provider` values are `generic`, `kemp`, `meent`, and `powerc`. KEMP uses
  the sample/config contract in `docs/KEMP_API.md`; MEENT can also be selected
  from the settings page and is then provisioned through the same transport.
- `post.json` contains `enabled`, `provider`, `url`, `interval`, `payload`,
  `auth`, `auth_key`, `auth_name`, `extra_header_name`,
  `extra_header_value`, and `accept_interval`. See `docs/POST_JSON.md`.

## MEENT provisioning secret

MEENT builds create a cryptographically random 32-byte client secret before the
first pod/API-key request. It is stored in NVS (separate from the settings
file) and sent only to `/api/pod/` and `/api/register/` in the
`X-Client-Secret` header. The MEENT provider should store a hash of this secret
and use it to make interrupted provisioning requests idempotent. Override the
header name with `MEENT_CLIENT_SECRET_HEADER` in `posts.h` if the provider
chooses another name. A factory reset removes the secret deliberately.

## 3) Example `energyid.h`

Create `../../_secrets/energyid.h` with:

```cpp
#pragma once

#define EID_PROV_URL   "https://hooks.energyid.eu/hello"
#define EID_PROF_KEY   "replace-with-your-key"
#define EID_PROF_SECR  "replace-with-your-secret"
```

Notes:
- If you do not use EnergyID, the project can still compile without this file.
- When EnergyID is enabled in settings, these values must be valid.

## 4) Example `direct_ap.h`

Create `../../_secrets/direct_ap.h` for a closed-network firmware that connects directly to a customer AP:

```cpp
#pragma once

#define DIRECT_AP_CONNECT 1
#define DIRECT_AP_SSID_PREFIX "Vendor-"
#define DIRECT_AP_TARGET_SERIAL ""

#define DIRECT_AP_CONNECT_TIMEOUT_MS 45000
#define DIRECT_AP_SCAN_INTERVAL_MS 5000
#define DIRECT_AP_ENABLE_LOCAL_LOGS 0

#define DIRECT_AP_OTAURL_PREFIX "direct-ap/"
```

Notes:
- `DIRECT_AP_SSID_PREFIX` and `DIRECT_AP_TARGET_SERIAL` are compile-time settings; no filesystem config file is needed.
- `DIRECT_AP_TARGET_SERIAL` can be left empty to connect to the strongest matching AP.
- `DIRECT_AP_OTAURL_PREFIX` is used when `DIRECT_AP_CONNECT` is enabled, unless `OTAURL_PREFIX` is explicitly defined elsewhere.

## 5) Profile selection

Do not hardcode hardware profile defines in `P1-Dongel-ESP32.ino` when using `build.sh`.

Use `build.sh` to compile all profiles by default:

```bash
./build.sh
```

You can also compile one or more selected profiles:

```bash
./build.sh ULTRA
./build.sh P1P ETH_P1EP
```

Add `--clear-cache` or `--clean` to remove the selected profile build directories before compiling:

```bash
./build.sh --clear-cache ULTRA
```

Extra Arduino build properties can be passed through as well. For `compiler.cpp.extra_flags`, the value is appended to the profile flags:

```bash
./build.sh ULTRA --extra-flags "-DDEBUG"
./build.sh ULTRA --build-property compiler.cpp.extra_flags="-DDEBUG"
```

### Network regression test with ESP32 core 3.3.12

The custom WebSocket write-readiness check, forced disconnect on send failure,
pong timeout, and HTTP stale-client recovery have been removed to test the core's
network fixes. HTTP and WebSocket servers now use the standard library classes.
The WebSocket library heartbeat, two-client limit, and slow-send logging remain.
Use the default library timeouts for this test, without a
`WEBSOCKETS_TCP_TIMEOUT` build override.

On hardware, keep the live dashboard open, interrupt the browser's network
connection without closing the tab, then reconnect and reopen the dashboard.
Repeat with two tabs and with interrupted HTTP requests. Check that the dongle
keeps processing meter readings, HTTP requests and live updates without a stall
or reboot. A successful build alone does not verify recovery from these failures.

### Physical button: network reset and emergency factory reset

WiFi portal IP settings are stored separately in NVS namespace `wifi-ip` as
one versioned record containing IP, gateway, subnet and DNS. Settings are
written only after a successful portal connection and restored before
WiFiManager auto-connect. An empty IP field selects DHCP, including when
switching back from a static address. DNS is optional. Existing installations
without this record keep their existing startup behavior; previously entered
static WiFi settings must be entered once again after updating. Legacy
`/fixedip.json` takes precedence when present.

Actions occur on release on every hardware/provider profile:

- Up to five seconds: existing short-press behavior, green feedback.
- More than five and up to fifteen seconds: network reset, steady red feedback.
- More than fifteen seconds: emergency factory reset, red blinking at 250 ms
  on / 250 ms off while held. This removes the customer configuration.

The network reset:

- Clear saved WiFi credentials on hardware with WiFi support.
- Clear the WiFi IP settings from NVS (also on a web/factory WiFi reset).
- Clear an active `skip-network` flag in `DSMRsettings.json`.
- Remove `/fixedip.json` so WiFi/Ethernet use DHCP again.
- Clear cached DirectAP SSID/password when that firmware mode is enabled.
- Restart the dongle.

The settings document is preserved; only `skip-network` is patched when
needed. The update uses a temporary file and rename. If it cannot be saved,
the reset is cancelled rather than discarding configuration. This network
reset does not erase POST provisioning, MQTT settings, device credentials,
counters, history or pairing. A short press keeps its existing behavior.

The emergency factory reset removes `DSMRsettings.json`, pending `/post.json`,
fixed-IP settings, WiFi credentials, cached AP credentials, POST NVS settings
(including legacy KEMP settings), the MEENT client secret, pairing and the
EnergyID enrollment state. It preserves hardware metadata, counters, history
and frontend files. It is serialized through the worker; pending settings
and status writes are blocked so they cannot restore deleted provisioning.
After reboot the firmware creates standard settings. Upload the customer
`DSMRsettings.json` and `post.json` again to restore that customer profile.
Removing POST provisioning also restores the normal local web interface on
standard firmware. Firmware-defined DirectAP/hardware choices remain in force.

AP hotspot settings and DirectAP selection rules are firmware/provisioning
choices, not a separately saved AP mode. They remain in force after reset;
a DirectAP build scans for a matching AP again. The provisioning hotspot keeps
its configured SSID/password. This change applies to the physical button;
existing web/telnet reset actions keep their behavior.

### PowerC provisioning (5.10.5)

PowerC uses the standard firmware for each hardware profile. Include
`post.json` in the LittleFS filesystem image when flashing the dongle, using
`"provider": "powerc"` and `"enabled": true`. The file is imported
before networking starts, saved in NVS and removed after successful storage.
The profile remains active on subsequent boots and after a WiFi reset.
No PowerC compiler define is needed.

See `docs/POST_JSON.md` for the format. Customer-specific examples and tests
are kept locally under the Git-ignored `private/` directory.
Keep the real URL, interval, payload and authentication aligned
with the server contract. A sketch binary alone does not include LittleFS;
flash the filesystem image as well for factory provisioning.

While this provider is enabled, WiFiManager uses `p1-hotspot` for both the
hotspot SSID and password. The captive configuration portal remains available.
An optional local `private/wifi_branding.h` can define
`POWERC_WIFI_PORTAL_TITLE` as a string literal to customize the header title
for this provider. Without that header or define, the standard WiFiManager
title is used. The `private/` directory is excluded from Git.
The AP address is unchanged (`4.3.2.1`); `p1.local` is not configured for it.

Release firmware does not start the normal HTTP server, REST API or browser
WebSocket API, and does not advertise these services through mDNS. `DEBUG`
firmware retains these local services for inspection.

A five-to-fifteen-second button press follows the general network reset above.
Holding for more than fifteen seconds performs the emergency factory reset,
including removing the active POST profile.
`DSMRsettings.json`, POST provisioning and device state are preserved; fixed
IP settings and cached network credentials are cleared.

To disable the profile, provision another `post.json` with `"enabled": false`,
or select another provider, then reboot. In release firmware with local HTTP
disabled, reprovision through flashing the filesystem rather than the web UI.
This step does not implement MAC-derived IDs/passwords or new LED patterns.

### Sketch-local compiler options

The ESP32 Arduino core reads compiler options from `build_opt.h` in the sketch root. This project uses it to disable C++ exceptions without changing the globally installed ESP32 platform:

```text
-fno-exceptions
```

Arduino IDE and `arduino-cli` both apply this file automatically. A `platform.local.txt` in the sketch root is not supported; that file is only read next to the installed ESP32 `platform.txt`. Unlike `platform.local.txt`, `build_opt.h` is project-specific and remains part of the repository when the ESP32 core is updated.

The script injects profile-specific defines and board settings, including:

- ESP32-C3 builds must always use the `Minimal SPIFFS` partition scheme (`OTA 1.9MB / 128KB SPIFFS`).
- ESP32-S3 builds must always use the 8MB partition scheme (`FlashSize=8M`, `PartitionScheme=default_8MB`, OTA 3MB / matching 8MB layout).
- `ULTRA` -> `ESP32S3`, `FlashSize=8M`, `PartitionScheme=default_8MB`

## 6) Libraries used by this project

The codebase uses a mix of libraries from the ESP32 Arduino core and a small set of external libraries that must be installed separately.

### ESP32 Arduino core (installed via board package)

These headers come from the ESP32 board support package rather than a separate Arduino library install:

- `WiFi.h`, `WiFiClientSecure.h`, `HTTPClient.h`, `HTTPUpdate.h`, `Update.h`, `Preferences.h`, `LittleFS.h`, `ESPmDNS.h`, `AsyncUDP.h`
- `esp_wifi.h`, `esp_now.h`, `esp_sntp.h`, `esp_timer.h`, `esp_mac.h`, `esp_task_wdt.h`, `esp_chip_info.h`, `esp_system.h`, `esp_efuse.h`, `esp_efuse_table.h`, `rom/rtc.h`
- Repo: <https://github.com/espressif/arduino-esp32>

### External libraries (install separately)

- `ArduinoJson` (`ArduinoJson.h`)
  Repo: <https://github.com/bblanchon/ArduinoJson>
- `Time` (`TimeLib.h`)
  Repo: <https://github.com/PaulStoffregen/Time>
- `TelnetStream` (`TelnetStream.h`)
  Repo: <https://github.com/jandrassy/TelnetStream>
- `dsmr3Lib` (`dsmr3.h`)
  Repo: <https://github.com/mhendriks/dsmr3Lib>
  DSMR-API 5.9.0 and newer require dsmr3Lib 1.0.0 or a newer compatible
  release. The firmware uses the v3-only `P1FieldWarning`, `P1Diagnostics` and
  `CompleteRaw(String&)` APIs; dsmr2Lib cannot be substituted without
  reverting those integrations.
- `WiFiManager` (`WiFiManager.h`)
  Repo: <https://github.com/tzapu/WiFiManager>
- `CRC32` (`CRC32.h`)
  Repo: <https://github.com/bakercp/CRC32>
- `eModbus` (`ModbusServerRTU.h`)
  Repo: <https://github.com/eModbus/eModbus>
- `micro-ecc` (`uECC.h`)
  Repo: <https://github.com/kmackay/micro-ecc>
- `ESPAsyncWebServer` (`ESPAsyncWebServer.h`)
  Repo: <https://github.com/ESP32Async/ESPAsyncWebServer>
- `AsyncTCP` (`AsyncTCP.h`)
  Repo: <https://github.com/ESP32Async/AsyncTCP>

### Local project headers

These are included by the sketch but live in this repository, so they are not extra dependencies:

- `safeTimers.h`
- `espnow.h`
