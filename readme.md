# DSMR-API for ESP32

This project provides firmware for the ESP32 to read Dutch Smart Meter (DSMR/P1) data via the P1 port and expose it via web interface, MQTT, json rest and other interfaces.

## Features

- Reads data from Dutch smart meters (DSMR v4 and v5 compatible)
- Built-in web interface for live readings and configuration
- (s)MQTT support for integration with platforms like Home Assistant
- Local storage up to 2 years
- Web-based configuration (Wi-Fi, MQTT, meter settings)
- OTA (Over-The-Air) update support
- Supports virtual P1
- Trigger io or Shelly switch based on system power
- Ethernet and Wifi support
- EnergyID support

## Hardware

- ESP32 (tested on ESP32-C3, ESP32-S3)
- P1 UART connection
- Powered via USB or via P1 port (depending on smart meter)

### Wiring (P1 DSMR cable → ESP32)

| P1 Pin | Signal      | ESP32 Pin |
|--------|-------------|-----------|
| 2      | GND         | GND       |
| 5      | TX (from meter) | GPIO3 (RX) or UART RX pin |
| (Optional) | 3.3V / 5V | 3V3 (if powered from P1) |

