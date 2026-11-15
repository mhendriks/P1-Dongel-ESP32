# Release notes

## 5.11.0

Changes since 5.10.8.

### Added

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
