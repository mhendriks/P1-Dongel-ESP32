# DSMR-API

DSMR-API is firmware for ESP32-based Smartstuff energy dongles. It reads data from a smart meter through the P1 port and makes current and historical energy data available through a local web interface and open integrations.

This README describes the functionality available in firmware version 5.9.0.

## Features

- Automatic smart-meter protocol detection and P1 telegram diagnostics
- Current electricity, gas and optional water readings
- Local history storage for up to two years
- Standalone, multilingual web interface for monitoring and configuration
- Wi-Fi and, on supported hardware, Ethernet connectivity
- JSON REST APIs and a live WebSocket API
- MQTT with optional TLS and Home Assistant MQTT Discovery
- Modbus TCP on network-connected dongles
- Modbus RTU/RS485 with the `NRG_DONGLE` and `ULTRA` profiles
- Configurable Modbus register mappings for several third-party meters and devices
- EnergyID, solar, battery/Victron and Shelly integrations
- Virtual P1 support on the Ethernet Pro+ and Ultra families
- OTA firmware updates and browser-based installation over USB

Available functionality depends on the connected meter and the selected hardware profile.

## Hardware-specific features

Most functionality is shared by all profiles, including the web interface, local history, REST and WebSocket APIs, MQTT, Home Assistant Discovery, Modbus TCP, EnergyID, solar and battery integrations, ESP-NOW and OTA updates. The following features depend on the build profile:

| Build profile | Target | MCU | Network | NetSwitch | Virtual P1 | Modbus RTU |
| --- | --- | --- | --- | :---: | :---: | :---: |
| `P1P` | P1 Dongle Pro | ESP32-C3 | Wi-Fi | — | — | — |
| `ETHERNET` | Ethernet Dongle Pro | ESP32-C3 | Ethernet | Yes | — | — |
| `ETH_P1EP` | Ethernet Dongle Pro+ | ESP32-C3 | Ethernet | Yes | Yes | — |
| `NRG_DONGLE` | NRG Dongle Pro / compatible NRG hardware | ESP32-C3 | Wi-Fi | — | — | Yes |
| `ULTRA` | Ultra and Ultra Mini | ESP32-S3 | Ethernet with Wi-Fi fallback | Yes | Yes | Yes |

- **NetSwitch** switches a local output or a Shelly relay using configurable import or export power thresholds and delays.
- **Virtual P1** reads P1 telegrams from another network-connected dongle instead of the local meter input. It is intended for hardware with a P1 output.
- **Modbus RTU** requires an RS485-capable hardware variant or module. Modbus TCP is available in every standard profile.
- **EnergyID** is available in every profile but is enabled by default only in the Ultra build.

Optional connectors, water inputs, P1 outputs and extension modules still depend on the exact hardware revision. Always select firmware that matches the device: partition layouts and pin assignments differ between profiles.

## Installation and documentation

For normal installation and use, start with the official documentation:

- [DSMR-API manual](https://docs.smart-stuff.nl/dsmr-api)
- [Wi-Fi setup](https://docs.smart-stuff.nl/dsmr-api/installeren/wifi-koppeling)
- [Web installer and firmware variants](https://docs.smart-stuff.nl/dsmr-api/geavanceerd/webinstaller)
- [Modbus RTU/TCP](https://docs.smart-stuff.nl/dsmr-api/geavanceerd/modbus-rtu-tcp)

The web installer is the recommended way to install released firmware. Back up locally stored history files before changing firmware variants or performing an installation that may erase flash storage.

## Building from source

Development builds require the Arduino ESP32 core, `arduino-cli` and several external libraries. See [docs/BUILDING.md](docs/BUILDING.md) for board settings, required libraries, optional private configuration headers and profile-specific build details.

## API and integrations

The device exposes versioned HTTP endpoints under `/api/v1` and `/api/v2`, together with a WebSocket feed for live data. MQTT publishing, Home Assistant discovery, Modbus and external integrations can be configured from the device web interface where supported.

Interface behaviour may change between development versions. Consumers should prefer documented, versioned endpoints.

## License and credits

This project is distributed under the [MIT License](LICENSE).

DSMR-API is maintained by Smartstuff and is based on the DSMR API work by Willem Aandewiel.
