/*
***************************************************************************
**  Program  : burnup_legacy.js, part of DSMRloggerAPI
**  Purpose  : Legacy price-cap report
***************************************************************************
*/

const PRICECAP_MONTHS_SHORT = ["JAN", "FEB", "MRT", "APR", "MEI", "JUN", "JUL", "AUG", "SEP", "OKT", "NOV", "DEC"];
const PRICECAP_CEILING_ELECTRICITY = [339, 280, 267, 207, 181, 159, 161, 176, 199, 266, 306, 356];
const PRICECAP_CEILING_GAS = [221, 188, 159, 86, 35, 19, 17, 17, 24, 81, 146, 207];

let priceCapTimerRefresh = 0;
let priceCapCurrentChart = "MONTH";
let priceCapElectricityChart;
let priceCapGasChart;

function priceCapText(key, fallback) {
  const text = (typeof t === "function") ? t(key) : fallback;
  return (!text || text === key) ? fallback : text;
}

function priceCapCreateDataset(fill, color, label) {
  return {
    fill,
    borderColor: color,
    backgroundColor: color,
    data: [],
    label,
    lineTension: 0.2
  };
}

function priceCapAccumulateArray(values) {
  const accumulated = [];
  let total = 0;
  for (let i = 0; i < values.length; i++) {
    accumulated.push(total);
    if (isNaN(total)) total = 0;
    total += Number(values[i]) || 0;
  }
  accumulated.push(total);
  return accumulated;
}

function priceCapAccumulateEndArray(values) {
  const accumulated = [];
  let total = 0;

  for (let i = 0; i < values.length; i++) {
    const value = values[i];
    if (typeof value !== "number" || isNaN(value)) {
      accumulated.push(null);
      continue;
    }

    total += value;
    accumulated.push(total);
  }

  return accumulated;
}

function priceCapFormatArrayDecimals(data, decimals) {
  for (let i = 0; i < data.length; i++) {
    if (typeof data[i] === "number" && !isNaN(data[i])) data[i] = Number(data[i].toFixed(decimals));
  }
}

function priceCapExpandData(dataIn) {
  if (!dataIn || !Array.isArray(dataIn.data)) return { data: [], actSlot: 0 };
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
      data.data[index].p_gd = data.data[index].values[4] - data.data[slotBefore].values[4];
    } else {
      data.data[index].p_edt1 = data.data[index].values[0];
      data.data[index].p_edt2 = data.data[index].values[1];
      data.data[index].p_gd = data.data[index].values[4];
    }
    data.data[index].p_ed = data.data[index].p_edt1 + data.data[index].p_edt2;
  }

  return data;
}

function priceCapHistoryContainer(labels) {
  return {
    current: { name: labels[0], valid: false, data: [] },
    previous: { name: labels[1], valid: false, data: [] },
    preprevious: { name: labels[2], valid: false, data: [] }
  };
}

function priceCapCreateCharts() {
  const electricityCanvas = document.getElementById("priceCapElectricityChart");
  const gasCanvas = document.getElementById("priceCapGasChart");
  if (!electricityCanvas || !gasCanvas) return false;

  const commonOptions = function(title, unitLabel) {
    return {
      responsive: true,
      maintainAspectRatio: false,
      plugins: { labels: false },
      title: { display: true, text: title },
      legend: { position: "right" },
      scales: {
        xAxes: [{
          gridLines: { display: false },
          scaleLabel: { display: true, labelString: "Dagen" }
        }],
        yAxes: [{
          scaleLabel: { display: true, labelString: unitLabel },
          ticks: {
            beginAtZero: true,
            callback: function(value) {
              return Number(value).toLocaleString("nl-NL");
            }
          }
        }]
      }
    };
  };

  if (!priceCapElectricityChart) {
    priceCapElectricityChart = new Chart(electricityCanvas.getContext("2d"), {
      type: "line",
      data: { labels: [], datasets: [] },
      options: commonOptions("BURNUP ELEKTRA", "kWh")
    });
  }

  if (!priceCapGasChart) {
    priceCapGasChart = new Chart(gasCanvas.getContext("2d"), {
      type: "line",
      data: { labels: [], datasets: [] },
      options: commonOptions("BURNUP GAS", "m3")
    });
  }

  return true;
}

function priceCapBuildMonthHistory(data) {
  const labels = [priceCapText("pricecap-last31days", "Laatste 31 dagen"), "", ""];
  const electricity = priceCapHistoryContainer(labels);
  const gas = priceCapHistoryContainer(labels);
  const points = [];
  const start = data.data.length + data.actSlot;
  const stop = data.actSlot;

  for (let cursor = start; cursor > stop && points.length < 31; cursor--) {
    const index = cursor % data.data.length;
    const item = data.data[index];
    if (!item || !item.date || item.date === "20000000") continue;

    points.push({
      date: item.date,
      electricity: Number(item.p_ed) || 0,
      gas: Number(item.p_gd) || 0
    });
  }

  points.reverse();
  electricity.current.data = priceCapAccumulateArray(points.map(point => point.electricity));
  gas.current.data = priceCapAccumulateArray(points.map(point => point.gas));
  electricity.current.valid = points.length > 0;
  gas.current.valid = points.length > 0;

  return {
    histories: [electricity, gas],
    labels: [""].concat(points.map(point => `${parseInt(point.date.substring(4, 6), 10)}/${parseInt(point.date.substring(2, 4), 10)}`)),
    ceilingElectricity: priceCapAccumulateArray(points.map(point => priceCapDailyCeiling(point.date, PRICECAP_CEILING_ELECTRICITY))),
    ceilingGas: priceCapAccumulateArray(points.map(point => priceCapDailyCeiling(point.date, PRICECAP_CEILING_GAS)))
  };
}

function priceCapBuildYearHistory(data) {
  const now = new Date();
  const currentMonth = now.getMonth();
  const currentAbsMonth = (now.getFullYear() * 12) + currentMonth;
  const currentStartAbsMonth = currentAbsMonth - 11;
  const previousStartAbsMonth = currentAbsMonth - 23;
  const labels = [
    priceCapFormatWindowLabel(currentStartAbsMonth, currentAbsMonth),
    priceCapFormatWindowLabel(previousStartAbsMonth, currentStartAbsMonth - 1),
    ""
  ];
  const electricity = priceCapHistoryContainer(labels);
  const gas = priceCapHistoryContainer(labels);
  electricity.current.data = new Array(12).fill(null);
  electricity.previous.data = new Array(12).fill(null);
  electricity.preprevious.data = [];
  gas.current.data = new Array(12).fill(null);
  gas.previous.data = new Array(12).fill(null);
  gas.preprevious.data = [];

  data.data.forEach((item, index) => {
    if (!item || !item.date || item.date === "20000000") return;
    if (index === ((data.actSlot + 1) % data.data.length)) return;

    const year = 2000 + parseInt(item.date.substring(0, 2), 10);
    const month = parseInt(item.date.substring(2, 4), 10) - 1;
    if (month < 0 || month > 11) return;
    const absMonth = (year * 12) + month;

    let slot = -1;
    let targetElectricity = null;
    let targetGas = null;
    if (absMonth >= currentStartAbsMonth && absMonth <= currentAbsMonth) {
      slot = absMonth - currentStartAbsMonth;
      targetElectricity = electricity.current;
      targetGas = gas.current;
    } else if (absMonth >= previousStartAbsMonth && absMonth < currentStartAbsMonth) {
      slot = absMonth - previousStartAbsMonth;
      targetElectricity = electricity.previous;
      targetGas = gas.previous;
    }

    if (!targetElectricity || !targetGas || slot < 0 || slot > 11) return;
    targetElectricity.data[slot] = Number(item.p_ed) || 0;
    targetGas.data[slot] = Number(item.p_gd) || 0;
    targetElectricity.valid = true;
    targetGas.valid = true;
  });

  electricity.current.data = priceCapAccumulateEndArray(electricity.current.data);
  electricity.previous.data = priceCapAccumulateEndArray(electricity.previous.data);
  gas.current.data = priceCapAccumulateEndArray(gas.current.data);
  gas.previous.data = priceCapAccumulateEndArray(gas.previous.data);

  return {
    histories: [electricity, gas],
    labels: priceCapBuildRollingYearLabels(currentMonth),
    ceilingElectricity: priceCapAccumulateEndArray(priceCapBuildRollingYearCeiling(currentMonth, PRICECAP_CEILING_ELECTRICITY)),
    ceilingGas: priceCapAccumulateEndArray(priceCapBuildRollingYearCeiling(currentMonth, PRICECAP_CEILING_GAS))
  };
}

function priceCapDailyCeiling(date, ceilingValues) {
  const year = 2000 + parseInt(date.substring(0, 2), 10);
  const month = parseInt(date.substring(2, 4), 10) - 1;
  if (month < 0 || month >= ceilingValues.length) return 0;
  const dayCount = new Date(year, month + 1, 0).getDate();
  return ceilingValues[month] / dayCount;
}

function priceCapBuildRollingYearLabels(currentMonth) {
  const labels = [];
  const startMonth = (currentMonth + 1) % 12;

  for (let offset = 0; offset < 12; offset++) {
    labels.push(PRICECAP_MONTHS_SHORT[(startMonth + offset) % 12]);
  }

  return labels;
}

function priceCapBuildRollingYearCeiling(currentMonth, ceilingValues) {
  const values = [];
  const startMonth = (currentMonth + 1) % 12;

  for (let offset = 0; offset < 12; offset++) {
    values.push(ceilingValues[(startMonth + offset) % 12]);
  }

  return values;
}

function priceCapFormatWindowLabel(startAbsMonth, endAbsMonth) {
  const startYear = Math.floor(startAbsMonth / 12);
  const startMonth = startAbsMonth % 12;
  const endYear = Math.floor(endAbsMonth / 12);
  const endMonth = endAbsMonth % 12;

  return `${PRICECAP_MONTHS_SHORT[startMonth]} '${String(startYear).slice(-2)} - ${PRICECAP_MONTHS_SHORT[endMonth]} '${String(endYear).slice(-2)}`;
}

function priceCapBuildChartData(labels, history, ceilingValues, yearView, rollingCeilingData) {
  const current = priceCapCreateDataset(false, "rgba(0, 0, 139, 1)", history.current.name);
  const previous = priceCapCreateDataset(false, "rgba(0, 0, 139, .25)", history.previous.name);
  const preprevious = priceCapCreateDataset(false, "rgba(0, 0, 139, .0625)", history.preprevious.name);
  const plafond = priceCapCreateDataset("start", "red", priceCapText("mnu-pricecap", "Plafond"));
  const ceiling = rollingCeilingData || priceCapAccumulateArray(ceilingValues);

  if (!yearView) {
    plafond.spanGaps = false;
  }

  current.data = history.current.data;
  previous.data = history.previous.data;
  preprevious.data = history.preprevious.data;
  plafond.data = ceiling;
  plafond.backgroundColor = "rgba(0, 255, 0, .125)";
  if (yearView) plafond.spanGaps = true;
  previous.hidden = !yearView;
  preprevious.hidden = true;

  priceCapFormatArrayDecimals(current.data, 3);
  priceCapFormatArrayDecimals(previous.data, 3);
  priceCapFormatArrayDecimals(preprevious.data, 3);

  const chartData = { labels, datasets: [current] };
  if (history.previous.valid) chartData.datasets.push(previous);
  if (yearView && history.preprevious.valid) chartData.datasets.push(preprevious);
  chartData.datasets.push(plafond);
  return chartData;
}

function priceCapRenderData(json) {
  if (!priceCapCreateCharts()) return;

  const data = priceCapExpandData(json);
  const yearView = priceCapCurrentChart === "YEAR";
  const monthView = yearView ? null : priceCapBuildMonthHistory(data);
  const yearData = yearView ? priceCapBuildYearHistory(data) : null;
  const labels = yearView ? yearData.labels : monthView.labels;
  const histories = yearView ? yearData.histories : monthView.histories;
  const axisLabel = yearView ? "Maanden" : priceCapText("pricecap-last31days", "Laatste 31 dagen");

  priceCapElectricityChart.options.scales.xAxes[0].scaleLabel.labelString = axisLabel;
  priceCapGasChart.options.scales.xAxes[0].scaleLabel.labelString = axisLabel;
  priceCapElectricityChart.data = priceCapBuildChartData(
    labels,
    histories[0],
    PRICECAP_CEILING_ELECTRICITY,
    yearView,
    yearView ? yearData.ceilingElectricity : monthView.ceilingElectricity
  );
  priceCapGasChart.data = priceCapBuildChartData(
    labels,
    histories[1],
    PRICECAP_CEILING_GAS,
    yearView,
    yearView ? yearData.ceilingGas : monthView.ceilingGas
  );
  priceCapElectricityChart.update();
  priceCapGasChart.update();
}

function refreshPriceCapData() {
  clearInterval(priceCapTimerRefresh);
  priceCapTimerRefresh = setInterval(refreshPriceCapData, 30 * 1000);

  const historyUrl = (priceCapCurrentChart === "YEAR") ? "/RNGmonths.json" : "/RNGdays.json";
  fetch(historyUrl)
    .then(response => response.json())
    .then(priceCapRenderData)
    .catch(error => console.log("refreshPriceCapData: " + error));
}

function setPriceCapViewMONTH() {
  priceCapCurrentChart = "MONTH";
  refreshPriceCapData();
}

function setPriceCapViewYEAR() {
  priceCapCurrentChart = "YEAR";
  refreshPriceCapData();
}
