/*
***************************************************************************
**  Program  : impact2027.js, part of DSMRloggerAPI
**  Purpose  : Post-saldering impact view
***************************************************************************
*/

const IMPACT_IMPORT_TARIFF_FALLBACK = 0.30;
const IMPACT_FEEDIN_TARIFF_FALLBACK = 0.00;
const IMPACT_STORAGE_IMPORT_KEY = "impact2027.importTariff";
const IMPACT_STORAGE_EXPORT_KEY = "impact2027.exportTariff";
const listMONTHS_SHORT = ["JAN", "FEB", "MRT", "APR", "MEI", "JUN", "JUL", "AUG", "SEP", "OKT", "NOV", "DEC"];

let timerRefresh = 0;
let timerRetryRender = 0;
let sCurrentChart = "YEAR";
let fLoadedBurnup = false;
let impactFlowChart;
let impactValueChart;

function impactText(key, fallback, replacements = {}) {
  let text = (typeof t === "function") ? t(key) : fallback;
  if (!text || text === key) text = fallback;
  Object.keys(replacements).forEach((name) => {
    text = text.replace(new RegExp(`\\{${name}\\}`, "g"), replacements[name]);
  });
  return text;
}

function impactApplyCopy() {
  const menuLabel = impactText("mnu-plafond", "Na Salderen");
  const titleLabel = impactText("tle-cap", "Na Salderen");
  const description = impactText(
    "tle-cap-description",
    "Vanaf 1 januari 2027 stopt voor Nederlandse huishoudens de salderingsregeling. Je krijgt dan minimaal 50% van de kale energieprijs als vergoeding voor teruglevering, terwijl energieleveranciers mogelijk ook terugleverkosten rekenen."
  );

  const tabButton = document.getElementById("bPlafondTab");
  if (tabButton) tabButton.textContent = menuLabel;

  const tabSection = document.getElementById("PlafondTab");
  if (!tabSection) return;

  const title = tabSection.querySelector("h2");
  if (title) title.textContent = titleLabel;

  const bodyText = tabSection.querySelector("p");
  if (bodyText) bodyText.innerHTML = description;
}

function impactFormatNumber(value, decimals) {
  if (isNaN(value)) return "-";
  return Number(value).toLocaleString("nl-NL", {
    minimumFractionDigits: decimals,
    maximumFractionDigits: decimals
  });
}

function impactFormatKwh(value) {
  return `${impactFormatNumber(value, 1)} kWh`;
}

function impactFormatEuro(value) {
  return `EUR ${impactFormatNumber(value, 2)}`;
}

function impactAverage(values, fallback) {
  const valid = values.filter(v => !isNaN(v) && v > 0);
  if (!valid.length) return fallback;
  return valid.reduce((sum, value) => sum + value, 0) / valid.length;
}

function impactStorageAvailable() {
  return typeof window !== "undefined" && typeof window.localStorage !== "undefined";
}

function impactReadStoredNumber(key) {
  if (!impactStorageAvailable()) return NaN;
  const raw = window.localStorage.getItem(key);
  if (raw === null) return NaN;
  const parsed = Number(String(raw).replace(",", "."));
  return Number.isFinite(parsed) ? parsed : NaN;
}

function impactWriteStoredNumber(key, value) {
  if (!impactStorageAvailable()) return;
  window.localStorage.setItem(key, String(value));
}

function impactImportTariff() {
  const stored = impactReadStoredNumber(IMPACT_STORAGE_IMPORT_KEY);
  if (Number.isFinite(stored) && stored >= 0) return stored;
  return impactAverage([Number(ed_tariff1), Number(ed_tariff2)], IMPACT_IMPORT_TARIFF_FALLBACK);
}

function impactFeedinTariff() {
  const stored = impactReadStoredNumber(IMPACT_STORAGE_EXPORT_KEY);
  if (Number.isFinite(stored) && stored >= 0) return stored;
  return impactAverage([Number(er_tariff1), Number(er_tariff2)], IMPACT_FEEDIN_TARIFF_FALLBACK);
}

function impactHasSolar(pointList) {
  return pointList.some(point => point.solarKwh !== null);
}

function impactFormatInputValue(value) {
  return Number(value).toFixed(3).replace(".", ",");
}

function impactBindTariffInputs() {
  const importInput = document.getElementById("impactImportTariffInput");
  const exportInput = document.getElementById("impactExportTariffInput");
  if (!importInput || !exportInput || importInput.dataset.bound === "true") return;

  importInput.value = impactFormatInputValue(impactImportTariff());
  exportInput.value = impactFormatInputValue(impactFeedinTariff());

  const persistImportValue = function() {
    const value = Number(String(importInput.value).replace(",", "."));
    if (!Number.isFinite(value) || value < 0) {
      importInput.value = impactFormatInputValue(impactImportTariff());
      return;
    }
    impactWriteStoredNumber(IMPACT_STORAGE_IMPORT_KEY, value);
    importInput.value = impactFormatInputValue(value);
    renderImpact2027();
  };

  const persistExportValue = function() {
    const value = Number(String(exportInput.value).replace(",", "."));
    if (!Number.isFinite(value) || value < 0) {
      exportInput.value = impactFormatInputValue(impactFeedinTariff());
      return;
    }
    impactWriteStoredNumber(IMPACT_STORAGE_EXPORT_KEY, value);
    exportInput.value = impactFormatInputValue(value);
    renderImpact2027();
  };

  importInput.addEventListener("change", persistImportValue);
  exportInput.addEventListener("change", persistExportValue);
  importInput.addEventListener("blur", persistImportValue);
  exportInput.addEventListener("blur", persistExportValue);
  importInput.addEventListener("keydown", function(event) {
    if (event.key === "Enter") persistImportValue();
  });
  exportInput.addEventListener("keydown", function(event) {
    if (event.key === "Enter") persistExportValue();
  });

  importInput.dataset.bound = "true";
  exportInput.dataset.bound = "true";
}

function BurnupBootstrap() {
  impactApplyCopy();
  impactBindTariffInputs();

  const flowCanvas = document.getElementById("impactFlowChart");
  const valueCanvas = document.getElementById("impactValueChart");
  if (!flowCanvas || !valueCanvas) return;

  if (!impactFlowChart) {
    impactFlowChart = new Chart(flowCanvas.getContext("2d"), {
      type: "bar",
      data: { labels: [], datasets: [] },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        plugins: { labels: false },
        legend: {
          position: "bottom",
          labels: {
            boxWidth: 14,
            fontColor: "#34435f"
          }
        },
        tooltips: {
          mode: "index",
          intersect: false
        },
        scales: {
          xAxes: [{
            gridLines: { display: false },
            ticks: { fontColor: "#42526e" }
          }],
          yAxes: [{
            ticks: {
              callback: function(value) {
                return `${value} kWh`;
              },
              fontColor: "#42526e"
            },
            gridLines: {
              color: "rgba(28, 58, 99, 0.08)"
            },
            scaleLabel: {
              display: true,
              labelString: "kWh"
            }
          }]
        }
      }
    });
  }

  if (!impactValueChart) {
    impactValueChart = new Chart(valueCanvas.getContext("2d"), {
      type: "bar",
      data: { labels: [], datasets: [] },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        plugins: { labels: false },
        legend: {
          position: "bottom",
          labels: {
            boxWidth: 14,
            fontColor: "#34435f"
          }
        },
        tooltips: {
          mode: "index",
          intersect: false
        },
        scales: {
          xAxes: [{
            stacked: false,
            gridLines: { display: false },
            ticks: { fontColor: "#42526e" }
          }],
          yAxes: [{
            ticks: {
              callback: function(value) {
                return `EUR ${impactFormatNumber(value, 0)}`;
              },
              fontColor: "#42526e"
            },
            gridLines: {
              color: "rgba(28, 58, 99, 0.08)"
            },
            scaleLabel: {
              display: true,
              labelString: "EUR"
            }
          }]
        }
      }
    });
  }

  fLoadedBurnup = true;
}

function ensureBurnupLoaded() {
  if (!fLoadedBurnup) BurnupBootstrap();
}

function setViewMONTH() {
  ensureBurnupLoaded();
  sCurrentChart = "MONTH";
  refreshData();
}

function setViewYEAR() {
  ensureBurnupLoaded();
  sCurrentChart = "YEAR";
  refreshData();
}

function refreshData() {
  ensureBurnupLoaded();

  clearInterval(timerRefresh);
  clearTimeout(timerRetryRender);
  timerRefresh = setInterval(function() {
    renderImpact2027();
  }, 30 * 1000);

  renderImpact2027();
}

function impactExpandData(dataIn) {
  if (!dataIn || !Array.isArray(dataIn.data)) {
    return { data: [], actSlot: 0 };
  }

  const data = structuredClone(dataIn);

  for (let cursor = data.data.length + data.actSlot; cursor > data.actSlot; cursor--) {
    const index = cursor % data.data.length;
    const slotBefore = math.mod(index - 1, data.data.length);

    if (cursor !== data.actSlot) {
      if (data.data[slotBefore].values[0] === 0) {
        data.data[slotBefore].values = data.data[index].values;
      }

      data.data[index].p_edt1 = data.data[index].values[0] - data.data[slotBefore].values[0];
      data.data[index].p_edt2 = data.data[index].values[1] - data.data[slotBefore].values[1];
      data.data[index].p_ert1 = data.data[index].values[2] - data.data[slotBefore].values[2];
      data.data[index].p_ert2 = data.data[index].values[3] - data.data[slotBefore].values[3];
      data.data[index].p_gd = data.data[index].values[4] - data.data[slotBefore].values[4];
      data.data[index].water = data.data[index].values[5] - data.data[slotBefore].values[5];
    } else {
      data.data[index].p_edt1 = data.data[index].values[0];
      data.data[index].p_edt2 = data.data[index].values[1];
      data.data[index].p_ert1 = data.data[index].values[2];
      data.data[index].p_ert2 = data.data[index].values[3];
      data.data[index].p_gd = data.data[index].values[4];
      data.data[index].water = data.data[index].values[5];
    }

    data.data[index].p_ed = data.data[index].p_edt1 + data.data[index].p_edt2;
    data.data[index].p_er = data.data[index].p_ert1 + data.data[index].p_ert2;
    data.data[index].solar = (data.data[index].values.length > 6 && !isNaN(Number(data.data[index].values[6])))
      ? Number(data.data[index].values[6])
      : null;
  }

  return data;
}

function impactLabel(type, date) {
  if (type === "YEAR") {
    const monthIndex = Math.max(0, parseInt(date.substring(2, 4), 10) - 1);
    const yearText = `20${date.substring(0, 2)}`;
    return `${listMONTHS_SHORT[monthIndex]} '${yearText.substring(2)}`;
  }

  return `${date.substring(4, 6)}-${date.substring(2, 4)}`;
}

function impactDeltaPerKwh() {
  return Math.max(0, impactImportTariff() - impactFeedinTariff());
}

function impactCollectPoints(data, type) {
  const points = [];
  const start = data.data.length + data.actSlot;
  const stop = (type === "YEAR") ? start - 12 : data.actSlot + 1;
  const importTariff = impactImportTariff();
  const feedinTariff = impactFeedinTariff();

  for (let cursor = start; cursor > stop; cursor--) {
    const index = cursor % data.data.length;
    const item = data.data[index];
    if (!item || !item.date || item.date === "20000000") continue;

    const importKwh = Math.max(0, Number(item.p_ed) || 0);
    const exportKwh = Math.max(0, Number(item.p_er) || 0);
    const solarKwh = (item.solar !== null && item.solar >= 0) ? Number(item.solar) : null;
    const selfUseKwh = (solarKwh !== null) ? Math.max(0, solarKwh - exportKwh) : null;
    const salderedKwh = Math.min(importKwh, exportKwh);

    points.push({
      label: impactLabel(type, item.date),
      importKwh,
      exportKwh,
      solarKwh,
      selfUseKwh,
      salderedKwh,
      before2027: 0,
      after2027: 0,
      delta2027: 0,
      effectiveSaldered: 0
    });
  }

  const orderedPoints = points.reverse();
  const totalImport = orderedPoints.reduce((sum, point) => sum + point.importKwh, 0);
  const totalExport = orderedPoints.reduce((sum, point) => sum + point.exportKwh, 0);
  const cappedExport = Math.min(totalImport, totalExport);
  const exportRatio = (totalExport > 0) ? Math.min(1, cappedExport / totalExport) : 0;
  let runningImport = 0;
  let runningExport = 0;

  orderedPoints.forEach(point => {
    const effectiveExport = point.exportKwh * exportRatio;
    point.effectiveSaldered = effectiveExport;
    point.delta2027 = impactDeltaPerKwh() * effectiveExport;

    if (type === "YEAR") {
      runningImport += point.importKwh;
      runningExport += effectiveExport;
      point.before2027 = Math.max(0, runningImport - runningExport) * importTariff;
      point.after2027 = point.before2027 + point.delta2027;
    } else {
      point.before2027 = Math.max(0, point.importKwh - effectiveExport) * importTariff;
      point.after2027 = point.before2027 + point.delta2027;
    }
  });

  return orderedPoints;
}

function impactUpdateCards(points) {
  const hasSolar = impactHasSolar(points);
  const totalImport = points.reduce((sum, point) => sum + point.importKwh, 0);
  const totalExport = points.reduce((sum, point) => sum + point.exportKwh, 0);
  const totalSolar = points.reduce((sum, point) => sum + (point.solarKwh || 0), 0);
  const totalSelfUse = points.reduce((sum, point) => sum + (point.selfUseKwh || 0), 0);
  const importTariff = impactImportTariff();
  const feedinTariff = impactFeedinTariff();
  const cappedExport = Math.min(totalImport, totalExport);
  const totalBefore2027 = Math.max(0, totalImport - cappedExport) * importTariff;
  const totalDelta = impactDeltaPerKwh() * cappedExport;
  const totalAfter2027 = totalBefore2027 + totalDelta;
  const saldered = Math.min(totalImport, totalExport);

  document.getElementById("impactImportValue").textContent = impactFormatKwh(totalImport);
  document.getElementById("impactExportValue").textContent = impactFormatKwh(totalExport);
  document.getElementById("impactDeltaValue").textContent = impactFormatEuro(totalDelta);
  document.getElementById("impactTariffNote").innerHTML = impactText(
    "impact-tariff-note",
    "Reken met <b>{importTariff} EUR/kWh</b> afname en <b>{feedinTariff} EUR/kWh</b> netto opbrengst per teruggeleverde kWh. De browser bewaart jouw invoer lokaal.",
    {
      importTariff: impactFormatNumber(importTariff, 3),
      feedinTariff: impactFormatNumber(feedinTariff, 3)
    }
  );

  const solarCard = document.getElementById("impactSolarCard");
  const selfUseCard = document.getElementById("impactSelfUseCard");
  solarCard.style.display = hasSolar ? "block" : "none";
  selfUseCard.style.display = hasSolar ? "block" : "none";

  if (totalExport <= 0) {
    document.getElementById("impactViewNote").innerHTML = impactText(
      "impact-note-no-export",
      "Er is in deze selectie geen teruglevering gevonden. De extra kosten door het stoppen van salderen blijven dan <b>{totalDelta}</b>.",
      { totalDelta: impactFormatEuro(totalDelta) }
    );
  } else if (hasSolar) {
    document.getElementById("impactSolarValue").textContent = impactFormatKwh(totalSolar);
    document.getElementById("impactSelfUseValue").textContent = impactFormatKwh(totalSelfUse);
    document.getElementById("impactViewNote").innerHTML = (sCurrentChart === "YEAR")
      ? impactText(
          "impact-note-year-solar",
          "Met productiedata: voor de extra kosten tellen we alleen het deel van de teruglevering mee dat vroeger nog gesaldeerd kon worden. Zomeroverschot kan dus winterverbruik compenseren. In deze selectie valt circa <b>{saldered}</b> onder salderen.",
          { saldered: impactFormatKwh(saldered) }
        )
      : impactText(
          "impact-note-month-solar",
          "Met productiedata: direct eigen verbruik wordt benaderd als productie min teruglevering. Voor de extra kosten tellen we alleen het deel van de teruglevering mee dat vroeger nog gesaldeerd kon worden."
        );
  } else {
    document.getElementById("impactViewNote").innerHTML = (sCurrentChart === "YEAR")
      ? impactText(
          "impact-note-year-nosolar",
          "Zonder productiedata: voor de extra kosten tellen we alleen het deel van de teruglevering mee dat vroeger nog gesaldeerd kon worden. Zomeroverschot kan dus winterverbruik compenseren. In deze selectie valt circa <b>{saldered}</b> onder salderen.",
          { saldered: impactFormatKwh(saldered) }
        )
      : impactText(
          "impact-note-month-nosolar",
          "Zonder productiedata: deze vergelijking gebruikt alleen afname en teruglevering. Voor de extra kosten tellen we alleen het deel van de teruglevering mee dat vroeger nog gesaldeerd kon worden."
        );
  }
}

function impactBuildFlowChart(points) {
  const hasSolar = impactHasSolar(points);

  impactFlowChart.data = {
    labels: points.map(point => point.label),
    datasets: [
      {
        type: "bar",
        label: impactText("impact-import", "Afname"),
        backgroundColor: "rgba(42, 109, 244, 0.78)",
        borderColor: "rgba(42, 109, 244, 1)",
        borderWidth: 1,
        data: points.map(point => Number(point.importKwh.toFixed(2)))
      },
      {
        type: "bar",
        label: impactText("impact-export", "Teruglevering"),
        backgroundColor: "rgba(29, 181, 114, 0.78)",
        borderColor: "rgba(29, 181, 114, 1)",
        borderWidth: 1,
        data: points.map(point => Number((-point.exportKwh).toFixed(2)))
      }
    ]
  };

  if (hasSolar) {
    impactFlowChart.data.datasets.push({
      type: "line",
      label: impactText("impact-production", "Productie"),
      borderColor: "rgba(247, 182, 56, 1)",
      backgroundColor: "rgba(247, 182, 56, 0.12)",
      borderWidth: 3,
      pointRadius: 3,
      pointBackgroundColor: "#f7b638",
      fill: false,
      lineTension: 0.28,
      data: points.map(point => point.solarKwh !== null ? Number(point.solarKwh.toFixed(2)) : null)
    });
    impactFlowChart.data.datasets.push({
      type: "line",
      label: impactText("impact-selfuse", "Direct verbruikt"),
      borderColor: "rgba(175, 132, 17, 1)",
      backgroundColor: "rgba(175, 132, 17, 0.10)",
      borderDash: [6, 4],
      borderWidth: 2,
      pointRadius: 2,
      fill: false,
      lineTension: 0.22,
      data: points.map(point => point.selfUseKwh !== null ? Number(point.selfUseKwh.toFixed(2)) : null)
    });
  }

  impactFlowChart.update();
}

function impactBuildValueChart(points) {
  impactValueChart.data = {
    labels: points.map(point => point.label),
    datasets: [
      {
        type: "bar",
        label: impactText("impact-sim-title", "Simulatie 2027"),
        backgroundColor: "rgba(209, 86, 86, 0.82)",
        borderColor: "rgba(209, 86, 86, 1)",
        borderWidth: 1,
        data: points.map(point => Number(point.delta2027.toFixed(2)))
      }
    ]
  };

  impactValueChart.update();
}

function renderImpact2027() {
  let raw = (sCurrentChart === "YEAR") ? objDAL.getMonths() : objDAL.getDays();
  if (!raw || !Array.isArray(raw.data) || raw.data.length === 0) {
    clearTimeout(timerRetryRender);
    timerRetryRender = setTimeout(function() {
      renderImpact2027();
    }, 1000);
    return;
  }

  const data = impactExpandData(raw);
  const points = impactCollectPoints(data, sCurrentChart);
  if (!points.length) {
    clearTimeout(timerRetryRender);
    timerRetryRender = setTimeout(function() {
      renderImpact2027();
    }, 1000);
    return;
  }

  clearTimeout(timerRetryRender);

  impactUpdateCards(points);
  impactBuildFlowChart(points);
  impactBuildValueChart(points);
}
