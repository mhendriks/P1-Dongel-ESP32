# Proxy frontend

Deze map gebruikt nu Variant A:

- gedeelde frontend-assets uit `dsmr-api/v5`
- een proxy-specifieke `dal-proxy.js`
- een kleine `proxy-shell.js` voor devicekeuze, linkerzijbalk en feature gating

De proxy laadt dezelfde:

- `DSMRindex.css`
- `DSMRindex_body.html`
- `DSMRgraphics.js`
- `DSMRindex.js`
- `burnup.js`

Alleen de DAL is anders.

Verwacht proxy-endpoint:

- `GET /api/proxy/devices/:device_id/summary`

Minimale response:

```json
{
  "device_id": "P1-001122334455",
  "hostname": "p1-dongle",
  "mac": "00:11:22:33:44:55",
  "fw_version": "5.5.0",
  "hardware": "P1P",
  "timestamp": "2026-04-18 15:45:12",
  "online": true,
  "ip": "192.168.1.42",
  "live": {
    "power_delivered_kw": 2.34,
    "power_returned_kw": 0.41,
    "voltage_l1": 230.4,
    "voltage_l2": 229.8,
    "voltage_l3": 231.1,
    "current_l1": 5.9,
    "current_l2": 3.2,
    "current_l3": 2.1,
    "gas_m3": 1243.21,
    "water_m3": 81.44,
    "quarter_peak_kw": 3.8,
    "highest_peak_kw": 4.5
  },
  "daily": {
    "import_kwh": 9.42,
    "export_kwh": 1.87,
    "gas_m3": 0.63,
    "water_m3": 0.18
  },
  "daily_history": [
    {
      "date": "2026-04-18",
      "import_kwh": 9.42,
      "export_kwh": 1.87,
      "gas_m3": 0.63,
      "water_m3": 0.18
    }
  ],
  "insights": {
    "I1piek": 0,
    "I2piek": 0,
    "I3piek": 0
  }
}
```

`dal-proxy.js` zet deze response om naar dezelfde frontend-shapes die de donglefrontend verwacht voor:

- `fields`
- `dev/info`
- `dev/settings`
- `time`
- `hist/days`
- `stats`
