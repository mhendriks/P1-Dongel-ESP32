# Release notes

## 5.11.1

This release integrates the network-unfix branch, upgrades the ESP32 Arduino
core, fixes Enphase daily history and consolidates solar configuration in
Connectors.

### Changed

- Updated the ESP32 Arduino core baseline to **3.3.13** (ESP-IDF 5.5.6+).
- Integrated the network-unfix changes from `sdk-3.3.12---network-unfix`:
  HTTP and WebSocket servers use the standard library classes again. Removed
  custom WebSocket write-readiness checks, forced disconnects on failed sends,
  the custom pong timeout, and HTTP stale-client recovery from the meter task.
  Library heartbeat, the two-client limit and slow-send logging remain enabled.
- Enphase, SMA and Omniksol settings are now available in separate Connectors
  tabs, with independent enable switches and preservation of existing settings.
- Removed Solar Setup and the SolarEdge HTTP integrations. Use the SolarEdge
  PV Modbus TCP connector instead; existing `solaredge.json` files are ignored.
- Battery and PV navigation both show **Modbus TCP**. Updated autocomplete
  hints for connector token/password fields to reduce browser login detection;
  password-manager prompts remain browser-dependent.

### Fixed

- Enphase readings require HTTP 200 and valid numeric power/day counters.
  Polling pauses from 23:00 until 05:00 on the dongle clock. Daily history
  retains the higher saved yield for the same date, including after a reboot,
  and the previous day's counter is not counted as production for a new day.

### Added

- **SolarEdge** battery 1 preset alongside Victron GX, with editable power,
  state-of-charge and operating-state mappings. See
  [connector setup](docs/CONNECTORS.md#solaredge-battery-preset).

### Validation

- Hardware tests reported successful: physical P1, WiFi, Ethernet, Virtual P1,
  Enphase and Victron battery integration. The web interface check also passed.
- SolarEdge PV and SolarEdge battery presets still require hardware validation.
- Clean release builds pass with ESP32 Arduino core 3.3.13 for P1P,
  NRG_DONGLE, ETHERNET, ETH_P1EP and ULTRA.
- All 21 Python tests, six JavaScript test files and the standalone Modbus
  uint32 test runner pass.

### Known issues

- On ULTRA, switching from Ethernet to WiFi during startup can print four
  IPv6 multicast-filter errors from the ESP-IDF/W5500 stack. ESP-IDF 5.5.6
  adds explicit removal of the IPv6 all-nodes filter (`ff02::1`) during network
  interface cleanup; the W5500 always accepts IPv6 multicast and rejects that
  removal request. Cleanup continues after the error and WiFi operation has
  been verified. No firmware workaround is applied; recheck with a future SDK.
- The existing `GPIO isr service already installed` startup message remains.
  It is separate from the IPv6 messages and is not introduced by this release.

## 5.11.0

Changes since 5.10.8.

### Fixed

- Enphase readings require HTTP 200 and valid numeric power/day counters.
  Polling pauses from 23:00 until 05:00 on the dongle clock, keeping the final
  day counter through midnight. Automatic daily history updates preserve the
  higher saved yield for the same date, including after a reboot.

### Added

- Enphase, SMA and Omniksol configuration in Connectors, preserving existing
  files and supporting independent enable switches. Solar Setup and SolarEdge
  HTTP polling have been removed; SolarEdge uses the PV Modbus TCP connector.

- A **Connectors** page with separate **Battery** and **PV** tabs for reading
  devices over Modbus TCP. Both connectors are read-only and use holding
  registers (FC03).
- Editable **Victron GX** battery and **SolarEdge SunSpec** PV presets, plus
  custom mappings. Configure the IP address, port, unit ID, polling interval,
  register addresses, datatypes, multipliers and word order from the web interface.
- SunSpec scale-factor registers for PV power and lifetime production, with
  daily production calculated from a baseline that survives a reboot.
- Opt-in **Diagnostics** on the Connectors page, showing connection status,
  the age of the last successful read, the last Modbus error and register,
  raw register values, SunSpec exponents and calculated measurements.
  **Copy diagnostics** exports the saved connection/mapping and recent results
  for troubleshooting, including on local HTTP connections.
  Capture is off by default, keeps at most 12 results per connector in RAM,
  and is disabled and cleared after a reboot or when switched off.

### Changed

- The frontend is pinned to the **5.11.0** release tag so firmware and web
  interface use the corresponding release assets, including translations.
- Firmware updates now use one dialog in the same style as the progress screen.
  **Download data** and **Update** are separate actions: requesting downloads
  never starts a firmware update automatically. Check the three downloaded
  history files before starting the update.
- Update dialogs have a close icon in the top-right corner, uppercase button
  labels and vertically stacked action buttons on mobile. Closing the progress
  dialog leaves an already started firmware update running.
- System information shows **Virtual P1** as the source when a Virtual P1 address
  is configured, instead of showing the physical serial port's DSMR mode.
  Telegram count labels are shorter in all six interface languages.
- Compile options are no longer exposed through device information or shown in
  the web interface.

### Compatibility and setup

- Existing Victron battery settings and the dongle's Modbus server mappings are
  retained. Opening the Connectors page does not overwrite saved mappings;
  applying a preset is an explicit action.
- The PV TCP connector replaces the SolarEdge HTTP contribution while enabled.
  Disable it to resume the configured SolarEdge HTTP integration. Other PV
  sources continue to contribute.
- Daily PV production starts at the first valid lifetime reading of the day;
  production before that reading cannot be recovered on installation day or
  when the dongle was offline at the start of the day.
- See [connector setup and register mapping](docs/CONNECTORS.md) for connection
  settings, register addresses and scale factors.

### Validation

- Automated tests cover connector decoding, SunSpec scale factors, polling,
  failure handling, daily baselines, opt-in diagnostics and copy fallbacks,
  frontend configuration and update dialogs.
- Communication and displayed measurements still need validation against an
  actual SolarEdge inverter and Victron GX. For SolarEdge, enable Modbus TCP
  and select the inverter's configured port; the preset starts with port 1502
  and unit ID 1.
