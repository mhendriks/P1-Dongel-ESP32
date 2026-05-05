/*
***************************************************************************
**  Proxy shell for shared DSMR frontend
***************************************************************************
*/

window.PROXY_CONTEXT = {
  deviceId: new URLSearchParams(window.location.search).get("device_id") || "",
  sharedBase: "",
  fwVersion: "",
  visibleTabs: ["bDashTab", "bInsightsTab", "bDaysTab"],
  viewerSessionId: sessionStorage.getItem("proxy_viewer_session_id") || "",
  viewerDeviceId: sessionStorage.getItem("proxy_viewer_device_id") || ""
};

if (!window.PROXY_CONTEXT.viewerSessionId) {
  window.PROXY_CONTEXT.viewerSessionId = `viewer-${Date.now()}-${Math.random().toString(36).slice(2, 10)}`;
  sessionStorage.setItem("proxy_viewer_session_id", window.PROXY_CONTEXT.viewerSessionId);
}

const proxySidebarRefs = {};

function bindDrawerRefs() {
  proxySidebarRefs.toggle = document.getElementById("proxy-drawer-toggle");
  proxySidebarRefs.form = document.getElementById("proxy-device-form");
  proxySidebarRefs.input = document.getElementById("proxy-device-id");
  proxySidebarRefs.datalist = document.getElementById("proxy-device-list");
  proxySidebarRefs.status = document.getElementById("proxy-status-pill");
  proxySidebarRefs.device = document.getElementById("proxy-meta-device");
  proxySidebarRefs.hostname = document.getElementById("proxy-meta-hostname");
  proxySidebarRefs.firmware = document.getElementById("proxy-meta-firmware");
  proxySidebarRefs.hardware = document.getElementById("proxy-meta-hardware");
  proxySidebarRefs.updated = document.getElementById("proxy-meta-updated");

  proxySidebarRefs.input.value = window.PROXY_CONTEXT.deviceId;

  proxySidebarRefs.toggle.addEventListener("click", () => {
    document.body.classList.toggle("proxy-drawer-open");
  });

  proxySidebarRefs.form.addEventListener("submit", (event) => {
    event.preventDefault();
    const deviceId = proxySidebarRefs.input.value.trim();
    if (!deviceId) return;

    if (window.PROXY_CONTEXT.viewerDeviceId && window.PROXY_CONTEXT.viewerDeviceId !== deviceId) {
      viewerCloseSync();
    }

    const nextUrl = new URL(window.location.href);
    nextUrl.searchParams.set("device_id", deviceId);
    window.location.href = nextUrl.toString();
  });
}

function drawerMarkup() {
  return `
    <aside class="proxy-drawer" id="proxy-drawer">
      <button class="proxy-drawer-handle" id="proxy-drawer-toggle" type="button" aria-label="Proxy paneel tonen of verbergen">
        <span class="proxy-drawer-handle-title">Remote Access</span>
      </button>
      <div class="proxy-drawer-card">
        <div class="proxy-brand">
          <p>Smartstuff</p>
          <h1>Remote Access</h1>
        </div>
        <form class="proxy-device-picker" id="proxy-device-form">
          <label for="proxy-device-id">Device ID</label>
          <input id="proxy-device-id" name="device_id" type="text" placeholder="P1-001122334455" list="proxy-device-list">
          <datalist id="proxy-device-list"></datalist>
          <button type="submit">Open device</button>
        </form>
        <section class="proxy-meta">
          <div class="proxy-status" id="proxy-status-pill">Verbinden...</div>
          <div class="proxy-meta-row">
            <span>Device</span>
            <strong id="proxy-meta-device">-</strong>
          </div>
          <div class="proxy-meta-row">
            <span>Hostname</span>
            <strong id="proxy-meta-hostname">-</strong>
          </div>
          <div class="proxy-meta-row">
            <span>Firmware</span>
            <strong id="proxy-meta-firmware">-</strong>
          </div>
          <div class="proxy-meta-row">
            <span>Hardware</span>
            <strong id="proxy-meta-hardware">-</strong>
          </div>
          <div class="proxy-meta-row">
            <span>Laatste update</span>
            <strong id="proxy-meta-updated">-</strong>
          </div>
        </section>
      </div>
    </aside>
  `;
}

function injectDrawer() {
  document.body.insertAdjacentHTML("beforeend", drawerMarkup());
  bindDrawerRefs();
}

async function viewerOpen(deviceId) {
  if (!deviceId) return;
  await fetch("./api/proxy/viewer/open", {
    method: "POST",
    headers: { "Content-Type": "application/json", Accept: "application/json" },
    body: JSON.stringify({
      session_id: window.PROXY_CONTEXT.viewerSessionId,
      device_id: deviceId
    })
  });
  window.PROXY_CONTEXT.viewerDeviceId = deviceId;
  sessionStorage.setItem("proxy_viewer_device_id", deviceId);
}

function viewerCloseSync() {
  const sessionId = window.PROXY_CONTEXT.viewerSessionId;
  if (!sessionId) return;
  const payload = JSON.stringify({ session_id: sessionId });
  if (navigator.sendBeacon) {
    navigator.sendBeacon("./api/proxy/viewer/close", new Blob([payload], { type: "application/json" }));
  }
  sessionStorage.removeItem("proxy_viewer_device_id");
}

window.proxyShellUpdateMeta = function proxyShellUpdateMeta(summary) {
  if (!proxySidebarRefs.status) return;
  proxySidebarRefs.status.textContent = summary.online ? "Online" : "Offline";
  proxySidebarRefs.status.classList.toggle("offline", !summary.online);
  proxySidebarRefs.device.textContent = summary.device_id || "-";
  proxySidebarRefs.hostname.textContent = summary.hostname || "-";
  proxySidebarRefs.firmware.textContent = summary.fw_version || "-";
  proxySidebarRefs.hardware.textContent = summary.hardware || "-";
  proxySidebarRefs.updated.textContent = summary.timestamp || "-";
};

function firmwareToCdnTag(fwVersion) {
  const match = String(fwVersion || "").match(/^(\d+)\.(\d+)/);
  if (!match) return "latest";
  return `${match[1]}.${match[2]}`;
}

function sharedBaseFromVersion(fwVersion) {
  const tag = firmwareToCdnTag(fwVersion);
  return `https://cdn.jsdelivr.net/gh/mhendriks/P1-Dongel-ESP32@${tag}/cdn`;
}

function ensureStylesheet(id, href) {
  let link = document.getElementById(id);
  if (!link) {
    link = document.createElement("link");
    link.id = id;
    link.rel = "stylesheet";
    document.head.appendChild(link);
  }
  link.href = href;
  return link;
}

function ensureFontStylesheet() {
  let link = document.getElementById("proxy-font-dosis");
  if (!link) {
    link = document.createElement("link");
    link.id = "proxy-font-dosis";
    link.rel = "stylesheet";
    document.head.appendChild(link);
  }
  link.href = "https://fonts.googleapis.com/css?family=Dosis:400,700";
}

function ensureFavicon(href) {
  let link = document.querySelector("link[rel='icon']");
  if (!link) {
    link = document.createElement("link");
    link.rel = "icon";
    link.type = "image/png";
    link.sizes = "32x32";
    document.head.appendChild(link);
  }
  link.href = href;
}

function loadScriptOnce(src) {
  return new Promise((resolve, reject) => {
    const existing = Array.from(document.scripts).find((script) => script.src === src);
    if (existing) {
      if (existing.dataset.loaded === "true") return resolve();
      existing.addEventListener("load", () => resolve(), { once: true });
      existing.addEventListener("error", () => reject(new Error(`Unable to load ${src}`)), { once: true });
      return;
    }

    const script = document.createElement("script");
    script.src = src;
    script.async = false;
    script.addEventListener("load", () => {
      script.dataset.loaded = "true";
      resolve();
    }, { once: true });
    script.addEventListener("error", () => reject(new Error(`Unable to load ${src}`)), { once: true });
    document.body.appendChild(script);
  });
}

async function loadSelectedDeviceSummary() {
  if (!window.PROXY_CONTEXT.deviceId) return null;
  const url = new URL(`./api/proxy/devices/${encodeURIComponent(window.PROXY_CONTEXT.deviceId)}/summary`, window.location.href);
  url.searchParams.set("viewer_session", window.PROXY_CONTEXT.viewerSessionId);
  const response = await fetch(url.toString(), {
    headers: { Accept: "application/json" }
  });
  if (!response.ok) {
    if (response.status === 404) {
      throw new Error(`Device ${window.PROXY_CONTEXT.deviceId} is nog niet gekoppeld of niet bekend op de proxy`);
    }
    throw new Error(`Device summary failed: ${response.status}`);
  }
  return response.json();
}

async function determineSharedBase() {
  if (!window.PROXY_CONTEXT.deviceId) {
    window.PROXY_CONTEXT.sharedBase = sharedBaseFromVersion("");
    ensureStylesheet("proxy-shared-css", `${window.PROXY_CONTEXT.sharedBase}/DSMRindex.min.css`);
    ensureFontStylesheet();
    ensureFavicon(`${window.PROXY_CONTEXT.sharedBase}/favicon/favicon-32x32.png`);
    return null;
  }

  let summary = null;
  try {
    summary = await loadSelectedDeviceSummary();
  } catch (error) {
    console.warn("Unable to load selected device summary", error);
    proxySidebarRefs.status.textContent = "Onbekend";
    proxySidebarRefs.status.classList.add("offline");
    proxySidebarRefs.device.textContent = window.PROXY_CONTEXT.deviceId || "-";
    proxySidebarRefs.hostname.textContent = "-";
    proxySidebarRefs.firmware.textContent = "-";
    proxySidebarRefs.hardware.textContent = "-";
    proxySidebarRefs.updated.textContent = "-";
  }

  window.PROXY_CONTEXT.fwVersion = summary?.fw_version || "";
  window.PROXY_CONTEXT.sharedBase = sharedBaseFromVersion(window.PROXY_CONTEXT.fwVersion);
  ensureStylesheet("proxy-shared-css", `${window.PROXY_CONTEXT.sharedBase}/DSMRindex.min.css`);
  ensureFontStylesheet();
  ensureFavicon(`${window.PROXY_CONTEXT.sharedBase}/favicon/favicon-32x32.png`);
  if (summary) window.proxyShellUpdateMeta(summary);
  return summary;
}

async function loadKnownDevices() {
  try {
    const response = await fetch("./api/devices", { headers: { Accept: "application/json" } });
    if (!response.ok) return;
    const payload = await response.json();
    const options = (payload.devices || [])
      .map((device) => `<option value="${device.device_id}">${device.hostname || device.device_id}</option>`)
      .join("");
    if (proxySidebarRefs.datalist) {
      proxySidebarRefs.datalist.innerHTML = options;
    }
  } catch (error) {
    console.warn("Unable to load device list", error);
  }
}

changeLanguage = function proxyChangeLanguage(lang) {
  locale = lang;
  localStorage.setItem("locale", lang);
  loadTranslations(lang);

  const nextUrl = new URL(window.location.href);
  window.location.href = nextUrl.toString();
};

function hideProxyTab(buttonId, sectionId) {
  const btn = document.getElementById(buttonId);
  if (btn) {
    const li = btn.closest("li");
    if (li) li.style.display = "none";
    else btn.style.display = "none";
  }

  const section = document.getElementById(sectionId);
  if (section) section.style.display = "none";
}

function applyProxyFeatureGate() {
  [
    ["bActualTab", "ActualTab"],
    ["bHoursTab", "HoursTab"],
    ["bMonthsTab", "MonthsTab"],
    ["bPlafondTab", "PlafondTab"],
    ["bSysInfoTab", "SysInfoTab"],
    ["bFSExplorer", "FSExplorer"],
    ["bSettings", "Settings"],
    ["bEditSettings", "Settings"],
    ["bEditMonths", "EditMonths"],
    ["bSolar", "Solar"],
    ["bReset", "Reset"],
    ["bESPHome", "ESPHome"],
    ["bEid", "EID"],
    ["bNRGM", "NRGM"],
    ["bNETSW", "NETSW"],
    ["bInfo_SysInfo", "SysInfoTab"],
    ["bInfo_frontend", "FrontendTab"],
    ["bInfo_Telegram", "TelegramTab"],
    ["bInfo_Fields", "FieldsTab"],
    ["bInfo_APIdoc", "APIdocTab"]
  ].forEach(([btn, section]) => hideProxyTab(btn, section));

  const home = document.getElementById("Home");
  if (home) home.style.marginRight = "18px";

  const fsSection = document.getElementById("FSExplorer");
  if (fsSection) fsSection.remove();
}

async function loadSharedBody() {
  const response = await fetch(`${window.PROXY_CONTEXT.sharedBase}/DSMRindex_body.html`);
  if (!response.ok) throw new Error(`Unable to load DSMR body: ${response.status}`);
  return response.text();
}

async function loadSharedScripts() {
  const base = window.PROXY_CONTEXT.sharedBase;
  try {
    await loadScriptOnce(`${base}/chartjs-plugin-labels.min.js`);
  } catch (error) {
    if (!base.includes("@latest/")) {
      await loadScriptOnce("https://cdn.jsdelivr.net/gh/mhendriks/P1-Dongel-ESP32@latest/cdn/chartjs-plugin-labels.min.js");
    } else {
      throw error;
    }
  }

  await loadScriptOnce(`${base}/DSMRgraphics.min.js`);
  await loadScriptOnce(`${base}/DSMRindex.min.js`);
  await loadScriptOnce(`${base}/burnup.min.js`);
}

window.onload = async function proxyBootstrap() {
  try {
    await determineSharedBase();
    const bodyMarkup = await loadSharedBody();
    document.body.innerHTML = bodyMarkup + document.body.innerHTML;
    injectDrawer();
    if (window.PROXY_CONTEXT.deviceId) {
      await viewerOpen(window.PROXY_CONTEXT.deviceId);
    }
    await loadKnownDevices();
    await loadSharedScripts();
  } catch (error) {
    document.body.innerHTML = `<section style="padding:40px"><h2>Remote Access</h2><p>${error.message}</p></section>`;
    return;
  }

  applyProxyFeatureGate();
  bootsTrapMain();

  document.body.classList.add("proxy-drawer-open");
};

window.addEventListener("pagehide", viewerCloseSync);
window.addEventListener("beforeunload", viewerCloseSync);
