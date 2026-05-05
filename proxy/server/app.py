from __future__ import annotations

import json
import secrets
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from aiohttp import WSMsgType, web


HOST = "0.0.0.0"
PORT = 8787
PUBLIC_ROOT = Path(__file__).resolve().parent.parent
REPO_ROOT = PUBLIC_ROOT.parent
DATA_ROOT_CANDIDATES = [
    PUBLIC_ROOT / "data",
    REPO_ROOT / "data",
]

MSG_AUTH = 1
MSG_AUTH_ACK = 2
MSG_MODE = 3
MSG_HEARTBEAT = 4
MSG_LIVE = 5
MSG_DAILY = 6
MSG_ERROR = 7
MSG_SUBSCRIBE = 8
MSG_SUB_ACK = 9
MSG_DEVICE_STATUS = 10
MSG_LIVE_UPDATE = 11

STATUS_OK = 1
STATUS_ERROR = 2

MODE_IDLE = 0
MODE_LIVE = 1


def now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


def iso_to_raw_timestamp(value: str | None) -> str:
    if value:
        try:
            dt = datetime.fromisoformat(value.replace("Z", "+00:00"))
        except ValueError:
            dt = datetime.now(timezone.utc)
    else:
        dt = datetime.now(timezone.utc)
    return dt.strftime("%y%m%d%H%M%SW")


def raw_timestamp_to_display(value: str | None) -> str:
    raw = str(value or "")
    if len(raw) < 12:
        return now_iso()
    year = f"20{raw[0:2]}"
    month = raw[2:4]
    day = raw[4:6]
    hour = raw[6:8]
    minute = raw[8:10]
    second = raw[10:12]
    return f"{year}-{month}-{day} {hour}:{minute}:{second}"


def sanitize_float(value: Any, fallback: float | str = 0) -> float | str:
    try:
        num = float(value)
    except (TypeError, ValueError):
        return fallback
    return num


def mint_token() -> str:
    return f"devtok_{secrets.token_hex(18)}"


def json_response(payload: Any, status: int = 200) -> web.Response:
    return web.json_response(payload, status=status, headers={"Cache-Control": "no-store"})


async def handle_index(_: web.Request) -> web.FileResponse:
    return web.FileResponse(PUBLIC_ROOT / "index.html", headers={"Cache-Control": "no-store"})


async def handle_frontend_json(_: web.Request) -> web.FileResponse:
    for data_root in DATA_ROOT_CANDIDATES:
        candidate = data_root / "Frontend.json"
        if candidate.exists():
            return web.FileResponse(candidate, headers={"Cache-Control": "no-store"})
    raise web.HTTPNotFound(text="Frontend.json not found")


@dataclass
class DeviceMeta:
    hostname: str = ""
    fw_version: str = ""
    hardware: str = ""
    mac_address: str = ""
    ip_address: str = ""


@dataclass
class DeviceState:
    device_id: str
    secret: str = ""
    token: str = ""
    online: bool = False
    mode: str = "idle"
    viewer_count: int = 0
    last_seen: str | None = None
    last_actual: dict[str, Any] | None = None
    last_daily: dict[str, Any] | None = None
    daily_history: list[dict[str, Any]] = field(default_factory=list)
    meta: DeviceMeta = field(default_factory=DeviceMeta)
    ingest_ws: web.WebSocketResponse | None = None


class ProxyState:
    def __init__(self) -> None:
        self.devices: dict[str, DeviceState] = {}
        self.browser_clients: set[web.WebSocketResponse] = set()
        self.browser_sessions: dict[str, str] = {}

    def lookup_device(self, device_id: str) -> DeviceState | None:
        return self.devices.get(device_id)

    def ensure_device(self, device_id: str) -> DeviceState:
        if device_id not in self.devices:
            self.devices[device_id] = DeviceState(device_id=device_id)
        return self.devices[device_id]

    def update_daily_history(self, device: DeviceState, payload: dict[str, Any]) -> None:
        day = str(payload.get("day") or "").strip()
        if not day:
            return
        row = {
            "date": day,
            "import_t1_kwh": sanitize_float(payload.get("import_t1_kwh")),
            "import_t2_kwh": sanitize_float(payload.get("import_t2_kwh")),
            "export_t1_kwh": sanitize_float(payload.get("export_t1_kwh")),
            "export_t2_kwh": sanitize_float(payload.get("export_t2_kwh")),
            "import_kwh": sanitize_float(payload.get("import_kwh")),
            "export_kwh": sanitize_float(payload.get("export_kwh")),
            "gas_m3": sanitize_float(payload.get("gas_m3")),
            "water_m3": sanitize_float(payload.get("water_m3")),
            "solar_kwh": sanitize_float(payload.get("solar_kwh")),
        }
        filtered = [entry for entry in device.daily_history if entry["date"] != row["date"]]
        filtered.append(row)
        filtered.sort(key=lambda entry: str(entry["date"]))
        device.daily_history = filtered[-45:]

    async def send_device_mode(self, device: DeviceState, mode: str) -> None:
        next_mode = "live" if mode == "live" else "idle"
        if device.mode == next_mode:
            return
        device.mode = next_mode
        if device.ingest_ws is not None and not device.ingest_ws.closed:
            payload = {
                "type_id": MSG_MODE,
                "mode_id": MODE_LIVE if device.mode == "live" else MODE_IDLE,
                "heartbeat_interval": 60,
                "live_interval": 5,
            }
            await device.ingest_ws.send_json(payload)

    async def update_viewer_mode(self, device_id: str) -> None:
        device = self.ensure_device(device_id)
        ws_viewers = sum(1 for client in self.browser_clients if getattr(client, "device_id", "") == device_id)
        session_viewers = sum(1 for session_device_id in self.browser_sessions.values() if session_device_id == device_id)
        viewer_count = ws_viewers + session_viewers
        device.viewer_count = viewer_count
        await self.send_device_mode(device, "live" if viewer_count > 0 else "idle")

    async def open_viewer_session(self, session_id: str, device_id: str) -> DeviceState:
        previous_device_id = self.browser_sessions.get(session_id)
        self.browser_sessions[session_id] = device_id
        if previous_device_id and previous_device_id != device_id:
            await self.update_viewer_mode(previous_device_id)
        await self.update_viewer_mode(device_id)
        return self.ensure_device(device_id)

    async def close_viewer_session(self, session_id: str) -> str | None:
        device_id = self.browser_sessions.pop(session_id, None)
        if device_id:
            await self.update_viewer_mode(device_id)
        return device_id

    def build_fields(self, device: DeviceState) -> dict[str, Any]:
        actual = device.last_actual or {}
        return {
            "timestamp": {"value": actual.get("meter_ts") or iso_to_raw_timestamp(actual.get("ts") or device.last_seen)},
            "electricity_tariff": {"value": int(actual.get("electricity_tariff") or 1)},
            "energy_delivered_tariff1": {"value": sanitize_float(actual.get("energy_delivered_t1_kwh")), "unit": "kWh"},
            "energy_delivered_tariff2": {"value": sanitize_float(actual.get("energy_delivered_t2_kwh")), "unit": "kWh"},
            "energy_returned_tariff1": {"value": sanitize_float(actual.get("energy_returned_t1_kwh")), "unit": "kWh"},
            "energy_returned_tariff2": {"value": sanitize_float(actual.get("energy_returned_t2_kwh")), "unit": "kWh"},
            "power_delivered": {"value": sanitize_float(actual.get("power_delivered_kw")), "unit": "kW"},
            "power_returned": {"value": sanitize_float(actual.get("power_returned_kw")), "unit": "kW"},
            "voltage_l1": {"value": sanitize_float(actual.get("voltage_l1_v"), "-"), "unit": "V"},
            "voltage_l2": {"value": sanitize_float(actual.get("voltage_l2_v"), "-"), "unit": "V"},
            "voltage_l3": {"value": sanitize_float(actual.get("voltage_l3_v"), "-"), "unit": "V"},
            "current_l1": {"value": sanitize_float(actual.get("current_l1_a"), "-"), "unit": "A"},
            "current_l2": {"value": sanitize_float(actual.get("current_l2_a"), "-"), "unit": "A"},
            "current_l3": {"value": sanitize_float(actual.get("current_l3_a"), "-"), "unit": "A"},
            "peak_pwr_last_q": {"value": sanitize_float(actual.get("peak_pwr_last_q_kw")), "unit": "kW"},
            "highest_peak_pwr": {"value": sanitize_float(actual.get("highest_peak_pwr_kw")), "unit": "kW"},
            "gas_delivered": {"value": sanitize_float(actual.get("gas_delivered_m3")), "unit": "m3"},
            "gas_delivered_timestamp": {
                "value": actual.get("gas_ts") or iso_to_raw_timestamp(actual.get("ts") or device.last_seen)
            },
            "water": {"value": sanitize_float(actual.get("water_m3")), "unit": "m3"},
        }

    def build_devinfo(self, device: DeviceState) -> dict[str, Any]:
        return {
            "fwversion": device.meta.fw_version or "-",
            "hostname": device.meta.hostname or device.device_id,
            "ipaddress": device.meta.ip_address or "-",
            "macaddress": device.meta.mac_address or "-",
            "hardware": device.meta.hardware or "-",
            "network": "Proxy" if device.online else "Offline",
            "compileoptions": "[PROXY]",
            "freeheap": {"value": 0, "unit": "bytes"},
        }

    def build_time(self, device: DeviceState) -> dict[str, Any]:
        actual = device.last_actual or {}
        time_value = actual.get("ts") or device.last_seen or now_iso()
        timestamp = actual.get("meter_ts") or iso_to_raw_timestamp(time_value)
        try:
            epoch = int(datetime.fromisoformat(time_value.replace("Z", "+00:00")).timestamp())
        except ValueError:
            epoch = int(datetime.now(timezone.utc).timestamp())
        return {"timestamp": timestamp, "time": raw_timestamp_to_display(timestamp), "epoch": epoch}

    def build_days(self, device: DeviceState) -> dict[str, Any]:
        rows = list(device.daily_history)
        if not rows and device.last_daily:
            rows.append(
                {
                    "date": device.last_daily.get("day"),
                    "import_t1_kwh": sanitize_float(device.last_daily.get("import_t1_kwh")),
                    "import_t2_kwh": sanitize_float(device.last_daily.get("import_t2_kwh")),
                    "export_t1_kwh": sanitize_float(device.last_daily.get("export_t1_kwh")),
                    "export_t2_kwh": sanitize_float(device.last_daily.get("export_t2_kwh")),
                    "gas_m3": sanitize_float(device.last_daily.get("gas_m3")),
                    "water_m3": sanitize_float(device.last_daily.get("water_m3")),
                    "solar_kwh": sanitize_float(device.last_daily.get("solar_kwh")),
                }
            )

        import_t1 = 0.0
        import_t2 = 0.0
        export_t1 = 0.0
        export_t2 = 0.0
        gas = 0.0
        water = 0.0
        data: list[dict[str, Any]] = []

        for row in rows:
            import_t1 += float(sanitize_float(row.get("import_t1_kwh", row.get("import_kwh")), 0))
            import_t2 += float(sanitize_float(row.get("import_t2_kwh"), 0))
            export_t1 += float(sanitize_float(row.get("export_t1_kwh", row.get("export_kwh")), 0))
            export_t2 += float(sanitize_float(row.get("export_t2_kwh"), 0))
            gas += float(sanitize_float(row.get("gas_m3"), 0))
            water += float(sanitize_float(row.get("water_m3"), 0))
            day = str(row.get("date") or "").replace("-", "")
            data.append(
                {
                    "date": f"{day[2:]}00" if len(day) >= 8 else "26010100",
                    "values": [
                        round(import_t1, 3),
                        round(import_t2, 3),
                        round(export_t1, 3),
                        round(export_t2, 3),
                        round(gas, 3),
                        round(water, 3),
                        round(float(sanitize_float(row.get("solar_kwh"), 0)), 3),
                    ],
                }
            )

        if not data:
            data = [{"date": iso_to_raw_timestamp(now_iso())[:8], "values": [0, 0, 0, 0, 0, 0, 0]}]

        return {"actSlot": max(0, len(data) - 1), "data": data}

    def build_summary(self, device: DeviceState) -> dict[str, Any]:
        time_data = self.build_time(device)
        actual = device.last_actual or {}
        return {
            "device_id": device.device_id,
            "hostname": device.meta.hostname or device.device_id,
            "mac": device.meta.mac_address or "-",
            "fw_version": device.meta.fw_version or "-",
            "hardware": device.meta.hardware or "-",
            "timestamp": time_data["timestamp"],
            "epoch": time_data["epoch"],
            "online": device.online,
            "ip": device.meta.ip_address or "-",
            "live": {
                "power_delivered_kw": sanitize_float(actual.get("power_delivered_kw")),
                "power_returned_kw": sanitize_float(actual.get("power_returned_kw")),
                "voltage_l1": sanitize_float(actual.get("voltage_l1_v")),
                "voltage_l2": sanitize_float(actual.get("voltage_l2_v")),
                "voltage_l3": sanitize_float(actual.get("voltage_l3_v")),
                "current_l1": sanitize_float(actual.get("current_l1_a")),
                "current_l2": sanitize_float(actual.get("current_l2_a")),
                "current_l3": sanitize_float(actual.get("current_l3_a")),
                "gas_m3": sanitize_float(actual.get("gas_delivered_m3")),
                "water_m3": sanitize_float(actual.get("water_m3")),
                "quarter_peak_kw": sanitize_float(actual.get("peak_pwr_last_q_kw")),
                "highest_peak_kw": sanitize_float(actual.get("highest_peak_pwr_kw")),
            },
            "daily": device.last_daily or {},
            "daily_history": device.daily_history,
            "insights": {
                "I1piek": 0,
                "I2piek": 0,
                "I3piek": 0,
                "P1max": 0,
                "P2max": 0,
                "P3max": 0,
                "P1min": 0,
                "P2min": 0,
                "P3min": 0,
                "U1piek": 0,
                "U2piek": 0,
                "U3piek": 0,
                "U1min": 0,
                "U2min": 0,
                "U3min": 0,
                "TU1over": 0,
                "TU2over": 0,
                "TU3over": 0,
                "Psluip": 0,
                "start_time": 0,
            },
        }

    async def broadcast_device_status(self, device: DeviceState) -> None:
        payload = {
            "type_id": MSG_DEVICE_STATUS,
            "device_id": device.device_id,
            "mode_id": MODE_LIVE if device.mode == "live" else MODE_IDLE,
            "online": device.online,
            "last_seen": device.last_seen,
        }
        await self._broadcast_to_device(device.device_id, payload)

    async def broadcast_live(self, device: DeviceState) -> None:
        actual = device.last_actual
        if not actual:
            return
        payload = {
            "type_id": MSG_LIVE_UPDATE,
            "device_id": device.device_id,
            "timestamp": actual.get("meter_ts") or iso_to_raw_timestamp(actual.get("ts") or device.last_seen),
            "power_delivered": sanitize_float(actual.get("power_delivered_kw")),
            "power_returned": sanitize_float(actual.get("power_returned_kw")),
            "net_power": float(sanitize_float(actual.get("power_delivered_kw"), 0))
            - float(sanitize_float(actual.get("power_returned_kw"), 0)),
            "voltage_l1": sanitize_float(actual.get("voltage_l1_v")),
            "voltage_l2": sanitize_float(actual.get("voltage_l2_v")),
            "voltage_l3": sanitize_float(actual.get("voltage_l3_v")),
            "current_l1": sanitize_float(actual.get("current_l1_a")),
            "current_l2": sanitize_float(actual.get("current_l2_a")),
            "current_l3": sanitize_float(actual.get("current_l3_a")),
            "peak_pwr_last_q": sanitize_float(actual.get("peak_pwr_last_q_kw")),
            "highest_peak_pwr": sanitize_float(actual.get("highest_peak_pwr_kw")),
            "gas_delivered": sanitize_float(actual.get("gas_delivered_m3")),
            "gas_delivered_timestamp": actual.get("gas_ts") or iso_to_raw_timestamp(actual.get("ts") or device.last_seen),
            "water": sanitize_float(actual.get("water_m3")),
            "energy_delivered_tariff1": sanitize_float(actual.get("energy_delivered_t1_kwh")),
            "energy_delivered_tariff2": sanitize_float(actual.get("energy_delivered_t2_kwh")),
            "energy_returned_tariff1": sanitize_float(actual.get("energy_returned_t1_kwh")),
            "energy_returned_tariff2": sanitize_float(actual.get("energy_returned_t2_kwh")),
        }
        await self._broadcast_to_device(device.device_id, payload)

    async def _broadcast_to_device(self, device_id: str, payload: dict[str, Any]) -> None:
        stale_clients: list[web.WebSocketResponse] = []
        for client in self.browser_clients:
            if getattr(client, "device_id", "") != device_id:
                continue
            if client.closed:
                stale_clients.append(client)
                continue
            await client.send_json(payload)
        for client in stale_clients:
            self.browser_clients.discard(client)


STATE = ProxyState()


async def handle_health(_: web.Request) -> web.Response:
    return json_response({"ok": True, "now": now_iso(), "devices": len(STATE.devices)})


async def handle_devices(_: web.Request) -> web.Response:
    return json_response(
        {
            "devices": [
                {
                    "device_id": device.device_id,
                    "hostname": device.meta.hostname or device.device_id,
                    "online": device.online,
                    "mode": device.mode,
                    "last_seen": device.last_seen,
                    "fw_version": device.meta.fw_version or "-",
                    "hardware": device.meta.hardware or "-",
                }
                for device in STATE.devices.values()
            ]
        }
    )


async def handle_register(request: web.Request) -> web.Response:
    try:
        body = await request.json()
    except json.JSONDecodeError:
        return json_response({"error": "invalid_json"}, status=400)

    device_id = str(body.get("device_id") or "").strip()
    secret = str(body.get("secret") or "").strip()
    if not device_id or not secret:
        return json_response({"error": "device_id_and_secret_required"}, status=400)

    device = STATE.ensure_device(device_id)
    device.secret = secret
    if not device.token:
        device.token = str(body.get("token") or "").strip() or mint_token()
    return json_response({"device_id": device.device_id, "token": device.token})


def device_from_query(request: web.Request) -> tuple[DeviceState | None, web.Response | None]:
    device_id = str(request.query.get("device_id") or "").strip()
    if not device_id:
        return None, json_response({"error": "device_id_required"}, status=400)
    return STATE.ensure_device(device_id), None


async def maybe_open_viewer_from_request(request: web.Request, device_id: str) -> None:
    session_id = str(request.query.get("viewer_session") or "").strip()
    if session_id and device_id:
        await STATE.open_viewer_session(session_id, device_id)


async def handle_summary(request: web.Request) -> web.Response:
    device_id = request.match_info["device_id"]
    device = STATE.lookup_device(device_id)
    if device is None:
        return json_response({"error": "unknown_device", "device_id": device_id}, status=404)
    await maybe_open_viewer_from_request(request, device_id)
    return json_response(STATE.build_summary(device))


async def handle_dev_info(request: web.Request) -> web.Response:
    device, error = device_from_query(request)
    if error:
        return error
    await maybe_open_viewer_from_request(request, device.device_id)
    return json_response(STATE.build_devinfo(device))


async def handle_dev_time(request: web.Request) -> web.Response:
    device, error = device_from_query(request)
    if error:
        return error
    await maybe_open_viewer_from_request(request, device.device_id)
    return json_response(STATE.build_time(device))


async def handle_dev_settings(request: web.Request) -> web.Response:
    device, error = device_from_query(request)
    if error:
        return error
    await maybe_open_viewer_from_request(request, device.device_id)
    return json_response(
        {
            "hostname": {"value": device.meta.hostname or device.device_id, "type": "s", "min": 0, "max": 31},
            "hist": True,
            "conf": "p1-p",
            "eid-enabled": False,
            "eid-planner": False,
            "dev-pairing": False,
        }
    )


async def handle_sm_actual(request: web.Request) -> web.Response:
    device, error = device_from_query(request)
    if error:
        return error
    await maybe_open_viewer_from_request(request, device.device_id)
    return json_response(STATE.build_fields(device))


async def handle_hist_days(request: web.Request) -> web.Response:
    device, error = device_from_query(request)
    if error:
        return error
    await maybe_open_viewer_from_request(request, device.device_id)
    return json_response(STATE.build_days(device))


async def handle_insights(request: web.Request) -> web.Response:
    device, error = device_from_query(request)
    if error:
        return error
    await maybe_open_viewer_from_request(request, device.device_id)
    return json_response(STATE.build_summary(device)["insights"])


async def handle_viewer_open(request: web.Request) -> web.Response:
    try:
        body = await request.json()
    except json.JSONDecodeError:
        return json_response({"error": "invalid_json"}, status=400)

    session_id = str(body.get("session_id") or "").strip()
    device_id = str(body.get("device_id") or "").strip()
    if not session_id or not device_id:
        return json_response({"error": "session_id_and_device_id_required"}, status=400)

    device = await STATE.open_viewer_session(session_id, device_id)
    return json_response({"ok": True, "device_id": device.device_id, "mode": device.mode, "viewer_count": device.viewer_count})


async def handle_viewer_close(request: web.Request) -> web.Response:
    try:
        body = await request.json()
    except json.JSONDecodeError:
        return json_response({"error": "invalid_json"}, status=400)

    session_id = str(body.get("session_id") or "").strip()
    if not session_id:
        return json_response({"error": "session_id_required"}, status=400)

    device_id = await STATE.close_viewer_session(session_id)
    return json_response({"ok": True, "device_id": device_id or ""})


async def handle_device_ingest_ws(request: web.Request) -> web.StreamResponse:
    ws = web.WebSocketResponse(heartbeat=30, protocols=("arduino",))
    await ws.prepare(request)
    ws.device_id = ""

    async for message in ws:
        if message.type != WSMsgType.TEXT:
            continue
        try:
            payload = json.loads(message.data)
        except json.JSONDecodeError:
            await ws.send_json({"type_id": MSG_ERROR, "status_id": STATUS_ERROR, "reason": "invalid_json"})
            continue

        msg_type_id = int(payload.get("type_id") or 0)

        if msg_type_id == MSG_AUTH:
            device_id = str(payload.get("device_id") or "").strip()
            token = str(payload.get("token") or "").strip()
            secret = str(payload.get("secret") or "").strip()
            if not device_id:
                await ws.send_json({"type_id": MSG_AUTH_ACK, "status_id": STATUS_ERROR, "reason": "missing_device_id"})
                continue

            device = STATE.ensure_device(device_id)
            token_matches = bool(token and device.token and token == device.token)
            secret_matches = bool(secret and (not device.secret or secret == device.secret))
            if not token_matches and not secret_matches:
                await ws.send_json({"type_id": MSG_AUTH_ACK, "status_id": STATUS_ERROR, "reason": "invalid_credentials"})
                await ws.close()
                break

            if not device.secret and secret:
                device.secret = secret
            if not device.token:
                device.token = token or mint_token()

            ws.device_id = device_id
            device.online = True
            device.last_seen = now_iso()
            device.ingest_ws = ws
            device.meta.hostname = str(payload.get("hostname") or device.meta.hostname or device_id)
            device.meta.fw_version = str(payload.get("fw_version") or device.meta.fw_version)
            device.meta.hardware = str(payload.get("hw_version") or payload.get("hardware") or device.meta.hardware)
            device.meta.mac_address = str(payload.get("mac_address") or device.meta.mac_address)
            device.meta.ip_address = str(payload.get("ip") or device.meta.ip_address)

            await STATE.update_viewer_mode(device_id)
            await ws.send_json(
                {
                    "type_id": MSG_AUTH_ACK,
                    "status_id": STATUS_OK,
                    "token": device.token,
                    "mode_id": MODE_LIVE if device.mode == "live" else MODE_IDLE,
                    "heartbeat_interval": 60,
                    "live_interval": 5,
                    "server_time": now_iso(),
                }
            )
            await STATE.broadcast_device_status(device)
            continue

        if not ws.device_id:
            await ws.send_json({"type_id": MSG_ERROR, "status_id": STATUS_ERROR, "reason": "not_authenticated"})
            continue

        device = STATE.ensure_device(ws.device_id)
        device.online = True
        device.last_seen = str(payload.get("ts") or now_iso())

        if msg_type_id == MSG_HEARTBEAT:
            device.meta.hostname = str(payload.get("hostname") or device.meta.hostname)
            device.meta.fw_version = str(payload.get("fw_version") or device.meta.fw_version)
            device.meta.hardware = str(payload.get("hw_version") or payload.get("hardware") or device.meta.hardware)
            device.meta.mac_address = str(payload.get("mac_address") or device.meta.mac_address)
            device.meta.ip_address = str(payload.get("ip") or device.meta.ip_address)
            await STATE.update_viewer_mode(device.device_id)
            await STATE.broadcast_device_status(device)
            continue

        if msg_type_id == MSG_LIVE:
            device.last_actual = payload
            device.meta.hostname = str(payload.get("hostname") or device.meta.hostname)
            device.meta.fw_version = str(payload.get("fw_version") or device.meta.fw_version)
            device.meta.hardware = str(payload.get("hardware") or device.meta.hardware)
            device.meta.mac_address = str(payload.get("mac_address") or device.meta.mac_address)
            device.meta.ip_address = str(payload.get("ip") or device.meta.ip_address)
            await STATE.update_viewer_mode(device.device_id)
            await STATE.broadcast_live(device)
            continue

        if msg_type_id == MSG_DAILY:
            device.last_daily = payload
            STATE.update_daily_history(device, payload)

    if ws.device_id:
        device = STATE.ensure_device(ws.device_id)
        if device.ingest_ws is ws:
            device.ingest_ws = None
        device.online = False
        device.last_seen = now_iso()
        await STATE.broadcast_device_status(device)

    return ws


async def handle_live_ws(request: web.Request) -> web.StreamResponse:
    ws = web.WebSocketResponse(heartbeat=30)
    await ws.prepare(request)
    ws.device_id = str(request.query.get("device_id") or "").strip()

    if ws.device_id:
        STATE.browser_clients.add(ws)
        await STATE.update_viewer_mode(ws.device_id)
        device = STATE.ensure_device(ws.device_id)
        await ws.send_json(
            {
                "type_id": MSG_SUB_ACK,
                "status_id": STATUS_OK,
                "device_id": ws.device_id,
                "online": device.online,
                "mode_id": MODE_LIVE if device.mode == "live" else MODE_IDLE,
            }
        )
        if device.last_actual:
            await STATE.broadcast_live(device)
        await STATE.broadcast_device_status(device)

    async for message in ws:
        if message.type != WSMsgType.TEXT:
            continue
        try:
            payload = json.loads(message.data)
        except json.JSONDecodeError:
            continue

        msg_type_id = int(payload.get("type_id") or 0)

        if msg_type_id == MSG_SUBSCRIBE:
            if ws.device_id:
                STATE.browser_clients.discard(ws)
                await STATE.update_viewer_mode(ws.device_id)
            ws.device_id = str(payload.get("device_id") or "").strip()
            if not ws.device_id:
                continue
            STATE.browser_clients.add(ws)
            await STATE.update_viewer_mode(ws.device_id)
            device = STATE.ensure_device(ws.device_id)
            await ws.send_json(
                {
                    "type_id": MSG_SUB_ACK,
                    "status_id": STATUS_OK,
                    "device_id": ws.device_id,
                    "online": device.online,
                    "mode_id": MODE_LIVE if device.mode == "live" else MODE_IDLE,
                }
            )
            if device.last_actual:
                await STATE.broadcast_live(device)
            await STATE.broadcast_device_status(device)

    if ws.device_id:
        STATE.browser_clients.discard(ws)
        await STATE.update_viewer_mode(ws.device_id)

    return ws


def build_app() -> web.Application:
    app = web.Application(client_max_size=1024**2)

    app.router.add_get("/", handle_index)
    app.router.add_get("/Frontend.json", handle_frontend_json)
    app.router.add_get("/health", handle_health)
    app.router.add_get("/api/devices", handle_devices)
    app.router.add_post("/api/device/register", handle_register)
    app.router.add_post("/api/proxy/viewer/open", handle_viewer_open)
    app.router.add_post("/api/proxy/viewer/close", handle_viewer_close)
    app.router.add_get("/api/proxy/devices/{device_id}/summary", handle_summary)
    app.router.add_get("/api/v2/dev/info", handle_dev_info)
    app.router.add_get("/api/v2/dev/time", handle_dev_time)
    app.router.add_get("/api/v2/dev/settings", handle_dev_settings)
    app.router.add_get("/api/v2/sm/actual", handle_sm_actual)
    app.router.add_get("/api/v2/hist/days", handle_hist_days)
    app.router.add_get("/api/v2/insights", handle_insights)
    app.router.add_get("/ws/device-ingest", handle_device_ingest_ws)
    app.router.add_get("/ws/live", handle_live_ws)

    app.router.add_static("/", str(PUBLIC_ROOT), show_index=False)
    return app


app = build_app()


if __name__ == "__main__":
    web.run_app(app, host=HOST, port=PORT)
