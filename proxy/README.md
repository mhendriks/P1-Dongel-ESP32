# Proxy frontend + server

Deze map volgt nu het compatibiliteitsmodel:

- dezelfde DSMR-frontendassets als de dongle
- een kleine proxy-shell voor devicekeuze en feature gating
- een proxy-DAL die alleen transport doet
- een Python-proxyserver die de dongle-WS en browser-API/WS koppelt
- de gedeelde frontendbestanden komen dynamisch van jsDelivr op basis van de firmwareversie van het gekozen device

## Structuur

- `index.html`
  is een lichte bootstrap-pagina
- `proxy-shell.js`
  regelt linkerzijbalk, devicekeuze, feature gating en de CDN-keuze
- `dal-proxy.js`
  haalt HTTP-data op en opent een live-WS naar de proxy
- `server/app.py`
  biedt de MVP proxyservice

De proxy-shell kiest de gedeelde assetbasis zo:

- `fw_version = 5.5.0` -> `https://cdn.jsdelivr.net/gh/mhendriks/P1-Dongel-ESP32@5.5/cdn/`
- onbekend of leeg -> `https://cdn.jsdelivr.net/gh/mhendriks/P1-Dongel-ESP32@latest/cdn/`

Daardoor hoeft de server geen lokale `shared/v5` of `cdn` map meer te hebben.

## Server starten

```bash
cd proxy/server
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
python app.py
```

Standaard draait de server op `http://localhost:8787`.

## Frontend assetkeuze

Bij openen van een device vraagt de proxy eerst device-meta op. Daarna wordt de frontend van jsDelivr geladen op basis van de firmwareversie van dat device. Zo blijft de externe UI zo dicht mogelijk bij de lokale dongleversie.

## MVP-routes

HTTP:

- `GET /api/devices`
- `GET /api/v2/dev/info?device_id=...`
- `GET /api/v2/dev/time?device_id=...`
- `GET /api/v2/sm/actual?device_id=...`
- `GET /api/v2/hist/days?device_id=...`
- `GET /api/v2/insights?device_id=...`
- `POST /api/device/register`

WebSocket:

- `WS /ws/device-ingest`
- `WS /ws/live`

## Dongle ingest-berichten

Eerste bericht:

```json
{
  "type": "auth",
  "device_id": "P1-AB12CD34",
  "token": "devtok_...",
  "secret": "P1PRO-4A1B",
  "hostname": "p1-dongle",
  "fw_version": "5.8.1",
  "hw_version": "P1P",
  "mac_address": "AA:BB:CC:DD:EE:FF"
}
```

Daarna:

- `heartbeat`
- `live`
- `daily`

## Browser live-WS

Browser subscribe:

```json
{
  "type": "subscribe",
  "device_id": "P1-AB12CD34"
}
```

Proxy events:

- `sub_ack`
- `device_status`
- `live_update`
