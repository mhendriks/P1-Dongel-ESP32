/*
***************************************************************************  
**  Copyright 2025 Smartstuff
**  Authors  : Martijn Hendriks / Mar10us
**  TERMS OF USE: MIT License.                                      
***************************************************************************      
*/

const SQUARE_M_CUBED 	   = "\u33A5";

"use strict";

//   let ota_url 				= "";
  let activeTab             = "bDashTab";
  let PauseAPI				= false; //pause api call when browser is inactive
  let presentationType      = "TAB";
  let tabTimer              = 0;
//   let actualTimer           = 0;
  let devVersion			= 0;
  let firmwareVersion       = 0;
  let firmwareVersion_dspl  = "-";
  let NRGStatusTimer		= 0;
  
  let ed_tariff1            = 0;
  let ed_tariff2            = 0;
  let er_tariff1            = 0;
  let er_tariff2            = 0;
  let gd_tariff             = 0;
  let electr_netw_costs     = 0;
  let gas_netw_costs        = 0;
  let hostName            	= "-";  
  let data		 			= [];
  let DayEpoch				= 0;
  let monthType        		= "ED";
  let settingFontColor 		= 'white';
  let SolarActive 			= false;
  let AccuActive			= false;
  let Pi_today				= 0;
  let Pd_today				= 0;
  let conf_netswitch 		= false;
  let update_reconnected	= false;
                   
  const spinner 			= document.getElementById("loader");

//---- frontend settings
  let AMPS					= 35 	//waarde zekering (meestal 35 of 25 ampere)
  let ShowVoltage			= true 	//toon spannningsverloop op dashboard
  let UseCDN				= true	//ophalen van hulpbestanden lokaal of via cdn //nog niet geimplementeerd
  let Injection				= false	//teruglevering energie false = bepaalt door de data uit slimme meter, true = altijd aan
  let Phases				= 1		//aantal fases (1,2,3) voor de berekening van de totale maximale stroom Imax = fases * AMPS
  let HeeftGas				= false	//gasmeter aanwezig. default=false => door de slimme meter te bepalen -> true door Frontend.json = altijd aan
  let HeeftWater			= false	//watermeter aanwezig. default=false => door de slimme meter te bepalen -> true door Frontend.json = altijd aan
  let EnableHist			= true  //weergave historische gegevens
  let Act_Watt				= false  //display actual in Watt instead of kW
  let SettingsRead 			= false 
  let AvoidSpikes			= false 
  let IgnoreInjection		= false 
  let IgnoreGas				= false 
  let Dongle_Config			= ""
  let eid_enabled			= false
  let eid_planner_enabled	= false
  let pairing_enabled    	= false
  let Meter_Source          = "DSMR"
  
  let locale 				= 'nl';  // standaard

// ---- DASH
let TotalAmps=0.0,minKW = 0.0, maxKW = 0.0,minV = 0.0, maxV = 0.0, Pmax,Gmax, Wmax;
let hist_arrW=[4], hist_arrG=[4], hist_arrPa=[4], hist_arrPi=[4], hist_arrP=[4]; //berekening verbruik
let day = 0;
let DailyHistoryReady = false;

function getTimestampDayKey(timestampValue) {
  if (typeof timestampValue !== "string" || timestampValue.length < 6) return 0;

  const year = Number(timestampValue.slice(0, 2));
  const month = Number(timestampValue.slice(2, 4));
  const day = Number(timestampValue.slice(4, 6));

  if ([year, month, day].some(Number.isNaN)) return 0;
  return (year * 10000) + (month * 100) + day;
}

function resetDailyHistoryBuffers() {
  hist_arrG = [0, 0, 0, 0];
  hist_arrW = [0, 0, 0, 0];
  hist_arrPa = [0, 0, 0, 0];
  hist_arrPi = [0, 0, 0, 0];
  DailyHistoryReady = false;
}

function resetDashboardDailyValues() {
  updateGaugeTrend(trend_p, [0, 0, 0]);
  updateGaugeTrend(trend_pi, [0, 0, 0]);
  updateGaugeTrend(trend_pa, [0, 0, 0]);
  updateGaugeTrend(trend_g, [0, 0, 0]);
  updateGaugeTrend(trend_q, [0, 0, 0]);
  updateGaugeTrend(trend_w, [0, 0, 0]);

  document.getElementById("P").innerHTML = formatValue(0);
  document.getElementById("Pi").innerHTML = formatValue(0);
  document.getElementById("Pa").innerHTML = formatValue(0);
  document.getElementById("G").innerHTML = formatValue(0);
  document.getElementById("Q").innerHTML = formatValueLocale(0, 0, 0);
  document.getElementById("W").innerHTML = formatValueLocale(0, 0, 0);
}

function refreshSolarSelfUse() {
  const json = objDAL.getSolar();
  const dailyProduction = Number(json?.total?.daily) || 0;

  if (!dailyProduction) {
    document.getElementById('scr').innerHTML = "-";
    document.getElementById('seue').innerHTML = "-";
    return;
  }

  const exportedWh = Math.max(0, Number(Pi_today) * 1000);
  const importedWh = Math.max(0, Number(Pd_today) * 1000);
  if ( exportedWh > dailyProduction) {
    // Feed-in cannot be higher than PV production when both counters describe the
    // same source/day. This can happen with battery export or mismatched day resets,
    // so direct self-use cannot be derived reliably.
    document.getElementById('scr').innerHTML = "-";
    document.getElementById('seue').innerHTML = importedWh > 0
      ? Number((dailyProduction / (dailyProduction + importedWh)) * 100).toFixed(0)
      : "100";
    return;
  }

  const selfUsedWh = Math.max(0, dailyProduction - exportedWh);
  const totalUsageWh = selfUsedWh + importedWh;

  document.getElementById('scr').innerHTML = Number((selfUsedWh / dailyProduction) * 100).toFixed(0);
  document.getElementById('seue').innerHTML = totalUsageWh > 0
    ? Number((selfUsedWh / totalUsageWh) * 100).toFixed(0)
    : "-";
}

//default struct for the gauge element
// ! structuredClone can NOT copy a struct with functions
let cfgDefaultGAUGE = {
  type: 'doughnut',
  data: {
    datasets: [
      {
        label: "l1",
        backgroundColor: ["#314b77", "rgba(0,0,0,0.1)"],
      },
      {
        label: "l2",
        backgroundColor: ["#316b77", "rgba(0,0,0,0.1)"],
      },
      {
        label: "l3",
        backgroundColor: ["#318b77", "rgba(0,0,0,0.1)"],
      }
    ]
  },
  options: {
    events: [],
    title: {
      display: true,
      text: 'Voltage',
      position: "bottom",
      padding: -18,
      fontSize: 17,
      fontColor: "#000",
      fontFamily: "Dosis",
    },
    responsive: true,
    circumference: Math.PI,
    rotation: -Math.PI,
    plugins: {
      labels: {
        render: {},   //structuredClone will fail if there is a function
        arc: true,
        fontColor: ["#fff", "rgba(0,0,0,0)"],
      },//labels      
    }, //plugins
    legend: { display: false },
  }, //options
};

//copy for phases based gauges
let cfgDefaultPHASES = structuredClone(cfgDefaultGAUGE);
cfgDefaultPHASES.data.datasets[0].label = "L1";
cfgDefaultPHASES.data.datasets[1].label = "L2";
cfgDefaultPHASES.data.datasets[2].label = "L3";

//copy for trend based gauges
let cfgDefaultTREND = structuredClone(cfgDefaultGAUGE);
cfgDefaultTREND.data.datasets[0].label = "vandaag";
cfgDefaultTREND.data.datasets[1].label = "gister";
cfgDefaultTREND.data.datasets[2].label = "eergisteren";

//phases based gauges
function renderLabelVoltage(args){return args.value + 207 + " V";}
let cfgGaugeVOLTAGE = structuredClone(cfgDefaultPHASES);
cfgGaugeVOLTAGE.options.title.text = "V";
cfgGaugeVOLTAGE.options.plugins.labels.render = renderLabelVoltage;

function renderLabel3F(args){return args.value + " A";}
let cfgGauge3F = structuredClone(cfgDefaultPHASES);
cfgGauge3F.options.title.text = "Ampere";
cfgGauge3F.options.plugins.labels.render = renderLabel3F;

//trend based gauges
function renderLabelElektra(args){return args.value + " kWh";}
let cfgGaugeELEKTRA = structuredClone(cfgDefaultTREND);
cfgGaugeELEKTRA.options.title.text = "kWh";
cfgGaugeELEKTRA.options.plugins.labels.render = renderLabelElektra;

let cfgGaugeSolar = {
  type: 'doughnut',
  data: {
    datasets: [
      {
        label: "Productie",
        backgroundColor: ["#f7b638", "rgba(0,0,0,0.1)"],
      }
    ]
  },
  options: {
    events: [],
    title: {
      display: true,
      text: 'Watt',
      position: "bottom",
      padding: -18,
      fontSize: 17,
      fontColor: "#000",
      fontFamily: "Dosis",
    },
    responsive: true,
    circumference: Math.PI,
    rotation: -Math.PI,
    plugins: {
      labels: {
        render: {},   //structuredClone will fail if there is a function
        arc: true,
        fontColor: ["#000", "rgba(0,0,0,0)"],
      },//labels      
    }, //plugins
    legend: { display: false },
  }, //options
};

function renderLabelPeak(args){return args.value + "kW";}
let cfgGaugePeak = structuredClone(cfgGaugeSolar);
cfgGaugePeak.options.title.text = "kW";
cfgGaugePeak.options.plugins.labels.render = renderLabelPeak;
cfgGaugePeak.data.datasets[0].backgroundColor = ["#009900", "rgba(0,0,0,0.1)"];

// function renderLabelAccu(args){return args.value + "%";}
function renderLabelAccu(args){return "";}
let cfgGaugeAccu = structuredClone(cfgGaugeSolar);
cfgGaugeAccu.options.title.text = "%";
cfgGaugeAccu.options.plugins.labels.render = renderLabelAccu;
cfgGaugeAccu.data.datasets[0].backgroundColor = ["#007700", "rgba(0,0,0,0.1)"];

cfgGaugeSolar.options.plugins.labels.render = renderLabelSolar;
// function renderLabelSolar(args){return args.value + "W";}
function renderLabelSolar(args){return "";}

let cfgGaugeELEKTRA2 = structuredClone(cfgDefaultTREND);
cfgGaugeELEKTRA2.options.title.text = "kWh";
cfgGaugeELEKTRA2.options.plugins.labels.render = renderLabelElektra;

let cfgGaugeELEKTRA3 = structuredClone(cfgDefaultTREND);
cfgGaugeELEKTRA3.options.title.text = "kWh";
cfgGaugeELEKTRA3.options.plugins.labels.render = renderLabelElektra;

function renderLabelGas(args){return args.value + " " + SQUARE_M_CUBED;}
let cfgGaugeGAS = structuredClone(cfgDefaultTREND);
cfgGaugeGAS.options.title.text = SQUARE_M_CUBED;
cfgGaugeGAS.options.plugins.labels.render = renderLabelGas;

function renderLabelWarmte(args){return args.value;}
let cfgGaugeWARMTE = structuredClone(cfgDefaultTREND);
cfgGaugeWARMTE.options.title.text = "kJ";
cfgGaugeWARMTE.options.plugins.labels.render = renderLabelWarmte;

function renderLabelWater(args){return args.value + " ltr";}
let cfgGaugeWATER = structuredClone(cfgDefaultTREND);
cfgGaugeWATER.options.title.text = "Liter";
cfgGaugeWATER.options.plugins.labels.render = renderLabelWater;

function ShowHidePV(){
	if ( document.getElementById('pv_enphase').checked ) {
		document.getElementById('conf_token').style.display = "none";
		document.getElementById('gw_url').value = "https://envoy/api/v1/production";
	}
	if ( document.getElementById('pv_solaredge').checked ) {
		document.getElementById('conf_token').style.display = "block";
		document.getElementById('gw_url').value = "";
	}
}
  
function InsightData(data){			
            const eenheden = {
                "U1piek": "V",
                "U2piek": "V",
                "U3piek": "V",
                "U1min": "V",
                "U2min": "V",
                "U3min": "V",
                "I1piek": "A",
                "I2piek": "A",
                "I3piek": "A",
                "Psluip": "W",
                "P1max": "W",
                "P2max": "W",
                "P3max": "W",
                "P1min": "W",
                "P2min": "W",
                "P3min": "W",
                "TU1over": "sec",
                "TU2over": "sec",
                "TU3over": "sec",
                "start_time": "uu:mm"
            };

            const insightRows = [
                { labelKey: "ins-current-peak", keys: ["I1piek", "I2piek", "I3piek"] },
                { labelKey: "ins-power-peak", keys: ["P1max", "P2max", "P3max"] },
                { labelKey: "ins-power-minimum", keys: ["P1min", "P2min", "P3min"] },
                { labelKey: "ins-voltage-peak", keys: ["U1piek", "U2piek", "U3piek"] },
                { labelKey: "ins-voltage-minimum", keys: ["U1min", "U2min", "U3min"] },
                { labelKey: "ins-time-overvoltage", keys: ["TU1over", "TU2over", "TU3over"] },
                { labelKey: "lbl-Psluip", keys: ["Psluip", null, null] },
                { labelKey: "lbl-start_time", keys: ["start_time", null, null] }
            ];

            function formatInsightValue(key, value) {
                if (value === undefined || value === null) return "";

                if (key.startsWith("U")) {
                    return formatValueLocale(value / 1000, 1, 1);
                }

                if (key.startsWith("I")) {
                    return formatValueLocale(value / 1000, 3, 3);
                }

                if (key === "start_time") {
                    const uur = Math.trunc(value / 3600 % 24);
                    const minuten = Math.trunc(value / 60 % 60);
                    return String(uur).padStart(2, '0') + ":" + String(minuten).padStart(2, '0');
                }

                if (key.startsWith("Psl") && value == 0xFFFFFFFF) {
                    return "-";
                }

                return value;
            }

            function rowTitle(keys) {
                return keys
                    .filter(key => key)
                    .map(key => t("tip-" + key))
                    .filter(text => text && !text.startsWith("tip-"))
                    .join("\n");
            }

            const tbody = document.getElementById("InsightsTableBody");
			tbody.innerHTML = '';

            for (const rowData of insightRows) {
                const row = document.createElement("tr");
                const naamCell = document.createElement("td");
                const l1Cell = document.createElement("td");
                const l2Cell = document.createElement("td");
                const l3Cell = document.createElement("td");
                const eenheidCell = document.createElement("td");

                naamCell.textContent = t(rowData.labelKey);
                naamCell.title = rowTitle(rowData.keys);

                const [l1Key, l2Key, l3Key] = rowData.keys;
                l1Cell.textContent = l1Key ? formatInsightValue(l1Key, data[l1Key]) : "";
                l2Cell.textContent = l2Key ? formatInsightValue(l2Key, data[l2Key]) : "";
                l3Cell.textContent = l3Key ? formatInsightValue(l3Key, data[l3Key]) : "";
                eenheidCell.textContent = eenheden[l1Key] || "";
                eenheidCell.style.textAlign = "center";

                row.appendChild(naamCell);
                row.appendChild(l1Cell);
                row.appendChild(l2Cell);
                row.appendChild(l3Cell);
                row.appendChild(eenheidCell);
                tbody.appendChild(row);
            }
}

function SolarSendData() {

	const jsonData = {
		"gateway-url"     : document.getElementById('gw_url').value,
		wp                : parseInt(document.getElementById('wp').value),
		"refresh-interval": parseInt(document.getElementById('interval').value),
		token             : document.getElementById('token').value,
		expire            : 0,
		siteid            : parseInt(document.getElementById('siteid').value)
	};

	var filename = "solaredge.json";
	if ( document.getElementById('pv_enphase').checked ) filename = "enphase.json"

	let jsonBlob = new Blob([JSON.stringify(jsonData, null, 2)], { type: "application/json" });
    let formData = new FormData();
    formData.append("file", jsonBlob, filename );
		
	fetch("/upload", {
        method: "POST",
        body: formData
    })
    .then(response => response.text())
    .then(result => window.alert("Solar config saved"))
    .catch(error => console.error("Error:", error));

}

//entry point 
window.onload=bootsTrapMain;

//============================================================================  
// Handle page visibility change events
function visibilityListener() {
  switch(document.visibilityState) {
    case "hidden":
		clearInterval(tabTimer);  
		PauseAPI=true;
		break;
    case "visible":
		PauseAPI=false;
		openTab();
// 		if ( activeTab == "bEID") getclaim();
		break;
    default:
		console.log("visibilityListener() - unknown visibiltyState");
  }
}

document.addEventListener( "visibilitychange", visibilityListener );

//============================================================================  
// if the user uses the backbutton in the browser
window.addEventListener('popstate', function (event) {
	activeTab = "b" + location.hash.slice(1, location.hash.length);
	openTab();
});

window.addEventListener('hashchange', () => {
  console.log('Hash changed:', window.location.hash);
  // Handle the hash change here
});


//============================================================================  
function applyShowCols(condition, tablesAndCols) {
  tablesAndCols.forEach(([table, cols]) => {
    cols.forEach(col => show_hide_column(table, col, condition));
  });
}

//============================================================================
// Normalize config values from Frontend.json (booleans can arrive as strings)
function asBoolean(value, fallback = false) {
	if (typeof value === "boolean") return value;
	if (typeof value === "number") return value !== 0;
	if (typeof value === "string") {
		const normalized = value.trim().toLowerCase();
		if (["true", "1", "yes", "on"].includes(normalized)) return true;
		if (["false", "0", "no", "off", ""].includes(normalized)) return false;
	}
	return fallback;
}

function asNumber(value) {
	if (typeof value === "number") return Number.isFinite(value) ? value : NaN;
	if (typeof value === "string") {
		const normalized = value.trim().replace(",", ".");
		const parsed = Number(normalized);
		return Number.isFinite(parsed) ? parsed : NaN;
	}
	return NaN;
}

function hasValidMeterValue(field) {
	if (!field || !Object.prototype.hasOwnProperty.call(field, "value")) return false;
	return !Number.isNaN(asNumber(field.value));
}

function meterValue(field, fallback = 0) {
	const value = asNumber(field?.value);
	return Number.isNaN(value) ? fallback : value;
}

function hasHistoryData(data) {
	return data && Array.isArray(data.data) && data.data.length > 0;
}

//============================================================================  
  
function SetOnSettings(json) {
	// Initiele detectie: water, gas, teruglevering
	if (!HeeftGas && !IgnoreGas) {
		const hasGasValue = hasValidMeterValue(json.gas_delivered);
		const hasGasTimestamp =
			typeof json.gas_delivered_timestamp?.value === "string" &&
			json.gas_delivered_timestamp.value.trim() !== "" &&
			json.gas_delivered_timestamp.value !== "-";
		HeeftGas = hasGasValue || hasGasTimestamp;
	}
	if (!HeeftWater) HeeftWater = hasValidMeterValue(json.water);

	if (!Injection) {
		Injection = !isNaN(json.energy_returned_tariff1?.value) ? json.energy_returned_tariff1.value : false;
		if (!Injection) Injection = !isNaN(json.energy_returned_tariff2?.value) ? json.energy_returned_tariff2.value : false;
	}
	Injection = Injection && !IgnoreInjection;

	// Fasebepaling voor slimme meter
	if (Dongle_Config !== "p1-q") {
		Phases = 1;
		if (!isNaN(json.current_l2?.value)) Phases++;
		if (!isNaN(json.current_l3?.value)) Phases++;
	}

	// Verberg historische weergave indien uitgeschakeld
	if (!EnableHist) {
		["bHoursTab", "bDaysTab", "bMonthsTab", "l3"].forEach(id => {
			document.getElementById(id).style.display = "none";
		});
	}

	// Kolommen tonen/verbergen op basis van beschikbaarheid
	const showCols = (table, cols, condition) => cols.forEach(col => show_hide_column(table, col, condition));

	applyShowCols(HeeftWater, [
	  ['lastHoursTable', [4]],
	  ['lastDaysTable', [4]],
	  ['lastMonthsTable', [13, 14, 15, 16]]
	]);

// 	showCols('lastHoursTable',  [4], HeeftWater);
// 	showCols('lastDaysTable',   [4], HeeftWater);
// 	showCols('lastMonthsTable', [13, 14, 15, 16], HeeftWater);

	if (Dongle_Config !== "p1-q") {
		applyShowCols(HeeftGas, [
		  ['lastHoursTable', [3]],
		  ['lastDaysTable', [3]],
		  ['lastMonthsTable', [9,10,11,12]],
		  ['lastMonthsTableCosts', [3,8]]
		]);
// 
// 		showCols('lastHoursTable',  [3], HeeftGas);
// 		showCols('lastDaysTable',   [3], HeeftGas);
// 		showCols('lastMonthsTable', [9, 10, 11, 12], HeeftGas);
// 		showCols('lastMonthsTableCosts', [3, 8], HeeftGas);
	}

	applyShowCols(Injection, [
	  ['lastHoursTable', [2]],
	  ['lastDaysTable', [2]],
	  ['lastMonthsTable', [5, 6, 7, 8]]
	]);

// 	showCols('lastHoursTable',  [2], Injection);
// 	showCols('lastDaysTable',   [2], Injection);
// 	showCols('lastMonthsTable', [5, 6, 7, 8], Injection);

	// Specifieke aanpassing voor p1-q dongle
	if (Dongle_Config === "p1-q") {
// 		showCols('lastHoursTable', [1], false);
// 		showCols('lastDaysTable',  [1], false);
		
		applyShowCols(false, [
		  ['lastHoursTable', [1]],
		  ['lastDaysTable', [1]],
		]);
		
		["l1", "l3"].forEach(id => document.getElementById(id).style.display = "none");
		document.getElementById("l8").style.display = "block";

		document.querySelectorAll('.mbus-name').forEach(e => e.innerHTML = "Warmte<br>(GJ)");
		document.querySelectorAll('.mbus-unit').forEach(e => e.innerHTML = "(GJ)");
	}
}

function UpdateAccu(){
	let json = objDAL.getAccu();
	"active" in json ? AccuActive=json.active : AccuActive = false;
	console.log("AccuActive: "+ AccuActive);
	if ( AccuActive ) {		
		console.log("parsed response: "+ JSON.stringify(json));
		console.log("json.status: "+ json.status);
		console.log("json.unit: "+ json.unit);
		console.log("json.currentPower: "+ json.currentPower);
		console.log("json.chargeLevel: "+ json.chargeLevel);				
		trend_accu.data.datasets[0].data=[json.chargeLevel,100-json.chargeLevel];	
		trend_accu.options.title.text = Number(json.chargeLevel).toLocaleString('nl-NL', {minimumFractionDigits: 0, maximumFractionDigits: 0} )+" %";
		trend_accu.update();
		document.getElementById('dash_accu_p').innerHTML = formatValue(json.currentPower);
		document.getElementById('dash_accu').style.display = 'block';
		document.getElementById('accu-status').innerHTML = json.status;

	}
 }

//============================================================================  
function UpdateSolar(){

	let json = objDAL.getSolar();
	"active" in json ? SolarActive=json.active : SolarActive = false;
	console.log("SolarActive: "+ SolarActive);
	if ( SolarActive ) {		
		console.log("parsed response: "+ JSON.stringify(json));
		console.log("json.total.daily: "+ json.total.daily);
		console.log("json.total.actual: "+ json.total.actual);
		console.log("json.Wp: "+ json.Wp);
		trend_solar.data.datasets[0].data=[json.total.actual,json.Wp-json.total.actual];	
		trend_solar.update();
		document.getElementById('dash_solar_p').innerHTML = formatValue(json.total.daily/1000.0);
		document.getElementById('dash_solar').style.display = 'block';
		trend_solar.options.title.text = Number(json.total.actual).toLocaleString('nl-NL', {minimumFractionDigits: 0, maximumFractionDigits: 0} )+" W";
		refreshSolarSelfUse();
	}
 }

//============================================================================  

function nrgm_start_stop(cmd){	
	fetch(APIHOST+"/pair?cmd="+cmd, {"setTimeout": 5000})
      .then(response => response.json())
      .then()
    nrgm_getstatus();
	NRGStatusTimer = setInterval(nrgm_getstatus, 5000); //every 5 sec
}

function nrgm_getstatus(){
	document.getElementById('nrgstatus').innerHTML = "ophalen...";  
	
	fetch(APIHOST+"/pair?cmd=status", {"setTimeout": 5000})
      .then(response => response.json())
      .then(json => {
        console.log("parsed response: "+ JSON.stringify(json));
		let nrgstatus;
        if ( "status" in json ) {
        	switch(json.status) {
			  case 0: //_INACTIVE
				nrgstatus = "INACTIEF";
				break;
			  case 1: //_WAITING
				nrgstatus = "WACHT OP AANMELDEN...";
				break;
			  case 2://_PAIRING
				nrgstatus = "BEZIG MET AANMELDEN";
				break;
			  case 3://_CONFIRMED
				nrgstatus = "<FONT COLOR='#70ac4d'>GEKOPPELD";
				clearInterval(NRGStatusTimer);
				break;
			}
        
        	document.getElementById('nrgstatus').innerHTML = nrgstatus;
        }	
        if ( "client" in json && json.client.length) {
        	document.getElementById('nrghost').innerHTML = json.client;
        }
      })
      .catch(function(error) {
      console.log(error);
      	document.getElementById('nrgstatus').innerHTML = "Fout - Probeer opnieuw!";
      });
}

function ProcessEIDClaim(json){
	if ( "webhookUrl" in json ) {
		document.getElementById('status').innerHTML = "<FONT COLOR='#70ac4d'>GEKOPPELD";
		document.getElementById('claim').style.display = 'none';
	}	
	if ( "claimUrl" in json ) {
		document.getElementById('status').innerHTML = "<FONT COLOR='RED'>Waiting for Activation";
		document.getElementById('claimurl').innerHTML = "<a href='" + json.claimUrl +"'target='_blank'>Activeer koppeling<i class='iconify' data-icon='mdi-external-link'></i></a>";
		document.getElementById('claim').style.display = 'block';
	}

}

function ProcessEIDPlanner(jsonData){
	console.log("process EID Planner");
// 	jsonData = {"h_start":21,"data":[1,1,1,1,2,2,2,2,2,1]}
	
	const kleurMapping = [
	{ kleur: "red", tekst: "ZEER SLECHT MOMENT" },
	{ kleur: "orange", tekst: "SLECHT MOMENT" },
	{ kleur: "grey", tekst: "NEUTRAAL" },
	{ kleur: "lightgreen", tekst: "GOED MOMENT" },
	{ kleur: "green", tekst: "ZEER GOED MOMENT" },
	];
	
	const huidigeUur = new Date().getHours();
	const offset = Math.max(0, huidigeUur - jsonData.h_start);
	const slicedData = jsonData.data.slice(offset, offset + 8);
	
	const container = document.getElementById("stroomplanner-hours");
	const footer = document.getElementById("stroomplanner-footer");
	
	container.innerHTML = "";
	footer.innerHTML = "";
	
	const eersteWaarde = slicedData[0];
	const kleurClass = kleurMapping[eersteWaarde].kleur;
	const tekstOmschrijving = kleurMapping[eersteWaarde].tekst;
	
	let duur = 1;
	for (let i = 1; i < slicedData.length; i++) {
	if (slicedData[i] === eersteWaarde) {
	  duur++;
	} else {
	  break;
	}
	}
	
	const vanUur = jsonData.h_start + offset;
	const totUur = (vanUur + duur) % 24;
	
	footer.innerHTML = `
	<span class="dot ${kleurClass}"></span> 
	Nu tot ${totUur}u: <strong>${tekstOmschrijving}</strong>
	`;
	
	// fill hours 
	slicedData.forEach((value, index) => {
	const kleur = kleurMapping[value].kleur;
	const uur = (jsonData.h_start + offset + index) % 24;
	
	const div = document.createElement("div");
	div.className = `eid-hour ${kleur}`;
	div.textContent = `${uur}u`;
	container.appendChild(div);
	});
	
}

function getclaim(){
	document.getElementById('status').innerHTML = "Status ophalen...";  	
	objDAL.refreshEIDClaim();
}

//============================================================================  

function parseVersionManifest(json)
{	
	console.log("json.version:" + json.version + " firmwareVersion: "+ firmwareVersion);
	if ( json.version != "" && firmwareVersion != "") {
	  if ( firmwareVersion < (json.major*10000 + 100 * json.minor + json.fix) ) 
		document.getElementById('message').innerHTML = "Software versie " + json.version + " beschikbaar";
	  else document.getElementById('message').innerHTML = "";
	}
}

function refreshDashboard(json){
	setPresentationType('TAB'); //zet grafische mode uit
	
	let Parr=[3],Parra=[3],Parri=[3], Garr=[3],Warr=[3];

		//-------CHECKS

		SetOnSettings(json);
		const timestampValue = json.timestamp?.value ?? "-";
		if (timestampValue == "-") {
			console.log("timestamp missing : p1 data incorrect!");
			return;
		}

		const dayKey = getTimestampDayKey(timestampValue);
		if (dayKey && DayEpoch !== dayKey) {
			resetDailyHistoryBuffers();
			Pi_today = 0;
			Pd_today = 0;
			if (EnableHist) objDAL.refreshHist();
			if (SolarActive) objDAL.refreshSolar();
			DayEpoch = dayKey;
		} else if (!DayEpoch && EnableHist) {
			objDAL.refreshHist();
		}
		
		//-------TOON METERS
		document.getElementById("w8api").style.display = "none"; //hide wait message
		document.getElementById("inner-dash").style.display = "flex"; //unhide dashboard
		if (Injection) {		
			document.getElementById("l5").style.display = "block";
			document.getElementById("l6").style.display = "block";
			document.getElementById("Ph").innerHTML = "<span class='iconify' data-icon='mdi-lightning-bolt'></span>" + t('lbl-net-use');
		}
		if (HeeftGas && EnableHist && (Dongle_Config != "p1-q") && !IgnoreGas) document.getElementById("l4").style.display = "block";
		if (HeeftWater && EnableHist) { 
			document.getElementById("l7").style.display = "block";
			document.getElementById("l2").style.display = "none";
		}
		const peakLast = meterValue(json.peak_pwr_last_q, NaN);
		const peakHighest = meterValue(json.highest_peak_pwr, NaN);
		const hasPeakData = !Number.isNaN(peakLast) && !Number.isNaN(peakHighest);
		if (hasPeakData) document.getElementById("dash_peak").style.display = "block";

		//------- EID Planner
		if ( eid_planner_enabled ) objDAL.refreshEIDPlanner();
		
		//-------Kwartierpiek
		if (hasPeakData) {
			document.getElementById("peak_month").innerHTML = formatValue(peakHighest);
			document.getElementById("dash_peak_delta").innerText = formatValue(peakHighest - peakLast);
			trend_peak.data.datasets[0].data=[peakLast, peakHighest - peakLast];	
			trend_peak.update();
			if (peakLast > 0.75 * peakHighest ) trend_peak.data.datasets[0].backgroundColor = ["#dd0000", "rgba(0,0,0,0.1)"];
			else trend_peak.data.datasets[0].backgroundColor = ["#009900", "rgba(0,0,0,0.1)"];
		}
		
		//-------VOLTAGE
		let v1 = 0, v2 = 0, v3 = 0;
		v1 = meterValue(json.voltage_l1, 0); 
		v2 = meterValue(json.voltage_l2, 0);
		v3 = meterValue(json.voltage_l3, 0);


		if ( ShowVoltage && v1 ) {
			document.getElementById("l2").style.display = "block"
			document.getElementById("fases").innerHTML = Phases;
			
      let Vmin_now = v1;
      let Vmax_now = v1;
      if( Phases != 1){
			  Vmin_now = math.min(v1, v2, v3);
			  Vmax_now = math.max(v1, v2, v3);
      }
			
			//min - max waarde
			if (minV == 0.0 || Vmin_now < minV) { minV = Vmin_now; }
			if (Vmax_now > maxV) { maxV = Vmax_now; }

			//update gauge
			gaugeV.data.datasets[0].data=[v1-207,253-v1];
			if (v2) gaugeV.data.datasets[1].data=[v2-207,253-v2];
			if (v3) gaugeV.data.datasets[2].data=[v3-207,253-v3];
			gaugeV.update();
		}
		//-------ACTUAL
		//afname of teruglevering bepalen en signaleren
		let TotalKW	= meterValue(json.power_delivered, 0) - meterValue(json.power_returned, 0);

		if ( TotalKW <= 0 ) { 
// 			TotalKW = -1.0 * json.power_returned.value;
			document.getElementById("power_delivered_l1h").style.backgroundColor = "green";
// 			document.getElementById("Ph").style.backgroundColor = "green";
		} else
		{
// 			TotalKW = json.power_delivered.value;
			if (Injection) {
				document.getElementById("power_delivered_l1h").style.backgroundColor = "red";
// 				document.getElementById("Ph").style.backgroundColor = "red";
			}
			else document.getElementById("power_delivered_l1h").style.backgroundColor = "#314b77";
		}
		
		//update gauge
		const currentL1 = meterValue(json.current_l1, NaN);
		const currentL2 = meterValue(json.current_l2, 0);
		const currentL3 = meterValue(json.current_l3, 0);
		if ( !Number.isNaN(currentL1) ) {
		TotalAmps = currentL1 + currentL2 + currentL3;

		gauge3f.data.datasets[0].data=[currentL1, AMPS - currentL1];
		if (Phases == 2) {
			gauge3f.data.datasets[1].data=[currentL2, AMPS - currentL2];
			document.getElementById("f2").style.display = "inline-block";
			document.getElementById("v2").style.display = "inline-block";
			}
		if (Phases == 3) {
			gauge3f.data.datasets[1].data=[currentL2, AMPS - currentL2];
			document.getElementById("f2").style.display = "inline-block";
			document.getElementById("v2").style.display = "inline-block";
			
			gauge3f.data.datasets[2].data=[currentL3, AMPS - currentL3];
			document.getElementById("f3").style.display = "inline-block";
			document.getElementById("v3").style.display = "inline-block";
			}
		} else {
			// current is missing = calc current based on actual power		
			TotalAmps = Number(Math.abs(TotalKW)*1000/230).toFixed(0);
			gauge3f.data.datasets[0].data=[TotalAmps,AMPS-TotalAmps];
		};	
		
		gauge3f.options.title.text = Number(TotalAmps).toFixed(2) + " A";
		gauge3f.update();

		//update actuele vermogen			
		if (Act_Watt) {
			document.getElementById("power_delivered").innerHTML = Number(TotalKW  * 1000).toFixed(0);
			document.getElementById("dsh-power").textContent = 'Watt';
		} else {
			document.getElementById("power_delivered").innerHTML = Number(TotalKW).toLocaleString("nl", {minimumFractionDigits: 3, maximumFractionDigits: 3} ) ;
			document.getElementById("dsh-power").textContent = 'kW';
		}

		//vermogen min - max bepalen
		let nvKW= TotalKW; 
		if (minKW == 0.0 || nvKW < minKW) { minKW = nvKW;}
		if (nvKW> maxKW){ maxKW = nvKW; }
				
    // stop here if there is no history enabled
		if (!EnableHist) {Spinner(false);return;}

		if (!DailyHistoryReady) {
		  Pi_today = 0;
		  Pd_today = 0;
		  resetDashboardDailyValues();
		  refreshSolarSelfUse();
		  Spinner(false);
		  return;
		}
		
    //-------VERBRUIK METER	
		if (Dongle_Config != "p1-q") {

      //bereken verschillen afname, teruglevering en totaal
      let nPA = meterValue(json.energy_delivered_tariff1, 0) + meterValue(json.energy_delivered_tariff2, 0);
      let nPI = meterValue(json.energy_returned_tariff1, 0) + meterValue(json.energy_returned_tariff2, 0);
      Parra = calculateDifferences( nPA, hist_arrPa, 1);
      Parri = calculateDifferences( nPI, hist_arrPi, 1);
      for(let i=0;i<3;i++){ Parr[i]=Parra[i] - Parri[i]; }

      //dataset berekenen voor Ptotaal      
      updateGaugeTrend(trend_p, Parr);
      document.getElementById("P").innerHTML = formatValue( Parr[0] );
      const netUsageTitle = document.querySelector("#l3 h1");
      if (netUsageTitle) {
        netUsageTitle.style.backgroundColor = (Parr[0] < 0) ? "green" : "red";
      }
      
		  Pi_today = Parri[0];
		  Pd_today = Parra[0];
		  refreshSolarSelfUse();
	        
      if (Injection) 
      {
        //-------INJECTIE METER
        updateGaugeTrend(trend_pi, Parri);
        document.getElementById("Pi").innerHTML = formatValue( Parri[0] );

        //-------AFNAME METER	
        updateGaugeTrend(trend_pa, Parra);
        document.getElementById("Pa").innerHTML = formatValue( Parra[0] );
      }
		} //!= p1-q
		
    //-------GAS	
			if ( HeeftGas && hasValidMeterValue(json.gas_delivered) && (Dongle_Config != "p1-q") ) 
		{
      Garr = calculateDifferences(meterValue(json.gas_delivered, 0), hist_arrG, 1);
      updateGaugeTrend(trend_g, Garr);
			document.getElementById("G").innerHTML = formatValue(Garr[0]);
		}
		
		//-------WATER	
			if (HeeftWater && hasValidMeterValue(json.water)) 
		{
      Warr = calculateDifferences(meterValue(json.water, 0), hist_arrW, 1000);
      updateGaugeTrend(trend_w, Warr);
			document.getElementById("W").innerHTML = Number(Warr[0]).toLocaleString();
		}
				
		//-------HEAT
		if (Dongle_Config == "p1-q") 
		{
      Garr = calculateDifferences(meterValue(json.gas_delivered, 0), hist_arrG, 1000);
      updateGaugeTrend(trend_q, Garr);
			document.getElementById("Q").innerHTML = Number(Garr[0]).toLocaleString("nl", {minimumFractionDigits: 0, maximumFractionDigits: 0} );
		}
	
	if ( SolarActive ) UpdateSolar();
	if ( AccuActive ) UpdateAccu();

}

//============================================================================   
function UpdateDash()
{	
	console.log("Update dash");
	objDAL.refreshFields();
}

//bereken verschillen gas, afname, teruglevering en totaal
function calculateDifferences(curval, hist_arr, factor)
{
  let out = []  
  for(let i=0; i<3; i++)
  {
    if (i==0) 
      out[0] = (curval - hist_arr[1]) * factor;
    else 
      out[i] = (hist_arr[i] - hist_arr[i+1]) * factor;

    if (out[i] < 0) out[i] = 0;
  }
  return out;
}
//update dataset and update gauge 
function updateGaugeTrend(objGauge, arr)
{
  let nMax = math.max(arr);
  for(let i=0; i<3; i++){
    objGauge.data.datasets[i].data = [ Number(arr[i]).toFixed(1), Number(nMax-arr[i]).toFixed(1) ];
  }
  objGauge.update();
}
	


//create all chart-based gauges
function createDashboardGauges()
{
	trend_p 	= new Chart(document.getElementById("container-3"), cfgGaugeELEKTRA);
	trend_pi 	= new Chart(document.getElementById("container-5"), cfgGaugeELEKTRA2);
	trend_pa 	= new Chart(document.getElementById("container-6"), cfgGaugeELEKTRA3);
	trend_g 	= new Chart(document.getElementById("container-4"), cfgGaugeGAS);
	trend_q 	= new Chart(document.getElementById("container-q"), cfgGaugeWARMTE);
	trend_w 	= new Chart(document.getElementById("container-7"), cfgGaugeWATER);
	gauge3f 	= new Chart(document.getElementById("gauge3f"),     cfgGauge3F);
	gaugeV 		= new Chart(document.getElementById("gauge-v"),     cfgGaugeVOLTAGE);
	trend_solar	= new Chart(document.getElementById("container-solar"), cfgGaugeSolar);	
	trend_accu	= new Chart(document.getElementById("container-accu"), cfgGaugeAccu);	
	trend_peak	= new Chart(document.getElementById("container-peak"), cfgGaugePeak);	
}

function RedirectBtn(){

setTimeout(mijnFunctie, 2000);
}

//main entry point from DSMRindex.html or window.onload
function bootsTrapMain() 
{
  console.log("bootsTrapMain()");
//   loadIcons();

	//get and set locale
	locale = localStorage.getItem('locale') || 'nl'; 
	console.log("!-- localstorage: "+ locale);
	loadTranslations(locale); 
	document.getElementById("languageSelector").value = locale;

	createDashboardGauges();

	handle_menu_click();
	FrontendConfig();
	
	//init DAL
	initDAL(updateFromDAL);

    setMonthTableType();
    openTab();
    initActualGraph();
    setPresentationType('TAB');
	
  //goto tab after reload FSExplorer
  	if ( location.hash ) {
		const hashPart = location.hash.substring(1); // strip '#'
		const [cmd, paramString] = hashPart.split("?");
		const params = new URLSearchParams(paramString || "");
	
		console.log("hash command:", cmd);
		console.log("hash params:", paramString);
	
		switch (cmd) {
			case "UpdateStart":
				UpdateStart(paramString);				
				break;
			case "Logout":
				console.log("logout : " + "http://logout:logout@ultra-dongle.local/#Logout" );
				location.href = "http://logout:logout@ultra-dongle.local/";
				break;				
			case "Redirect":
				handleRedirect();
				break;
	
			case "Updating":
				console.log("Updaten bezig...");
				break;
	
			default:
				//redirect to cmd
				const btn = document.getElementById("b"+cmd);
				if (btn) {
					btn.click();
					activeTab = btn.id;
					openTab();
				} else console.log("Button b"+cmd+" not found");
		}
	}
    document.getElementById('dongle_io').addEventListener('input', NTSWupdateVisibility);
 	
 	BurnupBootstrap();
 	
  } // bootsTrapMain()
  
  
  //handles all redirects
  function handleRedirect(){
	console.log("location-handle: " + location.hash.split('msg=')[1]);
	//close all sections
   	let elements = document.getElementsByClassName("tabName");
    for (let i = 0; i < elements.length; i++){
        elements[i].style.display = "none";
    }
    //open only redirect section
	document.getElementById("Redirect").style.display = "block";
   	
   	if (location.hash.split('msg=')[1] == "RebootOnly"){  document.getElementById("RedirectMessage").innerHTML = "Het systeem wordt opnieuw gestart...";}
   	else if (location.hash.split('msg=')[1] == "RebootResetWifi"){  document.getElementById("RedirectMessage").innerHTML = "Het systeem wordt opnieuw gestart... Stel hierna het wifi netwerk opnieuw in. zie handleiding.";}   	
   	else if (location.hash.split('msg=')[1] == "NoUpdateServer"){  document.getElementById("RedirectMessage").innerHTML = "Het systeem wordt opnieuw gestart... Er is geen update mogelijkheid beschikbaar";} 
   	else if (location.hash.split('msg=')[1] == "Update"){  document.getElementById("RedirectMessage").innerHTML = "Het systeem wordt vernieuwd en daarna opnieuw gestart";} 

	setInterval(function() {
		let div = document.querySelector('#counter');
		let count = div.textContent * 1 - 1;
		div.textContent = count;
		if (count <= 0) {
			window.location.replace("/");
		}
	}, 1000);
}

  //============================================================================ 
  //shows or hide spinner   
  function Spinner(show) {
	if (show) {
		document.getElementById("loader").removeAttribute('hidden');
		setTimeout(() => { document.getElementById("loader").setAttribute('hidden', '');}, 5000);
	} else document.getElementById("loader").setAttribute('hidden', '');
  }
  
  //============================================================================  
  // shows or hide table column v1
  function show_hide_column(table, col_no, do_show) {
   let tbl = document.getElementById(table);
   if (!tbl) return;
   let col = tbl.getElementsByTagName('col')[col_no];
   if (col) {
     col.style.visibility = "";
     col.style.display = do_show ? "" : "none";
   }
   tbl.querySelectorAll('tr').forEach(row => {
     const cell = row.children[col_no];
     if (cell) cell.style.display = do_show ? "" : "none";
   });
}

//============================================================================  
// show or hides table column v2
function show_hide_column2(table, col_no, do_show) {

    let tbl  = document.getElementById(table);
    let rows = tbl.getElementsByTagName('tr');
// 	console.log("rows: " + rows.length);
      let cels, header;
    for (let row=0; row<rows.length;row++) {
      if ( document.getElementById('mCOST').checked ) { 
      	header = 0; 
      	if (row == rows.length-1) continue;
      } else {
		  if ( (activeTab == "bMonthsTab") && ( row < 2 )) header = row; else header = 0;
		  if ( (activeTab == "bMonthsTab") && ( row == 0 )) continue;
      }
      if (row == header) cels = rows[row].getElementsByTagName('th')
      else cels = rows[row].getElementsByTagName('td');
      if (cels.length) cels[col_no].style.display=do_show?"block":"none";
    }
  }
  
  
  function BarOnClick() {
    
//     console.log("BaronClick");
    if ( document.getElementById("switch_on").value === "true") document.getElementById("switch_on").value = "false";
    else document.getElementById("switch_on").value = "true";
    NetSwitchUpdateBar();
    
    }
  
  //============================================================================  
function NetSwitchUpdateBar() {
	let switchOn = document.getElementById("switch_on").value === "true";
	let barFillLeft = document.getElementById("barFillLeft");
	let barFillRight = document.getElementById("barFillRight");
	let barText = document.getElementById("barText");
	let value = document.getElementById("value").value;
	
	barText.innerText = value + " Watt";
	
	if ( !switchOn ) {
		barFillLeft.style.background = "green";
		barFillRight.style.background = "red";
		barFillLeft.innerHTML = "ON";
		barFillRight.innerHTML = "OFF";
	} else {
		barFillLeft.style.background = "red";
		barFillRight.style.background = "green";
		barFillLeft.innerHTML = "OFF";
		barFillRight.innerHTML = "ON";
	}
}

function refreshNetSwitch(data){
	document.getElementById("value").value = data.value;
	document.getElementById("switch_on").value = data.switch_on ? "true" : "false";
	document.getElementById("time_true").value = data.time_true || 60;
	document.getElementById("time_false").value = data.time_false || 120;
	document.getElementById("dongle_io").value = data.device.dongle_io;
	document.getElementById("device").value = data.device.name;
	document.getElementById("relay").value = data.device.relay;
	document.getElementById("default").value = data.device.default;
	NetSwitchUpdateBar();
	NTSWupdateVisibility();
}

  //============================================================================    
function fetchNetSwitchConfig() {
	objDAL.refreshNetSwitch();

//     fetch( RL_NETSWITCH )  // Vraag JSON-bestand op van de ESP32-server
//         .then(response => response.json())  // Converteer het antwoord naar JSON
//         .then(data => {
//             // HTML-elementen ophalen
//         })
//         .catch(error => console.error("Fout bij ophalen van JSON:", error));
}

  //============================================================================   
function NTSWupdateVisibility() {
    const dongleIO = parseInt(document.getElementById('dongle_io').value);
    const displayStyle = (dongleIO === 0) ? 'block' : 'none';

    document.getElementById('shelly_conf').style.display = displayStyle;
}

function SendNetSwitchJson() {
// 	console.log("send data");
		let data = {
			value: parseInt(document.getElementById("value").value),
			switch_on: document.getElementById("switch_on").value === "true",
			time_true: parseInt(document.getElementById("time_true")?.value || 60),
			time_false: parseInt(document.getElementById("time_false")?.value || 120),
			device: {
				dongle_io: document.getElementById("dongle_io").value,
				name: document.getElementById("device").value,
				relay: document.getElementById("relay").value,
				default: document.getElementById("default").value
			}
		};
// 	console.log("nsw data: " + JSON.stringify(data) );	
	let jsonBlob = new Blob([JSON.stringify(data, null, 2)], { type: "application/json" });
    let formData = new FormData();
    formData.append("file", jsonBlob, "netswitch.json");
		
	fetch("/upload", {
        method: "POST",
        body: formData
    })
    .then(response => response.text())
    .then(result => window.alert("NetSwitch config saved"))
    .catch(error => console.error("Error:", error));
  }
  
  //============================================================================  
  // handle opening the current selected tab (from onclick menuitems)
  function openTab() {

    console.log("openTab : " + activeTab );
    document.getElementById("myTopnav").className = "main-navigation"; //close dropdown menu 
    clearInterval(tabTimer);  
    clearInterval(NRGStatusTimer);
    hideAllCharts();

	switch (activeTab) {
		case "bActualTab" : 
			refreshSmActual();
			tabTimer = setInterval(refreshSmActual, UPDATE_ACTUAL );            // repeat every 10s
      		break;
		case "bInsightsTab"	: objDAL.refreshInsights(); break;
		case "bPlafondTab"	: refreshData(); break;
		case "bHoursTab"	: refreshHistData("Hours"); break;
     	case "bDaysTab"		: refreshHistData("Days"); break;
      	case "bMonthsTab"	: refreshHistData("Months"); break;
      	case "bSysInfoTab"	: refreshDevInfo(); break;
      	case "bFieldsTab":
      		refreshSmFields();      		
      		clearInterval(tabTimer);
      		tabTimer = setInterval(refreshSmFields, 10 * 1000);
      		break;
      	case "bTelegramTab":
      		refreshSmTelegram();
      		clearInterval(tabTimer); // dont repeat!
      		tabTimer = setInterval(refreshSmTelegram, 5 * 1000);
      		break;
      	case "bInfo_APIdoc": // no action
      		break;
      	case "bDashTab":
			UpdateDash();
			clearInterval(tabTimer);
			tabTimer = setInterval(UpdateDash, UPDATE_ACTUAL); // repeat every 10s
			break;
		case "bFSExplorer":
		    FSExplorer();
		    break;
		case "bEditMonths":
			document.getElementById('tabEditMonths').style.display = "block";
			document.getElementById('tabEditSettings').style.display = "none";
			EditMonths();
			break;
		case "bSettings":
		case "bEditSettings":
			data = {};
			document.getElementById('tabEditSettings').style.display = 'block';
			document.getElementById('tabEditMonths').style.display = "none";
			objDAL.refreshDeviceInformation();
			activeTab = "bEditSettings";
			break;
		case "bEID":
			getclaim();
			break;
		case "bNRGM":
			nrgm_getstatus();
			break;			
		case "bInfo_frontend":
			data = {};
			FrontendConfig();
			break;
    }
  } // openTab()
  

//============================================================================  
  function FSExplorer() {
	 let main = document.querySelector('main');
	 let fileSize = document.querySelector('fileSize');

	 Spinner(true);
	 fetch('api/listfiles', {"setTimeout": 5000}).then(function (response) {
		 return response.json();
	 }).then(function (json) {
	
	//clear previous content	 
	 let list = document.getElementById("FSmain");
	 while (list.hasChildNodes()) {  
	   list.removeChild(list.firstChild);
	 }
    
	 nFilecount = json.length - 1; //last object is general information
     let dir = '<table id="FSTable" width=90%>';
	   for (var i = 0; i < json.length - 1; i++) {
		 dir += "<tr>";
		 dir += `<td width=250px nowrap><a href ="${json[i].name}" target="_blank">${json[i].name}</a></td>`;
		 dir += `<td width=100px nowrap><small>${json[i].size}</small></td>`;
		 dir += `<td width=100px nowrap><a href ="${json[i].name}"download="${json[i].name}"> Download </a></td>`;
		 dir += `<td width=100px nowrap><a href ="${json[i].name}?delete=/${json[i].name}"> Delete </a></td>`;
		 dir += "</tr>";
	   }	// for ..
	   main.insertAdjacentHTML('beforeend', dir);
	   document.querySelectorAll('[href*=delete]').forEach((node) => {
			 node.addEventListener('click', () => {
					 if (!confirm('Delete, sure ?!')) event.preventDefault();  
			 });
	   });
	   main.insertAdjacentHTML('beforeend', '</table>');
//        main.insertAdjacentHTML('beforeend', `<div id='filecount'>Aantal bestanden: ${nFilecount} </div>`);
       main.insertAdjacentHTML('beforeend', `<div id='filecount'>${t('lbl-fm-files')}: ${nFilecount} </div>`);       
	   main.insertAdjacentHTML('beforeend', `<p id="FSFree">${t('lbl-fm-storage')}: <b>${json[i].usedBytes} ${t('lbl-fm-used')}</b> | ${json[i].totalBytes} ${t('lbl-fm-total')}`);
	   free = json[i].freeBytes;
	   fileSize.innerHTML = "<b> &nbsp; </b><p>";    // spacer                
	   Spinner(false);
	 });	// function(json)
	 
    //view selected filesize
	  document.getElementById('Ifile').addEventListener('change', () => {
      //format filesize
		  let nBytes = document.getElementById('Ifile').files[0].size;
      let output = `${nBytes} Byte`;
		  for (let aMultiples = [
			 ' KB',
			 ' MB'
			], i = 0, nApprox = nBytes / 1024; nApprox > 1; nApprox /= 1024, i++) {
			  output = nApprox.toFixed(2) + aMultiples[i];
			}

      let fUpload = true;
      //check freespace
			if (nBytes > free) {
			  fileSize.innerHTML = `<p><small> File size: ${output}</small><strong style="color: red;"> not enough space! </strong><p>`;
        fUpload = false;
			}
      //check filecount
      //TODO: 
      //  Although uploading a new file is blocked, REPLACING a file when the count is 30 must still be possible.
      //  check if filename is already on the list, if so, allow this upload.
			if ( nFilecount >= MAX_FILECOUNT) {
			  fileSize.innerHTML = `<p><small> file size: ${output}</small><strong style="color: red;"> Max number of files (${MAX_FILECOUNT}) reached! </strong><p>`;
        fUpload = false;
			}
      if( fUpload ){
        fileSize.innerHTML = `<b>File size:</b> ${output}<p>`;
			  document.getElementById('Iupload').removeAttribute('disabled');
      }
			else {			  
        document.getElementById('Iupload').setAttribute('disabled', 'disabled');
			}
	 });	
  }

function parseDeviceInfo(obj) {
  const tableRef = document.getElementById('tb_info');
  tableRef.innerHTML = ""; // clear table
  console.log("dev info compileoptions:", obj.compileoptions);

  // NETSW config
  const showNetSw = obj.compileoptions.includes("[NETSW]");
  document.getElementById("bNETSW").style.display = showNetSw ? "block" : "none";
    
  // add version info 
  const manifest = objDAL.version_manifest;
  if (manifest.version) {
    const row = tableRef.insertRow(-1);
    row.insertCell(0).innerHTML = t("lbl-latest-fwversion");
    row.insertCell(1).innerHTML = manifest.version;
    row.insertCell(2).innerHTML = `<a style='color:red' onclick='startUpdateFlow("stable")' href='#'>${t('lbl-install')}</a>`;
    console.log("last version:", manifest.major * 10000 + manifest.minor * 100);
  }
  
    if (manifest.beta) {
    const row = tableRef.insertRow(-1);
    row.insertCell(0).innerHTML = t("lbl-beta-fwversion");
    row.insertCell(1).innerHTML = manifest.beta;
    row.insertCell(2).innerHTML = `<a style='color:red' onclick='startUpdateFlow("beta")' href='#'>${t('lbl-install')}</a>`;
  }

  // add dev info
  for (let k in obj) {
    if (k === "meter_source") Meter_Source = obj[k];

    const row = tableRef.insertRow(-1);
    row.insertCell(0).innerHTML = td(k);

    if (typeof obj[k] === "object") {
      row.insertCell(1).innerHTML = obj[k].value;
      row.insertCell(2).innerHTML = obj[k].unit;
      row.cells[1].style.textAlign = "right";
    } else {
      row.insertCell(1).innerHTML = obj[k];
      row.insertCell(2);
    }

    if (k === "fwversion") {
      devVersion = obj[k];
      console.log("fwversion:", devVersion);
    }
  }

  // firmware parsing
  document.getElementById('devVersion').innerHTML = devVersion;
  firmwareVersion_dspl = devVersion;
  let tmpFW = devVersion.replace("+", " ").replace("v", "");
  const tmpX = tmpFW.split(" ")[0];
  const [maj, min, fix] = tmpX.split(".").map(Number);
  const firmwareVersion = maj * 10000 + min * 100 + fix;
  console.log("tmpFW:", tmpFW);

  // check for update
  if (manifest.version) {
    const latest = manifest.major * 10000 + manifest.minor * 100 + manifest.fix;
  }
}
    
function closeUpdate() {
	document.getElementById("updatePopup").style.visibility = "hidden";
	document.location.href="/";
	// location.reload();
}    

let progress = 0;
let progressBar, checkInterval, update_interval; 

function UpdateStart( msg ){
// 	console.log("Updatestatus msg: " + msg );
// 	console.log("Updatestatus error: " + msg.split('=')[1] );

	document.getElementById("updatePopup").style.visibility = "visible";
	progressBar = document.getElementById("progressBar");
  	
  	const [type, detail] = String(msg || '').split('=', 2);

	if (type === "error") {
		document.getElementById("updatestatus").innerText = "Error: " + (detail || "");
	} else {
		progress = 0;
		updateProgress();
	}
}
        
function RemoteUpdate(type) {        
  if (type == "esphome") document.location.href = "/remote-update?version=esphome";
  else if (type == "beta")
    document.location.href = "/remote-update?version=" + objDAL.version_manifest.beta;
  else
    document.location.href = "/remote-update?version=" + objDAL.version_manifest.version;
}

// klein hulpfunctietje om even te pauzeren
const sleep = (ms) => new Promise(r => setTimeout(r, ms));

// downloads één voor één starten (betrouwbaarder voor popup/download-blockers)
async function downloadFilesSequential(urls) {
  for (const url of urls) {
    const a = document.createElement('a');
    a.href = url;
    a.download = ''; // laat browser bestandsnaam bepalen
    document.body.appendChild(a);
    a.click();
    a.remove();
    // mini-pauze zodat de browser de download kan initialiseren
    await sleep(150);
  }
}

async function startUpdateFlow( type ) {
  if (confirm( t("lbl-download-popup-1") )){
  // 1) Start downloads (in dezelfde user-gesture context)
  await downloadFilesSequential([
    "/RNGdays.json",
    "/RNGhours.json",
    "/RNGmonths.json"
  ]);
}	

  // 2) Tweede bevestiging voor de update
  if (confirm(t("lbl-download-popup-2"))) {
    RemoteUpdate( type );
  }
}

function updateProgress() {
  if (update_interval) clearInterval(update_interval);
  update_interval = setInterval(() => {
    if (progress < 90) {
      progress += 5;
      progressBar.style.width = progress + "%";
	  document.getElementById("updatestatus").innerText = t("lbl_update_start");
    } else {
      clearInterval(update_interval);
      update_interval = null;
      checkESPOnline();
    }
  }, 1000);
}

function checkESPOnline() {
  document.getElementById("updatestatus").innerText = t("lbl_update_wait");
  update_reconnected = false;
  if (checkInterval) clearInterval(checkInterval);
  checkInterval = setInterval(() => {
    objDAL.refreshTime(); // ergens anders zet je update_reconnected = true
    if (update_reconnected) {
      clearInterval(checkInterval);
      checkInterval = null;
      progressBar.style.width = "100%";
      document.getElementById("updatestatus").innerText = t("lbl_update_done");
    }
  }, 2000);
}
  
  //get new devinfo==============================================================  
  function refreshDevInfo()
  { 
	objDAL.refreshDeviceInformation();
  } // refreshDevInfo()

//============================================================================  
  function FrontendConfig()  {
    console.log("Read Frontend config");
	Spinner(true);
	fetch(APIGW+"../Frontend.json", {"setTimeout": 5000})
      .then(response => response.json())
      .then(json => {
		  data = json;
          AMPS = asNumber(json.Fuse);
          if (Number.isNaN(AMPS)) AMPS = 35;
          ShowVoltage = asBoolean(json.ShowVoltage, true);
          UseCDN = asBoolean(json.cdn, true);
          Injection = asBoolean(json.Injection, false);
          Phases = asNumber(json.Phases);
          if (Number.isNaN(Phases)) Phases = 1;
          HeeftGas = asBoolean(json.GasAvailable, false);
		  Act_Watt = asBoolean(json.Act_Watt, false);
		  AvoidSpikes = asBoolean(json.AvoidSpikes, false);
          IgnoreInjection = asBoolean(json.IgnoreInjection, false);
          IgnoreGas = asBoolean(json.IgnoreGas, false);
// 		  "i18n" in json ? setLocale(json.i18n) : setLocale("nl");

          for (let item in data) 
          {
//           	console.log("config item: " +item);
//           	console.log("config data[item].value: " +data[item].value);
            let tableRef = document.getElementById('frontendTable').getElementsByTagName('tbody')[0];
            if( ( document.getElementById("frontendTable_"+item)) == null )
            {
              let newRow   = tableRef.insertRow();
              newRow.setAttribute("id", "frontendTable_"+item, 0);
              // Insert a cell in the row at index 0
              let newCell  = newRow.insertCell(0);                  // name
              let newText  = document.createTextNode('');
              newCell.appendChild(newText);
              newCell  = newRow.insertCell(1);                      // humanName
              newCell.appendChild(newText);
              newCell  = newRow.insertCell(2);                      // value
              newCell.appendChild(newText);
            }
            tableCells = document.getElementById("frontendTable_"+item).cells;
            tableCells[0].innerHTML = item;
            tableCells[1].innerHTML = td(item);
            tableCells[2].innerHTML = data[item];
          }
         Spinner(false);
      }) //json
  }

  //============================================================================  
  function refreshTime( json ) {
  
	document.getElementById('theTime').classList.remove("afterglow");
	document.getElementById('theTime').innerHTML = json.time;//formatTimestamp(json.timestamp.value );
// 	console.log("time parsed .., data is ["+ JSON.stringify(json)+"]");
	//after reboot checks of the server is up and running and redirects to home
	if ((document.querySelector('#counter').textContent < 40) && (document.querySelector('#counter').textContent > 0)) window.location.replace("/");
	document.getElementById('theTime').offsetWidth;
	document.getElementById('theTime').classList.add("afterglow"); 

  } // refreshDevTime()
    
  //============================================================================  
  function refreshSmActual() {
	objDAL.refreshActual();
  }
  
  function ProcesActual(json){
    if (presentationType == "TAB")
      showActualTable(json);
    else{
      let hist = objDAL.getActualHistory();  
      copyActualHistoryToChart(hist);
      showActualGraph();
    }  
  }
  
// format a identifier
// input = "4530303433303036393938313736353137"
// output = "E0043006998176517"
function formatIdentifier(value) {
  let sTotal = "";
  for (let pos = 0; pos < value.length; pos += 2) {
    nVal = parseInt(value.substring(pos, pos + 2), 16);
    sTotal += String.fromCharCode(nVal);
  }
  return sTotal;
}

// format a timestamp 
// input "230125125424W" 
// output "2023-01-25 12:54:24 (dst=off)"
function formatTimestamp(value) {
  	const dst = value[12] == "S" ? "on" : "off";
	const year = "20" + value.slice(0, 2);  // "25" → 2025
	const month = value.slice(2, 4);        // "07"
	const day = value.slice(4, 6);          // "05"
	const hour = value.slice(6, 8);         // "14"
	const minute = value.slice(8, 10);      // "10"
	const second = value.slice(10, 12);     // "09"
	
	// Format and return
	return `${year}-${month}-${day} ${hour}:${minute}:${second}`; //   +(dst=" + dst + ")";
};

//parse the value from electricity_failure_log into json with timestamp and duration
//input= ""
//output = ""
function parseFailureLog(value) {
  let failures = [];
  //remove start '(' and end ')'
  let v = value.slice(1, -1);
  let items = v.split(")(");
  let count = items[0];
  let id = items[1]; //not used for now
  for (let i = 0; i < count; i++) {
    let timestamp = items[2 + (i * 2)];
    let duration = items[3 + (i * 2)];
    let failure = { 'timestamp': timestamp, 'duration': duration };
    failures.push(failure);
  }
  return failures;
};

//format duration (in sec)
//input: 3600
//output: 1 hours
function formatDuration(value) {
  let t = "";
  let sMIN = value / 60;
  let sHRS = value / 3600;
  if (Math.floor(sHRS) == 0) {
    if (Math.floor(sMIN) == 0) {
      t = value + " sec";
    }
    else
      t = sMIN.toFixed(2) + " min";
  }
  else
    t = sHRS.toFixed(2) + " hours";
  return t;
}

//format the complete failure log
//input:   
/*  
  restored on 2017-11-07 01:35:57 (dst=off) after 8.55 min downtime.
  restored on 2018-08-06 15:54:35 (dst=on) after 1.24 hours downtime.
  restored on 2018-11-17 12:55:23 (dst=off) after 23.57 min downtime.
  restored on 2021-06-02 10:03:32 (dst=on) after 50.12 min downtime.
  restored on 2022-12-24 03:36:26 (dst=off) after 4.46 hours downtime.
*/
function formatFailureLog(svalue) {
  let t = "";
  failures = parseFailureLog(svalue);
  for (pos in failures) {
    failure = failures[pos];
    tst = formatTimestamp(failure.timestamp);
    dur = formatDuration(parseInt(failure.duration.slice(0, -2)));
    sLine = "restored on " + tst + " after " + dur + " downtime.<br>";
    t += sLine;
  }
  return t;
}

  //parse the fields of the SM
  function parseSmFields(data)
  {
    //console.log("parsed .., fields is ["+ JSON.stringify(data)+"]");
    const decodeEquipmentIds = Meter_Source !== "HAN";
    for (let item in data) 
    {
      if (item.charAt(0) === "_") continue;

      console.log("fields item: " +item);
      console.log("fields data[item].value: " +data[item].value);

      data[item].humanName = td(item);
      //get tableref
      let tableRef = document.getElementById('fieldsTable').getElementsByTagName('tbody')[0];
      //create row if not exist
      if( ( document.getElementById("fieldsTable_"+item)) == null )
      {
        let newRow   = tableRef.insertRow();
        newRow.setAttribute("id", "fieldsTable_"+item, 0);
        // Insert a cell in the row at index 0
        let newCell  = newRow.insertCell(0);                  // name
        let newText  = document.createTextNode('');
        newCell.appendChild(newText);
        newCell  = newRow.insertCell(1);                      // humanName
        newCell.appendChild(newText);
        newCell  = newRow.insertCell(2);                      // value
        newCell.appendChild(newText);
        newCell  = newRow.insertCell(3);                      // unit
        newCell.appendChild(newText);
      }
      //get ref to tablecells
      tableCells = document.getElementById("fieldsTable_"+item).cells;

      //fill cells
      tableCells[0].innerHTML = item;
      tableCells[1].innerHTML = data[item].humanName;
      switch (item) {
        case 'electricity_failure_log':          
          tableCells[0].setAttribute("style", "vertical-align: top");
          tableCells[1].setAttribute("style", "vertical-align: top");
          tableCells[2].innerHTML = formatFailureLog(data[item].value);
          break;

        case 'timestamp':
        case 'water_delivered_ts':
        case 'gas_delivered_timestamp':
          tableCells[2].innerHTML = formatTimestamp(data[item].value);
          break;

        case 'equipment_id':
        case 'mbus1_equipment_id_tc':
        case 'mbus2_equipment_id_tc':
        case 'mbus3_equipment_id_tc':
        case 'mbus4_equipment_id_tc':
        case 'mbus1_equipment_id_ntc':
        case 'mbus2_equipment_id_ntc':
        case 'mbus3_equipment_id_ntc':
        case 'mbus4_equipment_id_ntc':
          tableCells[2].innerHTML = decodeEquipmentIds ? formatIdentifier(data[item].value) : data[item].value;
          break;

        default:
          tableCells[2].innerHTML = data[item].value;
      }
      if (data[item].hasOwnProperty('unit')) {
        tableCells[2].style.textAlign = "right";              // value
        tableCells[3].style.textAlign = "center";             // unit
        tableCells[3].innerHTML = data[item].unit;
      }
    }
  }
  
  //============================================================================  
  function refreshSmFields(json)
  { 
	objDAL.refreshFields();  

  };  // refreshSmFields()
  
  //============================================================================  
  //calculate the diffs, sums and costs per entry
  function expandData(data)
  {
    let i;
    let slotbefore;
    for (let x=data.data.length + data.actSlot; x > data.actSlot; x--)
    {
      i = x % data.data.length;
      slotbefore = math.mod(i-1, data.data.length);
      let costsED = 0;
      let costsER = 0;
      if ( x != data.actSlot 	)
      {
        //avoid gaps and spikes
        if ( AvoidSpikes ) {
        	if ( Dongle_Config == "p1-q" ) { 
        		if ( data.data[slotbefore].values[4] == 0 ) data.data[slotbefore].values = data.data[i].values; 
        	}
        	else  if ( data.data[slotbefore].values[0] == 0 ) data.data[slotbefore].values = data.data[i].values;
        }
        
		data.data[i].EEYY = {};
		data.data[i].MM   = {};
		data.data[i].EEYY = parseInt("20"+data.data[i].date.substring(0,2));
		data.data[i].MM   = parseInt(data.data[i].date.substring(2,4));
        
        //simple differences
        data.data[i].p_edt1= (data.data[i].values[0] - data.data[slotbefore].values[0]);
        data.data[i].p_edt2= (data.data[i].values[1] - data.data[slotbefore].values[1]);
        data.data[i].p_ert1= (data.data[i].values[2] - data.data[slotbefore].values[2]);
        data.data[i].p_ert2= (data.data[i].values[3] - data.data[slotbefore].values[3]);
		data.data[i].p_gd  = (data.data[i].values[4] - data.data[slotbefore].values[4]);
        data.data[i].water = (data.data[i].values[5] - data.data[slotbefore].values[5]);
        data.data[i].solar = (data.data[i].values.length > 6) ? Number(data.data[i].values[6]) : -1;

        //sums of T1 & T2
		data.data[i].p_ed  = (data.data[i].p_edt1 + data.data[i].p_edt2);
        data.data[i].p_er  = (data.data[i].p_ert1 + data.data[i].p_ert2);
        
        //sums in watts
        data.data[i].p_edw = (data.data[i].p_ed * 1000);
        data.data[i].p_erw = (data.data[i].p_er * 1000);        

        //-- calculate costs
        costsED  = ( data.data[i].p_edt1 * ed_tariff1 ) + ( data.data[i].p_edt2 * ed_tariff2 );
        costsER  = ( data.data[i].p_ert1 * er_tariff1 ) + ( data.data[i].p_ert2 * er_tariff2 );
        data.data[i].costs_e = costsED - costsER;
        
        //-- add Gas Delivered costs
        data.data[i].costs_g = HeeftGas ? ( data.data[i].p_gd  * gd_tariff ) : 0;

        //-- compute network costs
        data.data[i].costs_nw = (electr_netw_costs + (HeeftGas ? gas_netw_costs : 0)) * 1.0;

        //-- compute total costs
        data.data[i].costs_tt = ( (data.data[i].costs_e + data.data[i].costs_g + data.data[i].costs_nw) * 1.0);
      }
      else
      {
        costs = 0;
        data.data[i].p_edt1    = data.data[i].values[0];
        data.data[i].p_edt2    = data.data[i].values[1];
        data.data[i].p_ert1    = data.data[i].values[2];        
        data.data[i].p_ert2    = data.data[i].values[3];        
        data.data[i].p_gd      = data.data[i].values[4];
		data.data[i].water     = data.data[i].values[5];
        data.data[i].solar     = (data.data[i].values.length > 6) ? Number(data.data[i].values[6]) : -1;
        data.data[i].p_ed      = (data.data[i].p_edt1 + data.data[i].p_edt2);
        data.data[i].p_er      = (data.data[i].p_ert1 + data.data[i].p_ert2);        
        data.data[i].p_edw     = (data.data[i].p_ed * 1000);
        data.data[i].p_erw     = (data.data[i].p_er * 1000);
        data.data[i].costs_e   = 0.0;
        data.data[i].costs_g   = 0.0;
        data.data[i].costs_nw  = 0.0;
        data.data[i].costs_tt  = 0.0;
      }
    } // for i ..
  } // expandData()

  //limit every value in the array to n decimals
  //toFixed() will convert all into strings, so also add  convertion back to Number
  function applyArrayFixedDecimals(data, decimals){
    for( let i=0; i<data.length; i++){
      if( !isNaN(data[i]) ) data[i] = Number(data[i].toFixed(decimals));
    }
  }
  
  //============================================================================  
  //display an alert message at the top
  function alert_message(msg) {
  	if (msg==""){
  		document.getElementById('messages').style="display:none";
	
  	} else {  
	document.getElementById('messages').style="display:block";
	document.getElementById('messages').innerHTML = msg;
	} 
  }
//============================================================================  
function refreshHistData(type) {
	console.log(`refresh${type}`);
	let data = objDAL[`get${type}`]();
	if (!hasHistoryData(data)) {
		console.log(`refresh${type} : NO DATA`);
		return;
	}

	switch (type) {
		case "Hours":
			if (activeTab === "bHoursTab") {
				presentationType === "TAB" ? showHistTable(data, "Hours") : showHistGraph(data, "Hours");
			}
			break;

		case "Days":
			if (activeTab === "bDaysTab") {
				presentationType === "TAB" ? showHistTable(data, "Days") : showHistGraph(data, "Days");
			}
			// Extra logica voor dashboard
			let act_slot = data.actSlot;
			for (let i = 0; i < 4; i++) {
				let tempslot = math.mod(act_slot - i, data.data.length);
				let values = data.data[tempslot].values;
				hist_arrG[i] = values[4];
				hist_arrW[i] = values[5];
				hist_arrPa[i] = values[0] + values[1];
				hist_arrPi[i] = values[2] + values[3];
			}
			DailyHistoryReady = true;
			if (activeTab === "bDashTab" && objDAL.getFields()) {
				refreshDashboard(objDAL.getFields());
			}
			break;

		case "Months":
			if (activeTab === "bMonthsTab") {
				presentationType === "TAB" ? showMonthsHist(data) : showMonthsGraph(data, "Months");
			}
			break;
	}
}

function CopyTelegram(){
  const element = document.getElementById("TelData");
  if (!element) return;

  const range = document.createRange();
  range.selectNodeContents(element);

  const selection = window.getSelection();
  selection.removeAllRanges();
  selection.addRange(range);

  try {
    const success = document.execCommand("copy");
    if (success) {
      alert("Copied the text: \n" + element.textContent.trim());
    } 
  } catch (err) {console.error("Copy error:", err);}

  selection.removeAllRanges();
}

  //============================================================================  
  function refreshSmTelegram(json)
  { 
  	objDAL.refreshTelegram();
  } // refreshSmTelegram()

  //format a value based on the locale of the client
function formatValue(value)
  {
    let t="";
    if (!isNaN(value) ) 
      t = Number(value).toLocaleString('nl-NL', {minimumFractionDigits: 3, maximumFractionDigits: 3} );
    else 
      t = value;
    return t;
  }

  //============================================================================  
  function showActualTable(data)
  { 
    if (activeTab != "bActualTab") return;

    console.log("showActual()");

    for (let item in data) 
    {
      //skip if dongle=Q
     	if ( (item == "gas_delivered_timestamp") && (Dongle_Config == "p1-q") ) continue;
    	
      //ensure tablerow exists
      data[item].humanName = td(item);
      let tableRef = document.getElementById('actualTable').getElementsByTagName('tbody')[0];
      let itemid = "actualTable_"+item;
      if( ( document.getElementById(itemid)) == null )
      {
        createTableRowWithCells(tableRef, itemid, 3, '');
      }
      //get ref to tablecells
      tableCells = document.getElementById(itemid).cells;
      
      //fill cells
      switch(item)
      {
        case "gas_delivered":
          tableCells[0].innerHTML = data[item].humanName;
          tableCells[2].innerHTML = data[item].unit;
          if(Dongle_Config == "p1-q"){
            tableCells[0].innerHTML = "Warmtemeter stand";            
      	    tableCells[2].innerHTML = "GJ";
          }
          tableCells[1].innerHTML = formatValue(data[item].value);
          break;
        
        case "gas_delivered_timestamp":
        case "timestamp":
          tableCells[0].innerHTML = data[item].humanName;
          tableCells[1].innerHTML = formatTimestamp(data[item].value);
          //timestamps have no units
          break;

        default:
          tableCells[0].innerHTML = data[item].humanName;
          tableCells[1].innerHTML = formatValue(data[item].value);
          if (data[item].hasOwnProperty('unit')) tableCells[2].innerHTML = data[item].unit;
      }//endswitch
    }

    //--- hide canvas
    hideAllCharts();
    //--- show table
    document.getElementById("actual").style.display    = "block";

  } // showActualTable()

  function hasSolarHistory(data)
  {
    if (!data || !Array.isArray(data.data)) return false;
    return data.data.some(entry =>
      Array.isArray(entry.values) &&
      entry.values.length > 6 &&
      !isNaN(Number(entry.values[6])) &&
      Number(entry.values[6]) > 0
    );
  }
    
      
  //============================================================================  
  function showHistTable(data, type)
  { 
    console.log("showHistTable("+type+")");
    if (!hasHistoryData(data)) return;
    const solarVisible = (type == "Days") && hasSolarHistory(data);
    // the last element has the metervalue, so skip it
    let stop = data.actSlot + 1;
    let start = data.data.length + data.actSlot ;
    let index;
    	//console.log("showHistTable start: "+start);
    	//console.log("showHistTable stop: "+stop);
    let tableRef = document.getElementById('last'+type+'Table');
	tableRef.getElementsByTagName('tbody')[0].innerHTML = ''; //clear tbody content

    
    for (let i=start; i>stop; i--)
    {  
      index = i % data.data.length;
      let tref = tableRef.getElementsByTagName('tbody')[0];
      let itemid = type+"Table_"+type+"_R"+index;
      let nCells = 5;
      if (type == "Days") nCells += 2;
      createTableRowWithCells( tref, itemid, nCells, '-');

        /*
        newRow.setAttribute("id", itemid, 0);
        let newCell  = newRow.insertCell(0);
        let newText  = document.createTextNode('-');
        newCell.appendChild(newText);
        newCell  = newRow.insertCell(1);
        newCell.appendChild(newText);
        newCell  = newRow.insertCell(2);
        newCell.appendChild(newText);
        newCell  = newRow.insertCell(3);
        newCell.appendChild(newText);
		newCell  = newRow.insertCell(4);
        newCell.appendChild(newText);        if (type == "Days")
        {
          newCell  = newRow.insertCell(5);
          newCell.appendChild(newText);
        }
        }
      	*/
      tableCells = document.getElementById(itemid).cells;
      tableCells[0].innerHTML = formatDate(type, data.data[index].date);
      
      if (data.data[index].p_edw >= 0) tableCells[1].innerHTML = Number(data.data[index].p_edw).toLocaleString( 'nl-NL' );
      
      
      else tableCells[1].innerHTML = "-";

      if (data.data[index].p_erw >= 0) tableCells[2].innerHTML = Number(data.data[index].p_erw).toLocaleString( 'nl-NL' );
      else tableCells[2].innerHTML = "-";

      if (data.data[index].p_gd >= 0) tableCells[3].innerHTML = Number(data.data[index].p_gd).toLocaleString( 'nl-NL' );
      if (data.data[index].water >= 0) tableCells[4].innerHTML = Number(data.data[index].water).toLocaleString( 'nl-NL' );

      if (type == "Days") tableCells[5].innerHTML = Number( (data.data[index].costs_e + data.data[index].costs_g) * 1.0 ).toLocaleString('nl-NL', {minimumFractionDigits: 2, maximumFractionDigits: 2} );
      if (type == "Days") {
        const solarValue = data.data[index].solar;
        tableCells[6].innerHTML = solarValue >= 0
          ? Number(solarValue).toLocaleString('nl-NL', {minimumFractionDigits: 3, maximumFractionDigits: 3})
          : "-";
      }
      
    };

    //--- hide canvas
    hideAllCharts();
	
    //--- show table
    document.getElementById("lastHours").style.display = "block";
    document.getElementById("lastDays").style.display  = "block";
    if (type == "Hours") show_hide_column('lastHoursTable', 4, HeeftWater);
    if (type == "Days") show_hide_column('lastDaysTable', 4, HeeftWater);
    if (type == "Days") show_hide_column2('lastDaysTable', 6, solarVisible);

  } // showHistTable()

//============================================================================  
  function showMonthsHist(data)
  { 
    //console.log("now in showMonthsHist() ..");
    if (document.getElementById('mCOST').checked) {
	    showMonthsCosts(data);
    	return;
    }
    
    let start = data.data.length + data.actSlot ; //  maar 1 jaar ivm berekening jaar verschil
    let stop = start - 12;
    let i;
    let slotyearbefore = 0;
  
    for (let index=start; index>stop; index--)
    {  i = index % data.data.length;
      	slotyearbefore = math.mod(i-12,data.data.length);
      let tableRef = document.getElementById('lastMonthsTable').getElementsByTagName('tbody')[0];
      if( ( document.getElementById("lastMonthsTable_R"+i)) == null )
      {
        createTableRowWithCells(tableRef, "lastMonthsTable_R"+i, 17, '-' );
      }
      let mmNr = parseInt(data.data[i].date.substring(2,4), 10);

      tableCells = document.getElementById("lastMonthsTable_R"+i).cells;
      //fill with default values
      for (y=2; y <= 12; y=y+2){ tableCells[y].innerHTML = "-"; }
      
      tableCells[0].innerHTML = t("mth-"+mmNr);//monthNames[mmNr];                           // maand
      
      tableCells[1].innerHTML = "20"+data.data[i].date.substring(0,2);          // jaar
	  if (data.data[i].p_ed >= 0) tableCells[2].innerHTML = Number(data.data[i].p_ed).toLocaleString('nl-NL', {minimumFractionDigits: 3, maximumFractionDigits: 3} );
      
      tableCells[3].innerHTML = "20"+data.data[slotyearbefore].date.substring(0,2);       // jaar
      if (data.data[slotyearbefore].p_ed >= 0) tableCells[4].innerHTML = Number(data.data[slotyearbefore].p_ed).toLocaleString('nl-NL', {minimumFractionDigits: 3, maximumFractionDigits: 3} );                      // verbruik
      
      tableCells[5].innerHTML = "20"+data.data[i].date.substring(0,2);          // jaar
      if (data.data[i].p_er >= 0) tableCells[6].innerHTML = Number(data.data[i].p_er).toLocaleString('nl-NL', {minimumFractionDigits: 3, maximumFractionDigits: 3} );                         // opgewekt
      
      tableCells[7].innerHTML = "20"+data.data[slotyearbefore].date.substring(0,2);       // jaar
      if (data.data[slotyearbefore].p_er >= 0) tableCells[8].innerHTML = Number(data.data[slotyearbefore].p_er).toLocaleString('nl-NL', {minimumFractionDigits: 3, maximumFractionDigits: 3} );                      // opgewekt
      
      tableCells[9].innerHTML = "20"+data.data[i].date.substring(0,2);          // jaar
      if (data.data[i].p_gd >= 0) tableCells[10].innerHTML = Number(data.data[i].p_gd).toLocaleString('nl-NL', {minimumFractionDigits: 3, maximumFractionDigits: 3} );                        // gas
      
      tableCells[11].innerHTML = "20"+data.data[slotyearbefore].date.substring(0,2);      // jaar
      if (data.data[slotyearbefore].p_gd >= 0) tableCells[12].innerHTML = Number(data.data[slotyearbefore].p_gd).toLocaleString('nl-NL', {minimumFractionDigits: 3, maximumFractionDigits: 3} );                       // gas
      
		tableCells[13].innerHTML = "20"+data.data[i].date.substring(0,2);          // jaar
      if (data.data[i].water >= 0) tableCells[14].innerHTML = Number(data.data[i].water).toLocaleString('nl-NL', {minimumFractionDigits: 3, maximumFractionDigits: 3} );  
      else tableCells[14].innerHTML = "-";
      
      tableCells[15].innerHTML = "20"+data.data[slotyearbefore].date.substring(0,2);      // jaar
      if (data.data[slotyearbefore].water >= 0) tableCells[16].innerHTML = Number(data.data[slotyearbefore].water).toLocaleString('nl-NL', {minimumFractionDigits: 3, maximumFractionDigits: 3} ); 
      else tableCells[16].innerHTML = "-";
      
    };
    
    //--- hide canvas
    hideAllCharts();
    //--- show table
	if (Dongle_Config == "p1-q") {
  		show_hide_column2('lastMonthsTable', 1,false);
		show_hide_column2('lastMonthsTable', 2,false);
		show_hide_column2('lastMonthsTable', 3,false);
		show_hide_column2('lastMonthsTable', 4,false);
		show_hide_column2('lastMonthsTable', 5,false);
		show_hide_column2('lastMonthsTable', 6,false);
		show_hide_column2('lastMonthsTable', 7,false);
		show_hide_column2('lastMonthsTable', 8,false);
		show_hide_column2('lastMonthsTable', 13,false);
		show_hide_column2('lastMonthsTable', 14,false);
		show_hide_column2('lastMonthsTable', 15,false);
		show_hide_column2('lastMonthsTable', 16,false);
		
		let tbl  = document.getElementById('lastMonthsTable');
    	let rows = tbl.getElementsByTagName('tr');
		rows[0].style.display = "none";
	}	
    const waterHeader = document.querySelector('#lastMonthsTable thead tr:first-child th:last-child');
    if (waterHeader) waterHeader.style.display = HeeftWater ? "" : "none";
    [13, 14, 15, 16].forEach(col => show_hide_column('lastMonthsTable', col, HeeftWater));
    document.getElementById("lastMonths").style.display = "block";

  } // showMonthsHist()

  function createTableRowWithCells(tableRef, itemid, cellcount, content)
  {
    let newText  = document.createTextNode(content);
    let newRow   = tableRef.insertRow();
    newRow.setAttribute("id", itemid, 0);
    for( let i=0; i<cellcount; i++)
    {
      let newCell  = newRow.insertCell(i);
      newCell.appendChild(newText);
    }
  }
  
  function formatValueLocale(val, minf, maxf)
  {
    return Number(val).toLocaleString('nl-NL', {minimumFractionDigits: minf, maximumFractionDigits: maxf} );
  }
  
  //============================================================================  
  function showMonthsCosts(data)
  { 
    console.log("now in showMonthsCosts() ..");
    let totalCost   = 0;
    let totalCost_1 = 0;
    let start = data.data.length + data.actSlot ; //  maar 1 jaar ivm berekening jaar verschil
    let stop = start - 12;
    let i;
    let slotyearbefore = 0;
  
    for (let index=start; index>stop; index--)
    {  i = index % data.data.length;
      	slotyearbefore = math.mod(i-12,data.data.length);
      //console.log("showMonthsHist(): data["+i+"] => data["+i+"].name["+data[i].recid+"]");
      let tableRef = document.getElementById('lastMonthsTableCosts').getElementsByTagName('tbody')[0];

      //ensure tablerow exists
      let itemid = "lastMonthsTableCosts_R"+i;
      if( ( document.getElementById(itemid)) == null )
      {
        createTableRowWithCells(tableRef, itemid, 11, '-');       
      }
      let mmNr = parseInt(data.data[i].date.substring(2,4), 10);

      //get ref to cells
      tableCells = document.getElementById(itemid).cells;

      //fill table
      tableCells[0].style.textAlign = "right";
      tableCells[0].innerHTML = t("mth-"+mmNr);//monthNames[mmNr];                           // maand
      
      tableCells[1].style.textAlign = "center";
      tableCells[1].innerHTML = "20"+data.data[i].date.substring(0,2);          // jaar
      tableCells[2].style.textAlign = "right";
      tableCells[2].innerHTML = (data.data[i].costs_e *1).toFixed(2);            // kosten electra
      tableCells[3].style.textAlign = "right";
      tableCells[3].innerHTML = (data.data[i].costs_g *1).toFixed(2);            // kosten gas
      tableCells[4].style.textAlign = "right";
      tableCells[4].innerHTML = (data.data[i].costs_nw *1).toFixed(2);           // netw kosten
      tableCells[5].style.textAlign = "right";
      tableCells[5].style.fontWeight = 'bold';
      tableCells[5].innerHTML = "€ " + (data.data[i].costs_tt *1).toFixed(2);    // kosten totaal
      //--- omdat de actuele maand net begonnen kan zijn tellen we deze
      //--- niet mee, maar tellen we de laatste maand van de voorgaand periode
      if (i > 0)
            totalCost += data.data[i].costs_tt;
      else  totalCost += data.data[slotyearbefore].costs_tt;

      tableCells[6].style.textAlign = "center";
      tableCells[6].innerHTML = "20"+data.data[slotyearbefore].date.substring(0,2);         // jaar
      tableCells[7].style.textAlign = "right";
      tableCells[7].innerHTML = (data.data[slotyearbefore].costs_e *1).toFixed(2);           // kosten electra
      tableCells[8].style.textAlign = "right";
      tableCells[8].innerHTML = (data.data[slotyearbefore].costs_g *1).toFixed(2);           // kosten gas
      tableCells[9].style.textAlign = "right";
      tableCells[9].innerHTML = (data.data[slotyearbefore].costs_nw *1).toFixed(2);          // netw kosten
      tableCells[10].style.textAlign = "right";
      tableCells[10].style.fontWeight = 'bold';
      tableCells[10].innerHTML = "€ " + (data.data[slotyearbefore].costs_tt *1).toFixed(2);  // kosten totaal
      totalCost_1 += data.data[slotyearbefore].costs_tt;

    };

    if( ( document.getElementById("periodicCosts")) == null )
    {
      let newRow   = tableRef.insertRow();                                // voorschot regel
      newRow.setAttribute("id", "periodicCosts", 0);
      // Insert a cell in the row at index 0
      let newCell  = newRow.insertCell(0);                                // maand
      let newText  = document.createTextNode('-');
      newCell.appendChild(newText);
      newCell  = newRow.insertCell(1);              // description
      if ( Dongle_Config == "p1-q" ) newCell.setAttribute("colSpan", "3");
      else newCell.setAttribute("colSpan", "4");
      newCell.appendChild(newText);
      newCell  = newRow.insertCell(2);              // voorschot
      newCell.appendChild(newText);
      newCell  = newRow.insertCell(3);              // description
      if ( Dongle_Config == "p1-q" ) newCell.setAttribute("colSpan", "3");
      else newCell.setAttribute("colSpan", "4");
      newCell.appendChild(newText);
      newCell  = newRow.insertCell(4);              // voorschot
      newCell.appendChild(newText);
    }
    tableCells = document.getElementById("periodicCosts").cells;
    tableCells[1].style.textAlign = "right";
    tableCells[1].innerHTML = t("lbl-advance");
    tableCells[2].style.textAlign = "right";
    tableCells[2].innerHTML = "€ " + (totalCost / 12).toFixed(2);
    tableCells[3].style.textAlign = "right";
    tableCells[3].innerHTML = t("lbl-advance");
    tableCells[4].style.textAlign = "right";
    tableCells[4].innerHTML = "€ " + (totalCost_1 / 12).toFixed(2);

    
    //--- hide canvas
    hideAllCharts();
    //--- show table
    if ( Dongle_Config == "p1-q" ){
		show_hide_column2('lastMonthsTableCosts', 2,false);
		show_hide_column2('lastMonthsTableCosts', 7,false);	
    }
		
    if (document.getElementById('mCOST').checked)
    {
      document.getElementById("lastMonthsTableCosts").style.display = "block";
      document.getElementById("lastMonthsTable").style.display = "none";
    }
    else
    {
      document.getElementById("lastMonthsTable").style.display = "block";
      document.getElementById("lastMonthsTableCosts").style.display = "none";
    }
    document.getElementById("lastMonths").style.display = "block";

  } // showMonthsCosts()

  
  //============================================================================  
  function ParseDevSettings()
  { 
		let json = objDAL.getDeviceSettings();
		if ( json == "" ) return;
        console.log("ParseDevSettings: parsed .., data is ["+ JSON.stringify(json)+"]");
        "ed_tariff1" in json ? ed_tariff1 = json.ed_tariff1.value : ed_tariff1 = 0;
        "ed_tariff2" in json ? ed_tariff2 = json.ed_tariff2.value : ed_tariff2 = 0;
        "er_tariff1" in json ? er_tariff1 = json.er_tariff1.value : er_tariff1 = 0;
        "er_tariff2" in json ? er_tariff2 = json.er_tariff2.value : er_tariff2 = 0;
        "gd_tariff" in json ? gd_tariff = json.gd_tariff.value : gd_tariff = 0;
	  	"conf" in json ? Dongle_Config = json.conf : Dongle_Config = "";
        "electr_netw_costs" in json ? electr_netw_costs = json.electr_netw_costs.value : electr_netw_costs = 0;
        "eid-enabled" in json ? eid_enabled = json["eid-enabled"]: eid_enabled = false;
        "dev-pairing" in json ? pairing_enabled = json["dev-pairing"]: pairing_enabled = false;
        "eid-planner" in json ? eid_planner_enabled = json["eid-planner"]: eid_planner_enabled = false;
        "ota_url" in json ? ota_url = json.ota_url.value: ota_url = "ota.smart-stuff.nl/v5/";

        if ( eid_enabled ) document.getElementById("bEid").style.display = "block"; else document.getElementById("bEid").style.display = "none";
        if ( eid_planner_enabled ) document.getElementById("dash_eid").style.display = "flex"; else document.getElementById("dash_eid").style.display = "none";
        if ( pairing_enabled ) document.getElementById("bNRGM").style.display = "block"; else document.getElementById("bNRGM").style.display = "none";
                
        gas_netw_costs = json.gas_netw_costs.value;
        hostName = json.hostname.value;
        EnableHist =  "hist" in json ? json.hist : true;

  } // ParseDevSettings()
  
    
  //============================================================================  
  function setPresentationType(pType) {
    if (pType == "GRAPH") {
      console.log("Set presentationType to GRAPHICS mode!");
      presentationType = pType;
      document.getElementById('aGRAPH').checked = true;
      document.getElementById('aTAB').checked   = false;
      initActualGraph();
      document.getElementById('hGRAPH').checked = true;
      document.getElementById('hTAB').checked   = false;
      document.getElementById('dGRAPH').checked = true;
      document.getElementById('dTAB').checked   = false;
      document.getElementById('mGRAPH').checked = true;
      document.getElementById('mTAB').checked   = false;
      document.getElementById('mCOST').checked  = false;
      document.getElementById("lastMonthsTable").style.display      = "block";
      document.getElementById("lastMonthsTableCosts").style.display = "none";

    } else if (pType == "TAB") {
      console.log("Set presentationType to Tabular mode!");
      presentationType = pType;
      document.getElementById('aTAB').checked   = true;
      document.getElementById('aGRAPH').checked = false;
      document.getElementById('hTAB').checked   = true;
      document.getElementById('hGRAPH').checked = false;
      document.getElementById('dTAB').checked   = true;
      document.getElementById('dGRAPH').checked = false;
      document.getElementById('mTAB').checked   = true;
      document.getElementById('mGRAPH').checked = false;
      document.getElementById('mCOST').checked  = false;

    } else {
      console.log("setPresentationType to ["+pType+"] is quite shitty! Set to TAB");
      presentationType = "TAB";
    }

//    document.getElementById("APIdocTab").style.display = "none";

    if (activeTab == "bActualTab")  refreshSmActual();
    if (activeTab == "bHoursTab")   refreshHistData("Hours");
    if (activeTab == "bDaysTab")    refreshHistData("Days");
    if (activeTab == "bMonthsTab")  refreshHistData("Months");

  } // setPresenationType()
  
    
  //============================================================================  
  function setMonthTableType() {
    console.log("Set Month Table Type");
    if (presentationType == 'GRAPH') 
    {
      //reset checkbox when you select a graph
      document.getElementById('mCOST').checked = false;
      return;
    }
    if (document.getElementById('mCOST').checked)
    {
      document.getElementById("lastMonthsTableCosts").style.display = "block";
      document.getElementById("lastMonthsTable").style.display      = "none";
      refreshHistData("Months");

    }
    else
    {
      document.getElementById("lastMonthsTable").style.display      = "block";
      document.getElementById("lastMonthsTableCosts").style.display = "none";
    }
    document.getElementById('lastMonthsTableCosts').getElementsByTagName('tbody').innerHTML = "";
      
  } // setMonthTableType()


let _settingsSubTabsInited = false;

function initSettingsSubTabsOnce() {
  if (_settingsSubTabsInited) return;
  _settingsSubTabsInited = true;

  const root = document.getElementById("Settings");
  if (!root) return;

  const btns = root.querySelectorAll("#settings_subtabs .subtab-btn");
  const panes = root.querySelectorAll(".settings-pane");

  btns.forEach(btn => {
    btn.addEventListener("click", () => {
      btns.forEach(b => b.classList.remove("active"));
      panes.forEach(p => p.classList.remove("active"));

      btn.classList.add("active");
      const targetId = btn.getAttribute("data-target");
      const pane = document.getElementById(targetId);
      if (pane) pane.classList.add("active");
      if (targetId === "settings_modbus") refreshModbusMonitorView();
    });
  });
}

function splitSettingsUI() {
  const table   = document.getElementById("settings_table");
  const general = document.getElementById("settings_general");
  const tariff  = document.getElementById("settings_tariff");
  const mqtt    = document.getElementById("settings_mqtt");
  const modbus  = document.getElementById("settings_modbus");

  if ( !table || !general || !mqtt || !modbus || !tariff ) return;

  // velden op basis van "i" (dus zonder "settingR_")
  const MQTT_KEYS = new Set([
    "mqtt_enabled",
    "mqtt_tls",
    "mqtt_broker",
    "mqtt_broker_port",
    "mqtt_user",
    "mqtt_passwd",
    "mqtt_toptopic",
    "mqtt_interval",
    "act-json-mqtt",
	"ha_disc_enabl",
	"act-json-mqtt",
	"macid-topic"
  ]);

  const TARIFF_KEYS = new Set([
	"ed_tariff1",
	"ed_tariff2",
	"er_tariff1",
	"er_tariff2",
	"w_tariff",
	"gd_tariff",
	"electr_netw_costs",
	"water_netw_costs",
	"gas_netw_costs"
  ]);
  
  const MODBUS_KEYS = new Set([
    "mb_map",
    "mb_id",
    "mb_port",
    "mb_parity",
    "mb_baud",
    "mb_bits",
    "mb_stop",
    "modbus_mapping",
    "modbus_dev_id",
    "modbus_baud",
    "modbus_parity",
    "modbus_bits",
    "modbus_stop"
  ]);

  // alle bestaande rows (waar ze ook al staan) opnieuw indelen
  const rows = Array.from(document.querySelectorAll("#Settings .settingDiv"));

  rows.forEach(row => {
    const id = row.id || "";
    // id is "settingR_<key>"
    const key = id.startsWith("settingR_") ? id.substring("settingR_".length) : "";

    if (MQTT_KEYS.has(key)) mqtt.appendChild(row);
    else if (TARIFF_KEYS.has(key)) tariff.appendChild(row);
    else if (MODBUS_KEYS.has(key)) modbus.appendChild(row);
    else general.appendChild(row);
  });

  const mqttToggleRow = document.getElementById("settingR_mqtt_enabled");
  if (mqttToggleRow) mqtt.prepend(mqttToggleRow);
  const modbusMonitorCard = document.getElementById("modbus_monitor_card");
  if (modbusMonitorCard) modbus.appendChild(modbusMonitorCard);
  updateMQTTSettingsVisibility();
}

function updateMQTTSettingsVisibility() {
  const mqttEnabled = document.getElementById("setFld_mqtt_enabled");
  const mqttPane = document.getElementById("settings_mqtt");
  if (!mqttEnabled || !mqttPane) return;

  const showMQTTFields = mqttEnabled.checked;
  mqttPane.querySelectorAll(".settingDiv").forEach(row => {
    if (row.id === "settingR_mqtt_enabled") return;
    row.style.display = showMQTTFields ? "" : "none";
  });
}

function markDirty(el) {
  // Zet 'dirty' op de hele rij, dan kan CSS het type input bepalen
  const row = el.closest('.settingDiv');
  if (row) row.classList.add('is-dirty');
}

function clearDirtySettings(){
  document
    .querySelectorAll('#Settings .settingDiv.is-dirty')
    .forEach(row => row.classList.remove('is-dirty'));
}

function getModbusMonitorSetting() {
  return objDAL?.dev_settings?.mb_monitor;
}

function getModbusMonitorEnabled() {
  const setting = getModbusMonitorSetting();
  if (typeof setting === "boolean") return setting;
  if (setting && typeof setting === "object" && "value" in setting) return !!setting.value;
  return null;
}

function renderModbusMonitorData(json) {
  const body = document.getElementById("modbus_monitor_body");
  const empty = document.getElementById("modbus_monitor_empty");
  const wrap = document.getElementById("modbus_monitor_table_wrap");
  const tbody = document.querySelector("#modbus_monitor_table tbody");
  if (!body || !empty || !wrap || !tbody) return;

  if (!json?.enabled) {
    body.style.display = "none";
    wrap.style.display = "none";
    empty.style.display = "";
    tbody.innerHTML = "";
    return;
  }

  body.style.display = "";
  tbody.innerHTML = "";

  const rows = Array.isArray(json.data) ? json.data : [];
  if (rows.length === 0) {
    wrap.style.display = "none";
    empty.style.display = "";
    return;
  }

  empty.style.display = "none";
  wrap.style.display = "";

  rows.forEach(row => {
    const tr = document.createElement("tr");
    [
      row.timestamp ? formatTimestamp(row.timestamp) : "-",
      row.mode ?? "-",
      row.id ?? "-",
      row.fc ?? "-",
      row.address ?? "-",
      row.words ?? "-",
      row.result ?? "-"
    ].forEach(value => {
      const cell = document.createElement("td");
      cell.textContent = value;
      tr.appendChild(cell);
    });
    tbody.appendChild(tr);
  });
}

function refreshModbusMonitorData() {
  const enabled = getModbusMonitorEnabled();
  if (!enabled) {
    renderModbusMonitorData({ enabled: false, data: [] });
    return;
  }

  fetch("/api/v2/modbus/monitor")
    .then(response => response.json())
    .then(json => renderModbusMonitorData(json))
    .catch(error => console.error("refreshModbusMonitorData()", error));
}

function refreshModbusMonitorView() {
  const card = document.getElementById("modbus_monitor_card");
  const toggle = document.getElementById("modbus_monitor_toggle");
  const enabled = getModbusMonitorEnabled();
  if (!card || !toggle) return;

  if (enabled === null) {
    card.style.display = "none";
    return;
  }

  card.style.display = "";
  toggle.checked = enabled;
  renderModbusMonitorData({ enabled, data: [] });
  if (enabled) refreshModbusMonitorData();
}

function initModbusMonitorControls() {
  const toggle = document.getElementById("modbus_monitor_toggle");
  const refreshBtn = document.getElementById("modbus_monitor_refresh");
  const clearBtn = document.getElementById("modbus_monitor_clear");

  if (toggle && !toggle.dataset.bound) {
    toggle.dataset.bound = "1";
    toggle.addEventListener("change", () => {
      sendPostSetting("mb_monitor", toggle.checked);
      if (objDAL?.dev_settings) objDAL.dev_settings.mb_monitor = toggle.checked;
      refreshModbusMonitorView();
    });
  }

  if (refreshBtn && !refreshBtn.dataset.bound) {
    refreshBtn.dataset.bound = "1";
    refreshBtn.addEventListener("click", event => {
      event.preventDefault();
      refreshModbusMonitorData();
    });
  }

  if (clearBtn && !clearBtn.dataset.bound) {
    clearBtn.dataset.bound = "1";
    clearBtn.addEventListener("click", event => {
      event.preventDefault();
      fetch("/api/v2/modbus/monitor", { method: "POST" })
        .then(() => refreshModbusMonitorData())
        .catch(error => console.error("clear modbus monitor", error));
    });
  }
}
  
  //============================================================================  
  function refreshSettings()
  {
    console.log("refreshSettings() ..");

	data = objDAL.getDeviceSettings();
	for( let i in data )
	{
	  if ( i == "conf") continue;
	  if ( i == "mb_monitor") continue;
	  console.log("["+i+"]=>["+data[i].value+"]");
	  let settings = document.getElementById('settings_table');
	  if( ( document.getElementById("settingR_"+i)) == null )
	  {
	   
		let rowDiv = document.createElement("div");
		rowDiv.setAttribute("class", "settingDiv");
		rowDiv.setAttribute("id", "settingR_"+i);
		//--- field Name ---
		  let fldDiv = document.createElement("div");
			  if ( (i == "gd_tariff") && (Dongle_Config == "p1-q") ) fldDiv.textContent = "Warmte tarief (GJ)";
			  else if ( i == "gas_netw_costs") fldDiv.textContent = "Netwerkkosten Gas/maand";
			  else fldDiv.textContent = td(i);
			  rowDiv.appendChild(fldDiv);
		//--- input ---
		  let inputDiv = document.createElement("div");
			inputDiv.setAttribute("class", "settings-right");
			
			let sInput; // kan INPUT of SELECT worden
			const fldId = "setFld_" + i;
			
			// --- mb_map als dropdown ---
			if (i === "mb_map") {
			  const MAPS = [
				{ v: 0, t: "Default - uint32" },
				{ v: 1, t: "SDM630" },
				{ v: 2, t: "DTSU666" },
				{ v: 3, t: "Alfen / Socomec" },
				{ v: 4, t: "EM330" },
				{ v: 5, t: "ABB B21" },
				{ v: 6, t: "MX3xx" },
				{ v: 7, t: "Default 2 - floats" },
				{ v: 8, t: "KLEFR / INEPRO / Webasto Unite" },
				{ v: 9, t: "Phoenix Contact EEM-XM3xx" },
			  ];
			
			  const sel = document.createElement("select");
			  sel.setAttribute("id", fldId);
			
			  // huidige waarde uit settings
			  const cur = parseInt(data[i].value ?? "0", 10);
			
			  MAPS.forEach(o => {
				const opt = document.createElement("option");
				opt.value = String(o.v);
				opt.textContent = `${o.v} – ${o.t}`;
				if (o.v === cur) opt.selected = true;
				sel.appendChild(opt);
			  });
			
			  sInput = sel;
			}
			else if (i === "mimic") {
			  const MIMICS = [
				{ v: 0, t: t("Disabled") },
				{ v: 1, t: "HW P1" },
				{ v: 2, t: "Shelly Pro 3EM" }
			  ];

			  const sel = document.createElement("select");
			  sel.setAttribute("id", fldId);

			  const cur = parseInt(data[i].value ?? "0", 10);

			  MIMICS.forEach(o => {
				const opt = document.createElement("option");
				opt.value = String(o.v);
				opt.textContent = o.t;
				if (o.v === cur) opt.selected = true;
				sel.appendChild(opt);
			  });

			  sInput = sel;
			}
			else if (i === "mb_parity") {
			  const SERIAL_CONFIGS = [
				{ v: 134217744, t: "5N1" },
				{ v: 134217748, t: "6N1" },
				{ v: 134217752, t: "7N1" },
				{ v: 134217756, t: "8N1" },
				{ v: 134217776, t: "5N2" },
				{ v: 134217780, t: "6N2" },
				{ v: 134217784, t: "7N2" },
				{ v: 134217788, t: "8N2" },
				{ v: 134217746, t: "5E1" },
				{ v: 134217750, t: "6E1" },
				{ v: 134217754, t: "7E1" },
				{ v: 134217758, t: "8E1" },
				{ v: 134217778, t: "5E2" },
				{ v: 134217782, t: "6E2" },
				{ v: 134217786, t: "7E2" },
				{ v: 134217790, t: "8E2" },
				{ v: 134217747, t: "5O1" },
				{ v: 134217751, t: "6O1" },
				{ v: 134217755, t: "7O1" },
				{ v: 134217759, t: "8O1" },
				{ v: 134217779, t: "5O2" },
				{ v: 134217783, t: "6O2" },
				{ v: 134217787, t: "7O2" },
				{ v: 134217791, t: "8O2" }
			  ];

			  const sel = document.createElement("select");
			  sel.setAttribute("id", fldId);

			  const cur = parseInt(data[i].value ?? "0", 10);

			  SERIAL_CONFIGS.forEach(o => {
				const opt = document.createElement("option");
				opt.value = String(o.v);
				opt.textContent = o.t;
				if (o.v === cur) opt.selected = true;
				sel.appendChild(opt);
			  });

			  if (!SERIAL_CONFIGS.some(o => o.v === cur)) {
				const opt = document.createElement("option");
				opt.value = String(cur);
				opt.textContent = `Unknown (${cur})`;
				opt.selected = true;
				sel.appendChild(opt);
			  }

			  sInput = sel;
			}
			else {
			  // --- standaard INPUT gedrag ---
			  sInput = document.createElement("input");
			  sInput.setAttribute("id", fldId);
			
			  if (data[i].type === undefined) {
				sInput.setAttribute("type", "checkbox");
				sInput.checked = data[i];
				if (i === "dev-pairing") sInput.disabled = true;
				if (i === "eid-planner") sInput.disabled = true;
			  }
			  else {
				switch (data[i].type) {
				  case "s":
					sInput.setAttribute("type", "text");
					sInput.setAttribute("maxlength", data[i].max);
					sInput.setAttribute("placeholder", "<max " + data[i].max + ">");
					break;
				  case "f":
					sInput.setAttribute("type", "number");
					sInput.max = data[i].max;
					sInput.min = data[i].min;
					sInput.step = (data[i].min + data[i].max) / 1000;
					break;
				  case "i":
					sInput.setAttribute("type", "number");
					sInput.max = data[i].max;
					sInput.min = data[i].min;
					sInput.step = 1;
					break;
				}
				sInput.setAttribute("value", data[i].value);
			  }
			}
			
			// bestaande dirty-detectie behouden
			sInput.addEventListener("change", () => markDirty(sInput));
			sInput.addEventListener("input",  () => markDirty(sInput));
			if (i === "mqtt_enabled") {
			  sInput.addEventListener("change", updateMQTTSettingsVisibility);
			}
			
			inputDiv.appendChild(sInput);

			  
	  if( EnableHist || !( ["ed_tariff1","ed_tariff2", "er_tariff1","er_tariff2" ,"gd_tariff","electr_netw_costs","gas_netw_costs"].includes(i) ) ) {
			rowDiv.appendChild(inputDiv);
			settings.appendChild(rowDiv);
		};
	  }
	  else
	  {
// 		document.getElementById("setFld_"+i).style.background = "white";
		const existingField = document.getElementById("setFld_"+i);
		if (data[i].value === undefined) existingField.checked = data[i];
		else existingField.value = data[i].value;
	  }
	}

  initSettingsSubTabsOnce();
  splitSettingsUI();
  initModbusMonitorControls();
  refreshModbusMonitorView();

      
  } // refreshSettings()
  
  
  //============================================================================  
  function EditMonths() {	
  console.log("EditMonths()");
  
  const data = objDAL.getMonths();

  if (!data || !Array.isArray(data.data) || data.data.length === 0) {
    console.warn("EditMonths(): Data error");
    return;
  }

  if (typeof monthType === "undefined") {
    console.warn("EditMonths(): monthType undefined");
    return;
  }

  showMonthsV2(data, monthType);
  } // EditMonths()

  
  //============================================================================  
  function showMonthsV2(data, type)
  { 
    console.log("showMonthsV2("+type+")");

	const em = document.getElementById("editMonths");
	
	if (!em) {
		console.error("element not found");
		return;
	}
	if (!data || !Array.isArray(data.data)) {
		console.error("showMonthsV2(): data error");
		return;
	}

	em.innerHTML = "";
	
    let dlength = data.data.length;
    console.log("Now fill the DOM!");    
    console.log("data.data.length: " + dlength);
  
    for (let index=data.actSlot + dlength; index>data.actSlot; index--)
    {  let i = index % dlength;

      console.log("["+i+"] >>>["+data.data[i].EEYY+"-"+data.data[i].MM+"]");
      
      if( ( document.getElementById("em_R"+i)) == null )
      {
		console.log("==null");
        let div1 = document.createElement("div");
            div1.setAttribute("class", "settingDiv");
            div1.setAttribute("id", "em_R"+i);
            if (i == (data.data.length -1))  // last row
            {
            }
            let span2 = document.createElement("span");
              //--- create input for EEYY
              let sInput = document.createElement("INPUT");
              sInput.setAttribute("id", "em_YY_"+i);
              sInput.setAttribute("class", "tab_editMonth");
              sInput.setAttribute("type", "number");
              sInput.setAttribute("min", 2000);
              sInput.setAttribute("max", 2099);
              sInput.size              = 5;
              sInput.addEventListener('change',
                      function() { setNewValue(data, i, "EEYY", "em_YY_"+i); }, false);
              span2.appendChild(sInput);
              //--- create input for months
              sInput = document.createElement("INPUT");
              sInput.setAttribute("id", "em_MM_"+i);
              sInput.setAttribute("class", "tab_editMonth");
              sInput.setAttribute("type", "number");
              sInput.setAttribute("min", 1);
              sInput.setAttribute("max", 12);
              sInput.size              = 3;
              sInput.addEventListener('change',
                      function() { setNewValue(data, i, "MM", "em_MM_"+i); }, false);
              span2.appendChild(sInput);
              //--- create input for data column 1
              sInput = document.createElement("INPUT");
              sInput.setAttribute("id", "em_in1_"+i);
              sInput.setAttribute("class", "tab_editMonth");
              sInput.setAttribute("type", "number");
              sInput.setAttribute("step", 0.001);
              
              if (type == "ED")
              {
                sInput.addEventListener('change',
                    function() { setNewValue(data, i, "edt1", "em_in1_"+i); }, false );
              }
              else if (type == "ER")
              {
                sInput.addEventListener('change',
                    function() { setNewValue(data, i, "ert1", "em_in1_"+i); }, false);
              }
              else if (type == "GD")
              {
                sInput.addEventListener('change',
                    function() { setNewValue(data, i, "gdt", "em_in1_"+i); }, false);
              }
              
              span2.appendChild(sInput);
              //--- if not GD create input for data column 2
              if (type == "ED")
              {
                //console.log("add input for edt2..");
                let sInput = document.createElement("INPUT");
                sInput.setAttribute("id", "em_in2_"+i);
                sInput.setAttribute("class", "tab_editMonth");        
                sInput.setAttribute("type", "number");
                sInput.setAttribute("step", 0.001);
                sInput.addEventListener('change',
                      function() { setNewValue(data, i, "edt2", "em_in2_"+i); }, false);
                span2.appendChild(sInput);
              }
              else if (type == "ER")
              {
                //console.log("add input for ert2..");
                let sInput = document.createElement("INPUT");
                sInput.setAttribute("id", "em_in2_"+i);
              sInput.setAttribute("class", "tab_editMonth");
                sInput.setAttribute("type", "number");
                sInput.setAttribute("step", 0.001);
                sInput.addEventListener('change',
                      function() { setNewValue(data, i, "ert2", "em_in2_"+i); }, false);
                span2.appendChild(sInput);
              }
              div1.appendChild(span2);
              em.appendChild(div1);
      } // document.getElementById("em_R"+i)) == null 

      //--- year
      document.getElementById("em_YY_"+i).value = data.data[i].EEYY;
      document.getElementById("em_YY_"+i).style.background = "white";
      //--- month
      document.getElementById("em_MM_"+i).value = data.data[i].MM;
      document.getElementById("em_MM_"+i).style.background = "white";
      
      if (type == "ED")
      {
        document.getElementById("em_in1_"+i).style.background = "white";
        document.getElementById("em_in1_"+i).value = (data.data[i].values[0] *1).toFixed(3);
        document.getElementById("em_in2_"+i).style.background = "white";
        document.getElementById("em_in2_"+i).value = (data.data[i].values[1] *1).toFixed(3);
      }
      else if (type == "ER")
      {
        document.getElementById("em_in1_"+i).style.background = "white";
        document.getElementById("em_in1_"+i).value = (data.data[i].values[2] *1).toFixed(3);
        document.getElementById("em_in2_"+i).style.background = "white";
        document.getElementById("em_in2_"+i).value = (data.data[i].values[3] *1).toFixed(3);
      }
      else if (type == "GD")
      {
        document.getElementById("em_in1_"+i).style.background = "white";
        document.getElementById("em_in1_"+i).value = (data.data[i].values[4] *1).toFixed(3);
      }
      
    } // for all elements in data
    
    console.log("Now sequence EEYY/MM values ..");
    //--- sequence EEYY and MM data
// 	console.log("actslot: " + data.actSlot);

    let changed = false;
    for (let index=data.actSlot+dlength; index>data.actSlot; index--)
    {  let i = index % dlength;
       let next = math.mod(i-1,dlength);
    	console.log("index: "+index+" i: "+i+" next: "+next);
      //--- month
      if (data.data[next].MM == 0)
      {
        data.data[next].MM    = data.data[i].MM -1;
        changed = true;
        if (data.data[next].MM < 1) {
          data.data[next].MM   = 12;
		 data.data[next].EEYY = data.data[i].EEYY -1;
		 document.getElementById("em_YY_"+(next)).value = data.data[next].EEYY;
        } else {data.data[next].EEYY = data.data[i].EEYY}
        document.getElementById("em_MM_"+(next)).value = data.data[next].MM;        
  	 	document.getElementById("em_YY_"+(next)).value = data.data[next].EEYY;
      }
      if (changed) sendPostReading(next, data.data);

    } // sequence EEYY and MM

  } // showMonthsV2()

      
  //============================================================================  
  function undoReload()
  {
    if (activeTab == "bEditMonths") {
      console.log("EditMonths");
      EditMonths();
    } else if (activeTab == "bEditSettings") {
      console.log("undoReload(): reload Settings..");
//       data = {};
//       refreshSettings();
		document.getElementById('settings_table').innerHTML = '';
		objDAL.refreshDeviceInformation();
		clearDirtySettings();

    } else {
      console.log("undoReload(): I don't knwo what to do ..");
    }

  } // undoReload()
  
  
  //============================================================================  
  function saveData() 
  {
    document.getElementById('message').innerHTML = "Gegevens worden opgeslagen ..";
    switch(activeTab){
      case "bEditSettings":
        saveSettings();
        clearDirtySettings();
        break;
      case "bEditMonths":
        saveMeterReadings();
        break;
    }
    
  } // saveData()
  
  
  //============================================================================  
  function saveSettings() 
  {
    for( let i in objDAL.dev_settings )
    {
	  if ( i == "conf" || document.getElementById("setFld_"+i ) == undefined ) continue;
	  
	  let fldId  = i;
      let newVal = document.getElementById("setFld_"+fldId).value;
	  if ( objDAL.dev_settings[i].value === undefined ) {
	     newVal = document.getElementById("setFld_"+fldId).checked;
	     if (objDAL.dev_settings[i] != newVal) {
	     console.log("save data ["+i+"] => from["+objDAL.dev_settings[i]+"] to["+newVal+"]");
	     sendPostSetting(fldId, newVal);
	    }
	  }
	  else if (objDAL.dev_settings[i].value != newVal)
      {
        console.log("save data ["+i+"] => from["+objDAL.dev_settings[i].value+"] to["+newVal+"]");
        sendPostSetting(fldId, newVal);
      }
    }    
    // delay refresh as all fetch functions are async!!
    setTimeout(function() {
//       refreshSettings();
		document.getElementById('settings_table').innerHTML = '';
		objDAL.refreshDeviceInformation();
    }, 1000);
//     ParseDevSettings();
  } // saveSettings()
  
  
  //============================================================================  
  function saveMeterReadings() 
  {
    console.log("Saving months-data ..");
    let changes = false;
    
    /** skip this for now **
    if (!validateReadings(monthType))
    {
      return;
    }
    **/
    let data = objDAL.getMonths();
//     console.log(data.data);
    //--- has anything changed?
    for (i in data.data)
    {
      console.log("saveMeterReadings["+i+"] ..");
      changes = false;

      if (getBackGround("em_YY_"+i) == "lightgray")
      {
        setBackGround("em_YY_"+i, "white");
        changes = true;
      }
      if (getBackGround("em_MM_"+i) == "lightgray")
      {
        setBackGround("em_MM_"+i, "white");
        changes = true;
      }

      if (document.getElementById("em_in1_"+i).style.background == 'lightgray')
      {
        changes = true;
        document.getElementById("em_in1_"+i).style.background = 'white';
      }
      if (monthType != "GD")
      {
        if (document.getElementById("em_in2_"+i).style.background == 'lightgray')
        {
          changes = true;
          document.getElementById("em_in2_"+i).style.background = 'white';
        }
      }
      if (changes) {
        console.log("Changes where made in ["+i+"]["+data.data[i].EEYY+"-"+data.data[i].MM+"]");
        //processWithTimeout([(data.length -1), 0], 2, data, sendPostReading);
        sendPostReading(i, data.data);
      }
    } 

  } // saveMeterReadings()

    
  //============================================================================  
  function sendPostSetting(field, value) 
  {
    const jsonString = {"name" : field, "value" : value };
    const other_params = {
        headers : { "content-type" : "application/json; charset=UTF-8"},
        body : JSON.stringify(jsonString),
        method : "POST",
        //aangepast cors -> no-cors
        mode : "no-cors"
    };
	Spinner(true);
    fetch( URL_DEVICE_SETTINGS, other_params )
      .then(function(response) {
            //console.log(response.status );    //=> number 100–599
            //console.log(response.statusText); //=> String
            //console.log(response.headers);    //=> Headers
            //console.log(response.url);        //=> String
            //console.log(response.text());
            //return response.text()
            Spinner(false);
      }, function(error) {
        console.log("Error["+error.message+"]"); //=> String
      });      
  } // sendPostSetting()

    
  //============================================================================  
  function validateReadings(type) 
  {
    let withErrors = false;
    let prevMM     = 0;
    let lastBG     = "white";
        
    console.log("validate("+type+")");
    
    for (let i=0; i<(data.length -1); i++)
    {
      //--- reset background for the years
      if (getBackGround("em_YY_"+i) == "red")
      {
        setBackGround("em_YY_"+i, "lightgray");
      }
      //--- zelfde jaar, dan prevMM := (MM -1)
      if ( data[i].EEYY == data[i+1].EEYY )
      {
        prevMM = data[i].MM -1;
        //console.log("["+i+"].EEYY == ["+(i+1)+"].EEYY => ["+data[i].EEYY+"] prevMM["+prevMM+"]");
      }
      //--- jaar == volgend jaar + 1
      else if ( data[i].EEYY == (data[i+1].EEYY +1) )
      {
        prevMM = 12;
        //console.log("["+i+"].EEYY == ["+(i+1)+"].EEYY +1 => ["+data[i].EEYY+"]/["+data[i+1].EEYY+"] ("+prevMM+")");
      }
      else
      {
        setBackGround("em_YY_"+(i+1), "red");
        withErrors = true;
        prevMM = data[i].MM -1;
        //console.log("["+i+"].EEYY == ["+(i+1)+"].EEYY +1 => ["+data[i].EEYY+"]/["+data[i+1].EEYY+"] (error)");
      }
      
      //--- reset background for the months
      if (getBackGround("em_MM_"+(i+1)) == "red")
      {
        setBackGround("em_MM_"+(i+1), "lightgray");
      }
      //--- if next month != prevMM and this MM != next MM
      if (data[i+1].MM != prevMM && data[i].MM != data[i+1].MM)
      {
        setBackGround("em_MM_"+(i+1), "red");
        withErrors = true;
        //console.log("(["+(i+1)+"].MM != ["+prevMM+"].prevMM) && => ["+data[i].MM+"]/["+data[i+1].MM+"] (error)");
      }
      else
      {
        //setBackGround("em_MM_"+i, "lightgreen");
      }
      if (type == "ED")
      {
        if (getBackGround("em_in1_"+(i+1)) == "red")
        {
          setBackGround("em_in1_"+(i+1), "lightgray");
        }
        if (data[i].edt1 < data[i+1].edt1)
        {
          setBackGround("em_in1_"+(i+1), "red");
          withErrors = true;
        }
        if (getBackGround("em_in2_"+(i+1)) == "red")
        {
          setBackGround("em_in2_"+(i+1), "lightgray");
        }
        if (data[i].edt2 < data[i+1].edt2)
        {
          setBackGround("em_in2_"+(i+1), "red");
          withErrors = true;
        }
      }
      else if (type == "ER")
      {
        if (getBackGround("em_in1_"+(i+1)) == "red")
        {
          setBackGround("em_in1_"+(i+1), "lightgray");
        }
        if (data[i].ert1 < data[i+1].ert1)
        {
          setBackGround("em_in1_"+(i+1), "red");
          withErrors = true;
        }
        if (getBackGround("em_in2_"+(i+1)) == "red")
        {
          setBackGround("em_in2_"+(i+1), "lightgray");
        }
        if (data[i].ert2 < data[i+1].ert2)
        {
          setBackGround("em_in2_"+(i+1), "red");
          withErrors = true;
        }
      }
      else if (type == "GD")
      {
        if (getBackGround("em_in1_"+(i+1)) == "red")
        {
          setBackGround("em_in1_"+(i+1), "lightgray");
        }
        if (data[i].gdt < data[i+1].gdt)
        {
          setBackGround("em_in1_"+(i+1), "red");
          withErrors = true;
        }
      }
      
    }
    if (withErrors)  return false;

    return true;
    
  } // validateReadings()
  
    
  //============================================================================  
  function sendPostReading(i, row) 
  {
    console.log("sendPostReadings["+i+"]..");
    let sYY = (row[i].EEYY - 2000).toString();
    let sMM = "00";
    if ((row[i].MM *1) < 1 || (row[i].MM *1) > 24)
    {
      console.log("send: ERROR MM["+row[i].MM+"]");
      return;
    }
    if (row[i].MM < 10)
          sMM = "0"+(row[i].MM).toString();
    else  sMM = ((row[i].MM * 1)).toString();
    let sDDHH = "0101";
    let recId = sYY + sMM + sDDHH;
    console.log("send["+i+"] => ["+recId+"]");
    
    const jsonString = {"recid": recId, "edt1": 1 * row[i].values[0], "edt2": 1 * row[i].values[1],
                         "ert1": 1 * row[i].values[2],  "ert2": 1 * row[i].values[3], "gdt":  1 * row[i].values[4] };
	console.log ("JsonString: "+JSON.stringify(jsonString));
    const other_params = {
        headers : { "content-type" : "application/json; charset=UTF-8"},
        body : JSON.stringify(jsonString),
        method : "POST",
        mode : "cors"
    };
    fetch(APIGW+"v2/hist/months", other_params)
      .then(function(response) {
      }, function(error) {
        console.log("Error["+error.message+"]"); //=> String
      });
  } // sendPostReading()
  /*
  ****************************** UTILS *******************************************
  */

  function hideAllCharts(){//--- hide canvas -------
    document.getElementById("dataChart" ).style.display = "none";
    document.getElementById("gasChart"  ).style.display = "none";
    document.getElementById("waterChart").style.display = "none";
    document.getElementById("solarChart").style.display = "none";
  }
    
  //============================================================================  
  function setEditType(eType) {
    if (eType == "ED") {
      console.log("Edit Energy Delivered!");
      monthType = eType;
      EditMonths()
      showMonthsV2(data, monthType);
    } else if (eType == "ER") {
      console.log("Edit Energy Returned!");
      monthType = eType;
      EditMonths()
      showMonthsV2(data, monthType);
    } else if (eType == "GD") {
      console.log("Edit Gas Delivered!");
      monthType = eType;
      EditMonths()
      showMonthsV2(data, monthType);
    } else {
      console.log("setEditType to ["+eType+"] is quit shitty!");
      monthType = "";
    }

  } // setEditType()

   
  //============================================================================  
  function setNewValue(data, i, dField, field) {
    document.getElementById(field).style.background = "lightgray";
    if (dField == "EEYY")       data.data[i].EEYY = document.getElementById(field).value;
    else if (dField == "MM")    data.data[i].MM   = document.getElementById(field).value;
    else if (dField == "edt1")  data.data[i].values[0] = document.getElementById(field).value;
    else if (dField == "edt2")  data.data[i].values[1] = document.getElementById(field).value;
    else if (dField == "ert1")  data.data[i].values[2] = document.getElementById(field).value;
    else if (dField == "ert2")  data.data[i].values[3] = document.getElementById(field).value;
    else if (dField == "gdt")   data.data[i].values[4]  = document.getElementById(field).value;
    
  } // setNewValue()

   
  //============================================================================  
  function setBackGround(field, newColor) {
    document.getElementById(field).style.background = newColor;
    
  } // setBackGround()

   
  //============================================================================  
  function getBackGround(field) {
    return document.getElementById(field).style.background;
    
  } // getBackGround()

  
  //============================================================================  
  function validateNumber(field) {
    console.log("validateNumber(): ["+field+"]");
    if (field == "EDT1" || field == "EDT2" || field == "ERT1" || field == "ERT2" || field == "GAS") {
      let pattern = /^\d{1,1}(\.\d{1,5})?$/;
      let max = 1.99999;
    } else {
      let pattern = /^\d{1,2}(\.\d{1,2})?$/;
      let max = 99.99;
    }
    let newVal = document.getElementById(field).value;
    newVal = newVal.replace( /[^0-9.]/g, '' );
    if (!pattern.test(newVal)) {
      document.getElementById(field).style.color = 'orange';
      console.log("wrong format");
    } else {
      document.getElementById(field).style.color = settingFontColor;
      console.log("valid number!");
    }
    if (newVal > max) {
      console.log("Number to big!");
      document.getElementById(field).style.color = 'orange';
      newVal = max;
    }
    document.getElementById(field).value = newVal * 1;
    
  } // validateNumber()

  
  //============================================================================  
  function formatDate(type, dateIn) 
  {
    let dateOut = "";
    if (type == "Hours")
    {
      //date = "20"+dateIn.substring(0,2)+"-"+dateIn.substring(2,4)+"-"+dateIn.substring(4,6)+" ["+dateIn.substring(6,8)+"]";
      dateOut = "("+dateIn.substring(4,6)+") ["+dateIn.substring(6,8)+":00 - "+dateIn.substring(6,8)+":59]";
    }
    else if (type == "Days")
      dateOut = recidToWeekday(dateIn)+" "+dateIn.substring(4,6)+"-"+dateIn.substring(2,4)+"-20"+dateIn.substring(0,2);
    else if (type == "Months")
      dateOut = "20"+dateIn.substring(0,2)+"-["+dateIn.substring(2,4)+"]-"+dateIn.substring(4,6)+":"+dateIn.substring(6,8);
    else
      dateOut = "20"+dateIn.substring(0,2)+"-"+dateIn.substring(2,4)+"-"+dateIn.substring(4,6)+":"+dateIn.substring(6,8);
    return dateOut;
  }

  
  //============================================================================  
  function recidToEpoch(dateIn) 
  {
    let YY = "20"+dateIn.substring(0,2);
    console.log("["+YY+"]["+(dateIn.substring(2,4)-1)+"]["+dateIn.substring(4,6)+"]");
    //-------------------YY-------------------(MM-1)----------------------DD---------------------HH--MM--SS
    let epoch = Date.UTC(YY, (dateIn.substring(2,4)-1), dateIn.substring(4,6), dateIn.substring(6,8), 1, 1);
    //console.log("epoch is ["+epoch+"]");

    return epoch;
    
  } // recidToEpoch()
  
  
  //============================================================================  
  function recidToWeekday(dateIn)
  {
    let YY = "20"+dateIn.substring(0,2);
    //-------------------YY-------------------(MM-1)----------------------DD---------------------HH--MM--SS
    let dt = new Date(Date.UTC(YY, (dateIn.substring(2,4)-1), dateIn.substring(4,6), 1, 1, 1));

    return dt.toLocaleDateString(locale, {weekday: 'long'});
    
  } // epochToWeekday()
  
    
  //============================================================================  
  function round(value, precision) 
  {
    let multiplier = Math.pow(10, precision || 0);
    return Math.round(value * multiplier) / multiplier;
  }

//================ NAV.json

function menu() {
  let x = document.getElementById("myTopnav");
  if (x.className === "main-navigation") {
    x.className += " responsive";
    
  } else {
    x.className = "main-navigation";
  }
//change menu icon
    let menu = document.getElementById("menuid");
	menu.classList.toggle("mdi-close");
	menu.classList.toggle("mdi-menu");
}
   
//============================================================================         
// attach an onclick handler for all menuitems
function handle_menu_click()
{	
	let btns = document.getElementsByClassName("nav-item");
	for (let i = 0; i < btns.length; i++) {
  		btns[i].addEventListener("click", function() {
			//reset hamburger menu icon
			let menu = document.getElementById("menuid");
			menu.classList.remove("mdi-close");
			menu.classList.add("mdi-menu");
	
			//remove active classes
			let current = document.getElementById("myTopnav").getElementsByClassName("active");	
			for (let j=current.length-1; j>=0; j--) {
	// 					console.log("remove active ["+j+"] current:"+current[j]);
				current[j].classList.remove("active");
		}
		//add new active classes
			let closest = this.closest("ul");
			if (closest && !closest.previousElementSibling.classList.contains("active","topcorner","mdi")) {
				console.log("contains: subnav, closest:" +closest.className);
				closest.previousElementSibling.classList.add("active");
			}
			if(!this.classList.contains("active","nav-img")) 
			this.classList.add("active");

			activeTab = this.id;
			//console.log("ActiveID - " + activeTab );
// 			openTab();  		
  		});
	}
}

//====== I18N.JS ====

let translations = {};
const URL_I18N = typeof DEBUG !== 'undefined' && DEBUG
  ? "http://localhost/~martijn/dsmr-api/v5/lang"
  : "https://cdn.jsdelivr.net/gh/mhendriks/P1-Dongel-ESP32@latest/cdn/lang";

function t(key) {
  return translations[key] || key;
}

function td(key) {
  return t("dict_" + key);
}

function applyTranslations() {
  document.querySelectorAll('[data-i18n-key]').forEach(el => {
    const key = el.getAttribute('data-i18n-key');
    if (translations[key]) el.innerHTML = translations[key];
  });
}

function changeLanguage(lang) {
  locale = lang;
  localStorage.setItem('locale', lang);
  loadTranslations(lang);
  document.location.href="/";
}

function loadTranslations(lang) {
	console.log(URL_I18N + `/${lang}.json`);
  fetch( URL_I18N + `/${lang}.json` )
    .then(response => response.json())
    .then(json => {
      translations = json;
      applyTranslations();
    })
    .catch(err => console.error("Error fetching lang file:", err));
}
