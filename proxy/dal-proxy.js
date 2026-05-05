/*
***************************************************************************
**  Proxy DAL for shared DSMR frontend
***************************************************************************
*/

const DEBUG = false;

const UPDATE_HIST = DEBUG ? 1000 * 10 : 1000 * 300;
const UPDATE_ACTUAL = DEBUG ? 1000 * 5 : 1000 * 15;
const UPDATE_SOLAR = DEBUG ? 1000 * 30 : 1000 * 60;
const UPDATE_ACCU = DEBUG ? 1000 * 30 : 1000 * 60;

if (!DEBUG) {
  console.log = function () {};
}

const APIHOST = window.location.protocol + "//" + window.location.host;
const APIGW = APIHOST + "/api/";

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
window.objDAL = window.objDAL || null;

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
    this.liveSocket = null;
    this.lastPayload = {
      devinfo: this.devinfo,
      time: this.time,
      fields: this.fields,
      actual: this.actual,
      days: this.days,
      insights: this.insights
    };
  }

  setCallback(fnCB) {
    this.callback = fnCB;
  }

  endpoint(path) {
    if (!window.PROXY_CONTEXT?.deviceId) return null;
    const url = new URL(path, window.location.href);
    url.searchParams.set("device_id", window.PROXY_CONTEXT.deviceId);
    if (window.PROXY_CONTEXT?.viewerSessionId) {
      url.searchParams.set("viewer_session", window.PROXY_CONTEXT.viewerSessionId);
    }
    return url.toString();
  }

  async fetchJson(path, fallback) {
    const endpoint = this.endpoint(path);
    if (!endpoint) return fallback;
    const response = await fetch(endpoint, { headers: { Accept: "application/json" } });
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    return response.json();
  }

  init() {
    this.refreshDeviceInformation();
    this.refreshTime();
    this.refreshHist();
    this.refreshSolar();
    this.refreshAccu();
    this.refreshActual();
    this.refreshInsights();
    this.connectLiveSocket();
  }

  async refreshDeviceInformation() {
    try {
      this.devinfo = await this.fetchJson("./api/v2/dev/info", {});
    } catch {
      this.devinfo = {};
    }
    this.callback?.("devinfo", this.devinfo);
    if (window.proxyShellUpdateMeta) {
      window.proxyShellUpdateMeta({
        device_id: window.PROXY_CONTEXT?.deviceId || "-",
        hostname: this.devinfo.hostname || "-",
        fw_version: this.devinfo.fwversion || "-",
        hardware: this.devinfo.hardware || "-",
        online: this.devinfo.network !== "Offline",
        timestamp: this.time?.timestamp || "-"
      });
    }
  }

  async refreshManifest() {}

  async refreshTime() {
    clearInterval(this.timerREFRESH_TIME);
    await this.loadActualBundle();
    this.timerREFRESH_TIME = setInterval(this.refreshTime.bind(this), UPDATE_ACTUAL);
  }

  async refreshHist() {
    clearInterval(this.timerREFRESH_HIST);
    try {
      this.days = await this.fetchJson("./api/v2/hist/days", structuredClone(PROXY_EMPTY_RING));
    } catch {
      this.days = structuredClone(PROXY_EMPTY_RING);
    }
    this.callback?.("days", this.days);
    this.timerREFRESH_HIST = setInterval(this.refreshHist.bind(this), UPDATE_HIST);
  }

  async refreshInsights() {
    try {
      this.insights = await this.fetchJson("./api/v2/insights", structuredClone(PROXY_EMPTY_INSIGHTS));
    } catch {
      this.insights = structuredClone(PROXY_EMPTY_INSIGHTS);
    }
    this.callback?.("insights", this.insights);
  }

  refreshNetSwitch() {
    this.netswitch = {};
    this.callback?.("netswitch", this.netswitch);
  }

  refreshEIDClaim() {
    this.eid_claim = {};
    this.callback?.("eid_claim", this.eid_claim);
  }

  refreshEIDPlanner() {
    this.eid_planner = { h_start: 99, data: [] };
    this.callback?.("eid_planner", this.eid_planner);
  }

  refreshSolar() {
    clearInterval(this.timerREFRESH_SOLAR);
    this.solar = { active: false };
    this.callback?.("solar", this.solar);
    this.timerREFRESH_SOLAR = setInterval(this.refreshSolar.bind(this), UPDATE_SOLAR);
  }

  refreshAccu() {
    clearInterval(this.timerREFRESH_ACCU);
    this.accu = { active: false };
    this.callback?.("accu", this.accu);
    this.timerREFRESH_ACCU = setInterval(this.refreshAccu.bind(this), UPDATE_ACCU);
  }

  async refreshActual() {
    await this.loadActualBundle();
  }

  async refreshFields() {
    await this.loadActualBundle();
  }

  async loadActualBundle() {
    try {
      const [time, fields, settings] = await Promise.all([
        this.fetchJson("./api/v2/dev/time", {}),
        this.fetchJson("./api/v2/sm/actual", null),
        this.fetchJson("./api/v2/dev/settings", structuredClone(PROXY_DEFAULT_SETTINGS))
      ]);

      this.time = time || {};
      this.fields = fields || this.emptyFields();
      this.actual = structuredClone(this.fields);
      this.dev_settings = settings || structuredClone(PROXY_DEFAULT_SETTINGS);
      this.#addActualHistory(this.actual);

      this.callback?.("time", this.time);
      this.callback?.("fields", this.fields);
      this.callback?.("actual", this.actual);
      this.callback?.("dev_settings", this.dev_settings);
    } catch {
      if (!this.fields) {
        this.fields = this.emptyFields();
        this.actual = structuredClone(this.fields);
        this.callback?.("fields", this.fields);
        this.callback?.("actual", this.actual);
      }
    }
  }

  emptyFields() {
    return {
      timestamp: { value: "260101000000W" },
      electricity_tariff: { value: 1 },
      energy_delivered_tariff1: { value: 0, unit: "kWh" },
      energy_delivered_tariff2: { value: 0, unit: "kWh" },
      energy_returned_tariff1: { value: 0, unit: "kWh" },
      energy_returned_tariff2: { value: 0, unit: "kWh" },
      power_delivered: { value: 0, unit: "kW" },
      power_returned: { value: 0, unit: "kW" },
      voltage_l1: { value: "-", unit: "V" },
      voltage_l2: { value: "-", unit: "V" },
      voltage_l3: { value: "-", unit: "V" },
      current_l1: { value: "-", unit: "A" },
      current_l2: { value: "-", unit: "A" },
      current_l3: { value: "-", unit: "A" },
      peak_pwr_last_q: { value: 0, unit: "kW" },
      highest_peak_pwr: { value: 0, unit: "kW" },
      gas_delivered: { value: 0, unit: "m3" },
      gas_delivered_timestamp: { value: "260101000000W" },
      water: { value: 0, unit: "m3" }
    };
  }

  connectLiveSocket() {
    if (!window.PROXY_CONTEXT?.deviceId) return;
    if (this.liveSocket) this.liveSocket.close();

    const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
    const liveUrl = `${protocol}//${window.location.host}/ws/live?device_id=${encodeURIComponent(window.PROXY_CONTEXT.deviceId)}`;
    this.liveSocket = new WebSocket(liveUrl);

    this.liveSocket.addEventListener("open", () => {
      this.liveSocket.send(JSON.stringify({
        type_id: 8,
        device_id: window.PROXY_CONTEXT.deviceId
      }));
    });

    this.liveSocket.addEventListener("message", (event) => {
      let message;
      try {
        message = JSON.parse(event.data);
      } catch {
        return;
      }

      if (message.type_id === 11) {
        this.applyLiveUpdate(message);
        return;
      }

      if (message.type_id === 10 && window.proxyShellUpdateMeta) {
        window.proxyShellUpdateMeta({
          device_id: message.device_id,
          hostname: this.devinfo.hostname || "-",
          fw_version: this.devinfo.fwversion || "-",
          hardware: this.devinfo.hardware || "-",
          online: !!message.online,
          timestamp: this.time?.timestamp || "-"
        });
      }
    });

    this.liveSocket.addEventListener("close", () => {
      window.setTimeout(() => this.connectLiveSocket(), 3000);
    });
  }

  applyLiveUpdate(update) {
    if (!this.fields) this.fields = this.emptyFields();

    this.fields.timestamp = { value: update.timestamp || this.fields.timestamp?.value || "260101000000W" };
    this.fields.energy_delivered_tariff1 = { value: Number(update.energy_delivered_tariff1 || 0), unit: "kWh" };
    this.fields.energy_delivered_tariff2 = { value: Number(update.energy_delivered_tariff2 || 0), unit: "kWh" };
    this.fields.energy_returned_tariff1 = { value: Number(update.energy_returned_tariff1 || 0), unit: "kWh" };
    this.fields.energy_returned_tariff2 = { value: Number(update.energy_returned_tariff2 || 0), unit: "kWh" };
    this.fields.power_delivered = { value: Number(update.power_delivered || 0), unit: "kW" };
    this.fields.power_returned = { value: Number(update.power_returned || 0), unit: "kW" };
    this.fields.voltage_l1 = { value: update.voltage_l1 ?? "-", unit: "V" };
    this.fields.voltage_l2 = { value: update.voltage_l2 ?? "-", unit: "V" };
    this.fields.voltage_l3 = { value: update.voltage_l3 ?? "-", unit: "V" };
    this.fields.current_l1 = { value: update.current_l1 ?? "-", unit: "A" };
    this.fields.current_l2 = { value: update.current_l2 ?? "-", unit: "A" };
    this.fields.current_l3 = { value: update.current_l3 ?? "-", unit: "A" };
    this.fields.peak_pwr_last_q = { value: Number(update.peak_pwr_last_q || 0), unit: "kW" };
    this.fields.highest_peak_pwr = { value: Number(update.highest_peak_pwr || 0), unit: "kW" };
    this.fields.gas_delivered = { value: Number(update.gas_delivered || 0), unit: "m3" };
    this.fields.gas_delivered_timestamp = { value: update.gas_delivered_timestamp || this.fields.timestamp.value };
    this.fields.water = { value: Number(update.water || 0), unit: "m3" };

    this.actual = structuredClone(this.fields);
    this.time = {
      timestamp: this.fields.timestamp.value,
      time: this.formatTimestamp(this.fields.timestamp.value),
      epoch: Math.floor(Date.now() / 1000)
    };

    this.#addActualHistory(this.actual);
    this.callback?.("time", this.time);
    this.callback?.("fields", this.fields);
    this.callback?.("actual", this.actual);

    if (window.proxyShellUpdateMeta) {
      window.proxyShellUpdateMeta({
        device_id: window.PROXY_CONTEXT.deviceId,
        hostname: this.devinfo.hostname || "-",
        fw_version: this.devinfo.fwversion || "-",
        hardware: this.devinfo.hardware || "-",
        online: true,
        timestamp: this.fields.timestamp.value
      });
    }
  }

  formatTimestamp(raw) {
    if (typeof raw !== "string" || raw.length < 12) return "-";
    return `20${raw.slice(0, 2)}-${raw.slice(2, 4)}-${raw.slice(4, 6)} ${raw.slice(6, 8)}:${raw.slice(8, 10)}:${raw.slice(10, 12)}`;
  }

  refreshTelegram() {
    this.telegram = "Proxy mode: raw telegram is not exposed in MVP.";
    this.callback?.("telegram", this.telegram);
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
  window.objDAL = new dsmr_dal_main();
  window.objDAL.setCallback(callbackFn);
  window.objDAL.init();
}
