/*
***************************************************************************
**  Proxy DAL for shared DSMR frontend
***************************************************************************
*/

const DEBUG = false;

const UPDATE_HIST = DEBUG ? 1000 * 10 : 1000 * 300;
const UPDATE_ACTUAL = DEBUG ? 1000 * 5 : 1000 * 5;
const UPDATE_SOLAR = DEBUG ? 1000 * 30 : 1000 * 60;
const UPDATE_ACCU = DEBUG ? 1000 * 30 : 1000 * 60;
const UPDATE_PLAN = DEBUG ? 1000 * 10 : 1000 * 60;

if (!DEBUG) {
  console.log = function () {};
}

const PROXY_DEFAULT_SETTINGS = {
  hostname: { value: "proxy-device", type: "s", min: 0, max: 31 },
  ed_tariff1: { value: 0, type: "f", min: 0, max: 10 },
  ed_tariff2: { value: 0, type: "f", min: 0, max: 10 },
  er_tariff1: { value: 0, type: "f", min: 0, max: 10 },
  er_tariff2: { value: 0, type: "f", min: 0, max: 10 },
  gd_tariff: { value: 0, type: "f", min: 0, max: 10 },
  gas_netw_costs: { value: 0, type: "f", min: 0, max: 100 },
  electr_netw_costs: { value: 0, type: "f", min: 0, max: 100 },
  ota_url: { value: "ota.smart-stuff.nl/v5/", type: "s", min: 0, max: 64 },
  conf: "p1-p",
  hist: true,
  "eid-enabled": false,
  "eid-planner": false,
  "dev-pairing": false
};

const PROXY_EMPTY_RING = {
  actSlot: 0,
  data: [
    { date: "26010100", values: [0, 0, 0, 0, 0, 0, 0] }
  ]
};

const PROXY_EMPTY_INSIGHTS = {
  I1piek: 0,
  I2piek: 0,
  I3piek: 0,
  P1max: 0,
  P2max: 0,
  P3max: 0,
  P1min: 0,
  P2min: 0,
  P3min: 0,
  U1piek: 0,
  U2piek: 0,
  U3piek: 0,
  U1min: 0,
  U2min: 0,
  U3min: 0,
  TU1over: 0,
  TU2over: 0,
  TU3over: 0,
  Psluip: 0,
  start_time: 0
};

let ota_url = "ota.smart-stuff.nl/v5/";
let objDAL = null;

class dsmr_dal_main {
  constructor() {
    this.devinfo = [];
    this.time = [];
    this.dev_settings = structuredClone(PROXY_DEFAULT_SETTINGS);
    this.version_manifest = {};
    this.insights = structuredClone(PROXY_EMPTY_INSIGHTS);
    this.actual = [];
    this.telegram = "";
    this.fields = null;
    this.solar = { active: false };
    this.accu = { active: false };
    this.months = structuredClone(PROXY_EMPTY_RING);
    this.days = structuredClone(PROXY_EMPTY_RING);
    this.hours = structuredClone(PROXY_EMPTY_RING);
    this.eid_claim = {};
    this.netswitch = {};
    this.eid_planner = { h_start: 99, data: [] };
    this.actual_history = [];
    this.callback = null;
    this.timerREFRESH_TIME = 0;
    this.timerREFRESH_HIST = 0;
    this.timerREFRESH_ACTUAL = 0;
    this.timerREFRESH_SOLAR = 0;
    this.timerREFRESH_ACCU = 0;
    this.timerREFRESH_EIDPLAN = 0;
    this.summaryPromise = null;
    this.lastSummary = null;
  }

  setCallback(fnCB) {
    this.callback = fnCB;
  }

  init() {
    this.refreshDeviceInformation();
    this.refreshTime();
    this.refreshHist();
    this.refreshSolar();
    this.refreshAccu();
    this.refreshActual();
  }

  async fetchSummary() {
    if (!window.PROXY_CONTEXT?.deviceId) return this.demoSummary();
    const response = await fetch(`/api/proxy/devices/${encodeURIComponent(window.PROXY_CONTEXT.deviceId)}/summary`, {
      headers: { Accept: "application/json" }
    });
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    return response.json();
  }

  demoSummary() {
    const deviceId = window.PROXY_CONTEXT?.deviceId || "P1-001122334455";
    return Promise.resolve({
      device_id: deviceId,
      hostname: "p1-dongle",
      mac: "00:11:22:33:44:55",
      fw_version: "5.5.0",
      hardware: "P1P",
      timestamp: "260418174500S",
      online: true,
      ip: "192.168.1.42",
      live: {
        power_delivered_kw: 2.34,
        power_returned_kw: 0.41,
        voltage_l1: 230.4,
        voltage_l2: 229.8,
        voltage_l3: 231.1,
        current_l1: 5.9,
        current_l2: 3.2,
        current_l3: 2.1,
        gas_m3: 1243.210,
        water_m3: 81.440,
        quarter_peak_kw: 3.8,
        highest_peak_kw: 4.5
      },
      daily: {
        import_kwh: 9.42,
        export_kwh: 1.87,
        gas_m3: 0.63,
        water_m3: 0.18
      },
      daily_history: [
        { date: "2026-04-16", import_kwh: 8.66, export_kwh: 2.11, gas_m3: 0.59, water_m3: 0.19 },
        { date: "2026-04-17", import_kwh: 10.08, export_kwh: 1.53, gas_m3: 0.71, water_m3: 0.16 },
        { date: "2026-04-18", import_kwh: 9.42, export_kwh: 1.87, gas_m3: 0.63, water_m3: 0.18 }
      ],
      insights: structuredClone(PROXY_EMPTY_INSIGHTS)
    });
  }

  async syncSummary(force = false) {
    if (this.summaryPromise && !force) return this.summaryPromise;

    this.summaryPromise = this.fetchSummary()
      .catch(() => this.demoSummary())
      .then((summary) => {
        this.lastSummary = summary;
        this.applySummary(summary);
        this.summaryPromise = null;
        return summary;
      });

    return this.summaryPromise;
  }

  applySummary(summary) {
    this.fields = this.summaryToFields(summary);
    this.actual = structuredClone(this.fields);
    this.time = this.summaryToTime(summary);
    this.devinfo = this.summaryToDevInfo(summary);
    this.dev_settings = this.summaryToDeviceSettings(summary);
    this.insights = summary.insights || structuredClone(PROXY_EMPTY_INSIGHTS);
    this.days = this.summaryToDays(summary);
    this.hours = structuredClone(PROXY_EMPTY_RING);
    this.months = structuredClone(PROXY_EMPTY_RING);
    this.#addActualHistory(this.actual);

    this.callback?.("devinfo", this.devinfo);
    this.callback?.("dev_settings", this.dev_settings);
    this.callback?.("time", this.time);
    this.callback?.("fields", this.fields);
    this.callback?.("actual", this.actual);
    this.callback?.("days", this.days);
    this.callback?.("insights", this.insights);
    this.callback?.("solar", this.solar);
    this.callback?.("accu", this.accu);

    if (window.proxyShellUpdateMeta) window.proxyShellUpdateMeta(summary);
  }

  summaryToTime(summary) {
    const raw = this.normalizeRawTimestamp(summary.timestamp);
    return {
      timestamp: raw,
      time: this.formatTimestamp(raw),
      epoch: summary.epoch || Math.floor(Date.now() / 1000)
    };
  }

  summaryToDevInfo(summary) {
    return {
      fwversion: summary.fw_version || "-",
      hostname: summary.hostname || "-",
      ipaddress: summary.ip || "-",
      macaddress: summary.mac || "-",
      hardware: summary.hardware || "-",
      network: summary.online ? "Proxy" : "Offline",
      compileoptions: "[PROXY]",
      freeheap: { value: 0, unit: "bytes" }
    };
  }

  summaryToDeviceSettings(summary) {
    const settings = structuredClone(PROXY_DEFAULT_SETTINGS);
    settings.hostname.value = summary.hostname || settings.hostname.value;
    return settings;
  }

  summaryToFields(summary) {
    const history = this.summaryToDays(summary);
    const actSlot = history.actSlot;
    const currentValues = history.data[actSlot]?.values || [0, 0, 0, 0, 0, 0, 0];
    const live = summary.live || {};
    const rawTimestamp = this.normalizeRawTimestamp(summary.timestamp);

    return {
      timestamp: { value: rawTimestamp },
      electricity_tariff: { value: 1 },
      energy_delivered_tariff1: { value: currentValues[0], unit: "kWh" },
      energy_delivered_tariff2: { value: currentValues[1], unit: "kWh" },
      energy_returned_tariff1: { value: currentValues[2], unit: "kWh" },
      energy_returned_tariff2: { value: currentValues[3], unit: "kWh" },
      power_delivered: { value: live.power_delivered_kw || 0, unit: "kW" },
      power_returned: { value: live.power_returned_kw || 0, unit: "kW" },
      voltage_l1: { value: live.voltage_l1 ?? "-", unit: "V" },
      voltage_l2: { value: live.voltage_l2 ?? "-", unit: "V" },
      voltage_l3: { value: live.voltage_l3 ?? "-", unit: "V" },
      current_l1: { value: live.current_l1 ?? "-", unit: "A" },
      current_l2: { value: live.current_l2 ?? "-", unit: "A" },
      current_l3: { value: live.current_l3 ?? "-", unit: "A" },
      peak_pwr_last_q: { value: live.quarter_peak_kw || 0, unit: "kW" },
      highest_peak_pwr: { value: live.highest_peak_kw || 0, unit: "kW" },
      gas_delivered: { value: live.gas_m3 ?? 0, unit: "m3" },
      gas_delivered_timestamp: { value: rawTimestamp },
      water: { value: live.water_m3 ?? 0, unit: "m3" }
    };
  }

  summaryToDays(summary) {
    const rows = Array.isArray(summary.daily_history) && summary.daily_history.length
      ? [...summary.daily_history]
      : [{
          date: this.isoDateFromRaw(summary.timestamp),
          import_kwh: summary.daily?.import_kwh || 0,
          export_kwh: summary.daily?.export_kwh || 0,
          gas_m3: summary.daily?.gas_m3 || 0,
          water_m3: summary.daily?.water_m3 || 0
        }];

    rows.sort((a, b) => String(a.date).localeCompare(String(b.date)));

    let importCum = 0;
    let exportCum = 0;
    let gasCum = 0;
    let waterCum = 0;

    const data = rows.map((row) => {
      importCum += Number(row.import_kwh || 0);
      exportCum += Number(row.export_kwh || 0);
      gasCum += Number(row.gas_m3 || 0);
      waterCum += Number(row.water_m3 || 0);

      return {
        date: this.isoToCompact(row.date),
        values: [
          Number(importCum.toFixed(3)),
          0,
          Number(exportCum.toFixed(3)),
          0,
          Number(gasCum.toFixed(3)),
          Number(waterCum.toFixed(3)),
          Number(row.solar_kwh || 0)
        ]
      };
    });

    return {
      actSlot: Math.max(0, data.length - 1),
      data: data.length ? data : structuredClone(PROXY_EMPTY_RING.data)
    };
  }

  isoDateFromRaw(raw) {
    if (typeof raw !== "string") return "2026-01-01";
    if (raw.includes("-")) return raw.slice(0, 10);
    if (raw.length < 6) return "2026-01-01";
    return `20${raw.slice(0, 2)}-${raw.slice(2, 4)}-${raw.slice(4, 6)}`;
  }

  isoToCompact(value) {
    if (typeof value !== "string") return "26010100";
    const parts = value.split("-");
    if (parts.length !== 3) return "26010100";
    return `${parts[0].slice(2)}${parts[1]}${parts[2]}00`;
  }

  formatTimestamp(raw) {
    if (typeof raw !== "string" || raw.length < 12) return "-";
    return `20${raw.slice(0, 2)}-${raw.slice(2, 4)}-${raw.slice(4, 6)} ${raw.slice(6, 8)}:${raw.slice(8, 10)}:${raw.slice(10, 12)}`;
  }

  normalizeRawTimestamp(raw) {
    if (typeof raw !== "string" || raw.length === 0) return "260101000000W";
    if (!raw.includes("-")) return raw;

    const match = raw.match(/^(\d{4})-(\d{2})-(\d{2})[ T](\d{2}):(\d{2})(?::(\d{2}))?/);
    if (!match) return "260101000000W";

    const [, year, month, day, hour, minute, second = "00"] = match;
    return `${year.slice(2)}${month}${day}${hour}${minute}${second}W`;
  }

  refreshEIDPlanner() {
    this.eid_planner = { h_start: 99, data: [] };
    this.callback?.("eid_planner", this.eid_planner);
  }

  refreshHist() {
    clearInterval(this.timerREFRESH_HIST);
    this.syncSummary(true);
    this.timerREFRESH_HIST = setInterval(this.refreshHist.bind(this), UPDATE_HIST);
  }

  refreshNetSwitch() {
    this.netswitch = {};
    this.callback?.("netswitch", this.netswitch);
  }

  refreshEIDClaim() {
    this.eid_claim = {};
    this.callback?.("eid_claim", this.eid_claim);
  }

  refreshInsights() {
    this.syncSummary(false);
  }

  refreshSolar() {
    this.solar = { active: false };
    this.callback?.("solar", this.solar);
  }

  refreshAccu() {
    this.accu = { active: false };
    this.callback?.("accu", this.accu);
  }

  refreshDeviceInformation() {
    this.syncSummary(true);
  }

  refreshManifest() {}

  refreshTelegram() {
    this.telegram = "Proxy mode: raw telegram is not exposed in MVP.";
    this.callback?.("telegram", this.telegram);
  }

  refreshFields() {
    this.syncSummary(false);
  }

  refreshTime() {
    clearInterval(this.timerREFRESH_TIME);
    this.syncSummary(false);
    this.timerREFRESH_TIME = setInterval(this.refreshTime.bind(this), UPDATE_ACTUAL);
  }

  refreshActual() {
    this.syncSummary(false);
  }

  #addActualHistory(json) {
    if (this.actual_history.length >= 15 * 60 / 5) this.actual_history.shift();
    this.actual_history.push(structuredClone(json));
  }

  getDeviceInfo() { return this.devinfo; }
  getDeviceSettings() { return this.dev_settings; }
  getVersionManifest() { return this.version_manifest; }
  getMonths() { return this.months; }
  getDays() { return this.days; }
  getHours() { return this.hours; }
  getMonth() { return []; }
  getYear() { return []; }
  getSolar() { return this.solar; }
  getAccu() { return this.accu; }
  getFields() { return this.fields; }
  getActual() { return this.actual; }
  getActualHistory() { return this.actual_history; }
}

function updateFromDAL(source, json) {
  console.log("updateFromDAL(); source=" + source);

  switch (source) {
    case "time": refreshTime(json); update_reconnected = true; break;
    case "devinfo": parseDeviceInfo(json); break;
    case "dev_settings": ParseDevSettings(); refreshSettings(); break;
    case "versionmanifest": parseVersionManifest(json); break;
    case "days": expandData(json); refreshHistData("Days"); break;
    case "hours": expandData(json); refreshHistData("Hours"); break;
    case "months": expandData(json); refreshHistData("Months"); break;
    case "actual": ProcesActual(json); break;
    case "solar": UpdateSolar(); break;
    case "accu": UpdateAccu(); break;
    case "insights": InsightData(json); break;
    case "fields": SetOnSettings(json); if (activeTab == "bDashTab") refreshDashboard(json); else parseSmFields(json); break;
    case "telegram": {
      const el = document.getElementById("TelData");
      if (el) el.textContent = json;
      break;
    }
    case "eid_claim": ProcessEIDClaim(json); break;
    case "eid_planner": ProcessEIDPlanner(json); break;
    case "netswitch": refreshNetSwitch(json); break;
    default:
      console.error("missing handler; source=" + source);
      break;
  }
}

function initDAL(callbackFn) {
  objDAL = new dsmr_dal_main();
  objDAL.setCallback(callbackFn);
  objDAL.init();
}
