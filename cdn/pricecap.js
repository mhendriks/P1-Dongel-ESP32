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

function priceCapFormatArrayDecimals(data, decimals) {
  for (let i = 0; i < data.length; i++) {
    if (!isNaN(data[i])) data[i] = Number(data[i].toFixed(decimals));
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
  const now = new Date();
  const currentMonth = now.getMonth();
  const previousMonth = currentMonth === 0 ? 11 : currentMonth - 1;
  const labels = [PRICECAP_MONTHS_SHORT[currentMonth], PRICECAP_MONTHS_SHORT[previousMonth], ""];
  const electricity = priceCapHistoryContainer(labels);
  const gas = priceCapHistoryContainer(labels);
  electricity.current.data = new Array(31);
  electricity.previous.data = new Array(31);
  gas.current.data = new Array(31);
  gas.previous.data = new Array(31);

  data.data.forEach((item, index) => {
    if (!item || !item.date || item.date === "20000000") return;
    if (index === ((data.actSlot + 1) % data.data.length)) return;

    const month = parseInt(item.date.substring(2, 4), 10) - 1;
    const day = parseInt(item.date.substring(4, 6), 10) - 1;
    if (day < 0 || day > 30) return;

    if (month === currentMonth) {
      electricity.current.data[day] = Number(item.p_ed) || 0;
      gas.current.data[day] = Number(item.p_gd) || 0;
      electricity.current.valid = true;
      gas.current.valid = true;
    } else if (month === previousMonth) {
      electricity.previous.data[day] = Number(item.p_ed) || 0;
      gas.previous.data[day] = Number(item.p_gd) || 0;
      electricity.previous.valid = true;
      gas.previous.valid = true;
    }
  });

  electricity.current.data = priceCapAccumulateArray(electricity.current.data);
  electricity.previous.data = priceCapAccumulateArray(electricity.previous.data);
  gas.current.data = priceCapAccumulateArray(gas.current.data);
  gas.previous.data = priceCapAccumulateArray(gas.previous.data);
  return [electricity, gas];
}

function priceCapBuildYearHistory(data) {
  const currentYear = new Date().getFullYear();
  const currentYY = currentYear - 2000;
  const labels = [currentYear, currentYear - 1, currentYear - 2];
  const electricity = priceCapHistoryContainer(labels);
  const gas = priceCapHistoryContainer(labels);
  electricity.current.data = new Array(12);
  electricity.previous.data = new Array(12);
  electricity.preprevious.data = new Array(12);
  gas.current.data = new Array(12);
  gas.previous.data = new Array(12);
  gas.preprevious.data = new Array(12);

  data.data.forEach((item, index) => {
    if (!item || !item.date || item.date === "20000000") return;
    if (index === ((data.actSlot + 1) % data.data.length)) return;

    const year = parseInt(item.date.substring(0, 2), 10);
    const month = parseInt(item.date.substring(2, 4), 10) - 1;
    if (month < 0 || month > 11) return;

    let targetElectricity;
    let targetGas;
    if (year === currentYY) {
      targetElectricity = electricity.current;
      targetGas = gas.current;
    } else if (year === currentYY - 1) {
      targetElectricity = electricity.previous;
      targetGas = gas.previous;
    } else if (year === currentYY - 2) {
      targetElectricity = electricity.preprevious;
      targetGas = gas.preprevious;
    }

    if (!targetElectricity || !targetGas) return;
    targetElectricity.data[month] = Number(item.p_ed) || 0;
    targetGas.data[month] = Number(item.p_gd) || 0;
    targetElectricity.valid = true;
    targetGas.valid = true;
  });

  electricity.current.data = priceCapAccumulateArray(electricity.current.data);
  electricity.previous.data = priceCapAccumulateArray(electricity.previous.data);
  electricity.preprevious.data = priceCapAccumulateArray(electricity.preprevious.data);
  gas.current.data = priceCapAccumulateArray(gas.current.data);
  gas.previous.data = priceCapAccumulateArray(gas.previous.data);
  gas.preprevious.data = priceCapAccumulateArray(gas.preprevious.data);
  return [electricity, gas];
}

function priceCapBuildChartData(labels, history, ceilingValues, yearView) {
  const current = priceCapCreateDataset(false, "rgba(0, 0, 139, 1)", history.current.name);
  const previous = priceCapCreateDataset(false, "rgba(0, 0, 139, .25)", history.previous.name);
  const preprevious = priceCapCreateDataset(false, "rgba(0, 0, 139, .0625)", history.preprevious.name);
  const plafond = priceCapCreateDataset("start", "red", priceCapText("mnu-pricecap", "Plafond"));
  const ceiling = yearView ? priceCapAccumulateArray(ceilingValues) : new Array(32).fill(null);

  if (!yearView) {
    const month = new Date().getMonth();
    const year = new Date().getFullYear();
    const dayCount = new Date(year, month + 1, 0).getDate();
    ceiling[0] = 0;
    ceiling[dayCount] = ceilingValues[month];
  }

  current.data = history.current.data;
  previous.data = history.previous.data;
  preprevious.data = history.preprevious.data;
  plafond.data = ceiling;
  plafond.backgroundColor = "rgba(0, 255, 0, .125)";
  plafond.spanGaps = true;
  previous.hidden = true;
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
  const labels = yearView
    ? ["0"].concat(PRICECAP_MONTHS_SHORT)
    : Array.from({ length: 32 }, (_, index) => String(index));
  const histories = yearView ? priceCapBuildYearHistory(data) : priceCapBuildMonthHistory(data);
  const axisLabel = yearView ? "Maanden" : "Dagen";

  priceCapElectricityChart.options.scales.xAxes[0].scaleLabel.labelString = axisLabel;
  priceCapGasChart.options.scales.xAxes[0].scaleLabel.labelString = axisLabel;
  priceCapElectricityChart.data = priceCapBuildChartData(labels, histories[0], PRICECAP_CEILING_ELECTRICITY, yearView);
  priceCapGasChart.data = priceCapBuildChartData(labels, histories[1], PRICECAP_CEILING_GAS, yearView);
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
