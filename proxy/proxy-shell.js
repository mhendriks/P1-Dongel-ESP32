/*
***************************************************************************
**  Proxy shell for shared DSMR frontend
***************************************************************************
*/

window.PROXY_CONTEXT = {
  deviceId: new URLSearchParams(window.location.search).get("device_id") || "",
  sharedBase: "/~martijn/dsmr-api/v5",
  visibleTabs: ["bDashTab", "bInsightsTab", "bDaysTab"]
};

const proxySidebarRefs = {
  toggle: document.getElementById("proxy-sidebar-toggle"),
  form: document.getElementById("proxy-device-form"),
  input: document.getElementById("proxy-device-id"),
  status: document.getElementById("proxy-status-pill"),
  device: document.getElementById("proxy-meta-device"),
  hostname: document.getElementById("proxy-meta-hostname"),
  firmware: document.getElementById("proxy-meta-firmware"),
  hardware: document.getElementById("proxy-meta-hardware"),
  updated: document.getElementById("proxy-meta-updated"),
  links: Array.from(document.querySelectorAll(".proxy-links a"))
};

proxySidebarRefs.input.value = window.PROXY_CONTEXT.deviceId;

proxySidebarRefs.toggle.addEventListener("click", () => {
  if (window.innerWidth <= 980) {
    document.body.classList.toggle("proxy-sidebar-open");
  } else {
    document.body.classList.toggle("proxy-collapsed");
  }
});

proxySidebarRefs.form.addEventListener("submit", (event) => {
  event.preventDefault();
  const deviceId = proxySidebarRefs.input.value.trim();
  if (!deviceId) return;

  const nextUrl = new URL(window.location.href);
  nextUrl.searchParams.set("device_id", deviceId);
  window.location.href = nextUrl.toString();
});

proxySidebarRefs.links.forEach((link) => {
  link.addEventListener("click", () => {
    proxySidebarRefs.links.forEach((item) => item.classList.remove("active"));
    link.classList.add("active");
    const btnId = link.dataset.proxyTab;
    const btn = document.getElementById(btnId);
    if (btn) btn.click();
    if (window.innerWidth <= 980) document.body.classList.remove("proxy-sidebar-open");
  });
});

window.proxyShellUpdateMeta = function proxyShellUpdateMeta(summary) {
  proxySidebarRefs.status.textContent = summary.online ? "Online" : "Offline";
  proxySidebarRefs.status.classList.toggle("offline", !summary.online);
  proxySidebarRefs.device.textContent = summary.device_id || "-";
  proxySidebarRefs.hostname.textContent = summary.hostname || "-";
  proxySidebarRefs.firmware.textContent = summary.fw_version || "-";
  proxySidebarRefs.hardware.textContent = summary.hardware || "-";
  proxySidebarRefs.updated.textContent = summary.timestamp || "-";
};

FrontendConfig = function proxyFrontendConfig() {
  AMPS = 35;
  ShowVoltage = true;
  UseCDN = false;
  Injection = true;
  Phases = 3;
  HeeftGas = true;
  HeeftWater = true;
  EnableHist = true;
  Act_Watt = false;
  AvoidSpikes = false;
  IgnoreInjection = false;
  IgnoreGas = false;
  SettingsRead = true;
};

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

function syncSidebarActiveState() {
  proxySidebarRefs.links.forEach((link) => {
    link.classList.toggle("active", link.getAttribute("href") === window.location.hash);
  });
}

async function loadSharedBody() {
  const response = await fetch(`${window.PROXY_CONTEXT.sharedBase}/DSMRindex_body.html`);
  if (!response.ok) throw new Error(`Unable to load DSMR body: ${response.status}`);
  return response.text();
}

window.onload = async function proxyBootstrap() {
  const target = document.getElementById("proxy-app");
  try {
    target.innerHTML = await loadSharedBody();
  } catch (error) {
    target.innerHTML = `<section style="padding:40px"><h2>Shared frontend niet beschikbaar</h2><p>${error.message}</p></section>`;
    return;
  }

  applyProxyFeatureGate();
  bootsTrapMain();
  syncSidebarActiveState();

  if (window.PROXY_CONTEXT.deviceId) {
    proxySidebarRefs.links[0]?.classList.add("active");
  }
};

window.addEventListener("hashchange", syncSidebarActiveState);
