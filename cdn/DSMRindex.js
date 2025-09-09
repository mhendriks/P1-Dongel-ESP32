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
  
  let locale 				= 'nl';  // standaard

// ---- DASH
let TotalAmps=0.0,minKW = 0.0, maxKW = 0.0,minV = 0.0, maxV = 0.0, Pmax,Gmax, Wmax;
let hist_arrW=[4], hist_arrG=[4], hist_arrPa=[4], hist_arrPi=[4], hist_arrP=[4]; //berekening verbruik
let day = 0;

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
			
            const tbody = document.getElementById("InsightsTableBody");
			tbody.innerHTML = '';
			
            for (const key in data) {
//                 if (!(key in namen)) continue; 

                const row = document.createElement("tr");
                const naamCell = document.createElement("td");
               	naamCell.textContent = key;
            	naamCell.title = t("tip-"+key);
                const waardeCell = document.createElement("td");
                const eenheidCell = document.createElement("td");

                naamCell.textContent = t("lbl-"+key);

                let waarde = data[key];
				
                if (key.startsWith("U")) {
                    waarde = (waarde / 1000).toFixed(1); // mV → V
                } else if (key.startsWith("I")) {
                    waarde = (waarde / 1000).toFixed(3); // mA → A
                } else if (key === "start_time") {
//                     const datum = new Date(waarde * 1000);
                    let uur  = Math.trunc(waarde/3600 % 24);
                    let minuten  = Math.trunc(waarde/60 % 60);
                    waarde = String(uur).padStart(2, '0') + ":" + String(minuten).padStart(2, '0');
                    
                } else if (key.startsWith("Psl")) {
                	if ( waarde == 0xFFFFFFFF ) waarde = "-";
                }

                waardeCell.textContent = waarde;
                eenheidCell.textContent = eenheden[key] || "";
                eenheidCell.style.textAlign = "center";

                row.appendChild(naamCell);
                row.appendChild(waardeCell);
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

// function loadIcons(){
// Iconify.addCollection({
//    icons: {
//        "mdi-external-link": {
//            body: '<path d="M12 6H6a2 2 0 0 0-2 2v10a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2v-6m-7 1l9-9m-5 0h5v5M10 6v2H5v11h11v-5h2v6a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1V7a1 1 0 0 1 1-1h6Zm11-3v8h-2V6.413l-7.793 7.794l-1.414-1.414L17.585 5H13V3h8Z" fill="currentColor"/>',
//        },
//        "mdi-folder-outline": {
//            body: '<path d="M20 18H4V8h16m0-2h-8l-2-2H4c-1.11 0-2 .89-2 2v12a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2V8a2 2 0 0 0-2-2Z" fill="currentColor"/>',
//        },
//        "mdi-information-outline": {
//            body: '<path d="M11 9h2V7h-2m1 13c-4.41 0-8-3.59-8-8s3.59-8 8-8s8 3.59 8 8s-3.59 8-8 8m0-18A10 10 0 0 0 2 12a10 10 0 0 0 10 10a10 10 0 0 0 10-10A10 10 0 0 0 12 2m-1 15h2v-6h-2v6Z" fill="currentColor"/>',
//        },
//        "mdi-cog": {
//            body: '<path d="M12 15.5A3.5 3.5 0 0 1 8.5 12A3.5 3.5 0 0 1 12 8.5a3.5 3.5 0 0 1 3.5 3.5a3.5 3.5 0 0 1-3.5 3.5m7.43-2.53c.04-.32.07-.64.07-.97c0-.33-.03-.66-.07-1l2.11-1.63c.19-.15.24-.42.12-.64l-2-3.46c-.12-.22-.39-.31-.61-.22l-2.49 1c-.52-.39-1.06-.73-1.69-.98l-.37-2.65A.506.506 0 0 0 14 2h-4c-.25 0-.46.18-.5.42l-.37 2.65c-.63.25-1.17.59-1.69.98l-2.49-1c-.22-.09-.49 0-.61.22l-2 3.46c-.13.22-.07.49.12.64L4.57 11c-.04.34-.07.67-.07 1c0 .33.03.65.07.97l-2.11 1.66c-.19.15-.25.42-.12.64l2 3.46c.12.22.39.3.61.22l2.49-1.01c.52.4 1.06.74 1.69.99l.37 2.65c.04.24.25.42.5.42h4c.25 0 .46-.18.5-.42l.37-2.65c.63-.26 1.17-.59 1.69-.99l2.49 1.01c.22.08.49 0 .61-.22l2-3.46c.12-.22.07-.49-.12-.64l-2.11-1.66Z" fill="currentColor"/>',
//               },
//        "mdi-gauge": {
//            body: '<path d="M12 2A10 10 0 0 0 2 12a10 10 0 0 0 10 10a10 10 0 0 0 10-10A10 10 0 0 0 12 2m0 2a8 8 0 0 1 8 8c0 2.4-1 4.5-2.7 6c-1.4-1.3-3.3-2-5.3-2s-3.8.7-5.3 2C5 16.5 4 14.4 4 12a8 8 0 0 1 8-8m2 1.89c-.38.01-.74.26-.9.65l-1.29 3.23l-.1.23c-.71.13-1.3.6-1.57 1.26c-.41 1.03.09 2.19 1.12 2.6c1.03.41 2.19-.09 2.6-1.12c.26-.66.14-1.42-.29-1.98l.1-.26l1.29-3.21l.01-.03c.2-.51-.05-1.09-.56-1.3c-.13-.05-.26-.07-.41-.07M10 6a1 1 0 0 0-1 1a1 1 0 0 0 1 1a1 1 0 0 0 1-1a1 1 0 0 0-1-1M7 9a1 1 0 0 0-1 1a1 1 0 0 0 1 1a1 1 0 0 0 1-1a1 1 0 0 0-1-1m10 0a1 1 0 0 0-1 1a1 1 0 0 0 1 1a1 1 0 0 0 1-1a1 1 0 0 0-1-1Z" fill="currentColor"/>',
//        },
//        "mdi-lightning-bolt": {
//            body: '<path d="M11 15H6l7-14v8h5l-7 14v-8Z" fill="currentColor"/>',
//        },       
//        "mdi-home-import-outline": {
//            body: '<path d="m15 13l-4 4v-3H2v-2h9V9l4 4M5 20v-4h2v2h10v-7.81l-5-4.5L7.21 10H4.22L12 3l10 9h-3v8H5Z" fill="currentColor"/>',
//        },       
//        "mdi-home-export-outline": {
//            body: '<path d="m24 13l-4 4v-3h-9v-2h9V9l4 4M4 20v-8H1l10-9l7 6.3v.7h-2.21L11 5.69l-5 4.5V18h10v-2h2v4H4Z" fill="currentColor"/>',
//        },  
//        "mdi-fire": {
//            body: '<path d="M17.66 11.2c-.23-.3-.51-.56-.77-.82c-.67-.6-1.43-1.03-2.07-1.66C13.33 7.26 13 4.85 13.95 3c-.95.23-1.78.75-2.49 1.32c-2.59 2.08-3.61 5.75-2.39 8.9c.04.1.08.2.08.33c0 .22-.15.42-.35.5c-.23.1-.47.04-.66-.12a.58.58 0 0 1-.14-.17c-1.13-1.43-1.31-3.48-.55-5.12C5.78 10 4.87 12.3 5 14.47c.06.5.12 1 .29 1.5c.14.6.41 1.2.71 1.73c1.08 1.73 2.95 2.97 4.96 3.22c2.14.27 4.43-.12 6.07-1.6c1.83-1.66 2.47-4.32 1.53-6.6l-.13-.26c-.21-.46-.77-1.26-.77-1.26m-3.16 6.3c-.28.24-.74.5-1.1.6c-1.12.4-2.24-.16-2.9-.82c1.19-.28 1.9-1.16 2.11-2.05c.17-.8-.15-1.46-.28-2.23c-.12-.74-.1-1.37.17-2.06c.19.38.39.76.63 1.06c.77 1 1.98 1.44 2.24 2.8c.04.14.06.28.06.43c.03.82-.33 1.72-.93 2.27Z" fill="currentColor"/>',
//        },         
//        "mdi-water": {
//            body: '<path d="M12 20a6 6 0 0 1-6-6c0-4 6-10.75 6-10.75S18 10 18 14a6 6 0 0 1-6 6Z" fill="currentColor"/>',
//        },         
//        "mdi-sine-wave": {
//            body: '<path d="M16.5 21c-3 0-4.19-4.24-5.45-8.72C10.14 9.04 9 5 7.5 5C4.11 5 4 11.93 4 12H2c0-.37.06-9 5.5-9c3 0 4.21 4.25 5.47 8.74C13.83 14.8 15 19 16.5 19c3.44 0 3.53-6.93 3.53-7h2c0 .37-.06 9-5.53 9Z" fill="currentColor"/>',
//        },        
//        "mdi-heat-wave": {
//            body: '<path d="m8.5 4.5l-3.1 5l3.1 5.2l-3.3 5.8l-1.8-.9l2.7-4.9L3 9.5l3.7-5.9l1.8.9m6.2-.1l-3.1 5.1l3.1 5l-3.3 5.8l-1.8-.9l2.7-4.9l-3.1-5l3.7-6l1.8.9m6.3 0l-3.1 5.1l3.1 5l-3.3 5.8l-1.8-.9l2.7-4.9l-3.1-5l3.7-6l1.8.9" fill="currentColor"/>',
//        }, 
//        "mdi-chart-box-outline": {
//            body: '<path d="M9 17H7v-7h2v7m4 0h-2V7h2v10m4 0h-2v-4h2v4m2 2H5V5h14v14.1M19 3H5c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h14c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2Z" fill="currentColor"/>',
//        },
//        "mdi-pulse": {
//            body: '<path d="M9 17H7v-7h2v7m4 0h-2V7h2v10m4 0h-2v-4h2v4m2 2H5V5h14v14.1M19 3H5c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h14c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2Z" fill="currentColor"/>',
//        },        
// 	   "mdi-solar-power-letiant": {
//            body: '<path d="M3.33 16H11V13H4L3.33 16M13 16H20.67L20 13H13V16M21.11 18H13V22H22L21.11 18M2 22H11V18H2.89L2 22M11 8H13V11H11V8M15.76 7.21L17.18 5.79L19.3 7.91L17.89 9.33L15.76 7.21M4.71 7.91L6.83 5.79L8.24 7.21L6.12 9.33L4.71 7.91M3 2H6V4H3V2M18 2H21V4H18V2M12 7C14.76 7 17 4.76 17 2H7C7 4.76 9.24 7 12 7Z" fill="currentColor"/>',
//        },
// 	   "mdi-battery-low": {
//            body: '<path d="M16 20H8V6H16M16.67 4H15V2H9V4H7.33C6.6 4 6 4.6 6 5.33V20.67C6 21.4 6.6 22 7.33 22H16.67C17.41 22 18 21.41 18 20.67V5.33C18 4.6 17.4 4 16.67 4M15 16H9V19H15V16" />',
//        }, 
// 
// 	   "mdi-format-vertical-align-top": {
//            body: '<path d="M8,11H11V21H13V11H16L12,7L8,11M4,3V5H20V3H4Z" />',
//        },        
//        
//     },
//    width: 24,
//    height: 24,
// });
// }

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
  
function SetOnSettings(json) {
	// Initiele detectie: water, gas, teruglevering
	if (!HeeftGas && !IgnoreGas)
		HeeftGas = "gas_delivered" in json ? !isNaN(json.gas_delivered.value) : false;
	if (!HeeftWater)
		HeeftWater = "water" in json ? !isNaN(json.water.value) : false;

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
	  ['lastHoursTable', [2]],
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

		//SCR
		if ( !Pi_today ) return;
		// console.log("Pi_today: " + Pi_today*1000);
		// console.log("json.total.daily: " + json.total.daily);
		if ( json.total.daily ) document.getElementById('scr').innerHTML = Number(((json.total.daily-(1000*Pi_today))/json.total.daily)*100).toFixed(0);
		else document.getElementById('seue').innerHTML = "-";
		// Number(maxV.toFixed(1)).toLocaleString("nl", {minimumFractionDigits: 1, maximumFractionDigits: 1} );
		//SEUE				( Productie - Teruglevering ) / ( Afname + Productie - Teruglevering )
		if ( json.total.daily ) document.getElementById('seue').innerHTML = Number( (json.total.daily-(1000*Pi_today)) / (json.total.daily-(1000*Pi_today)+(Pd_today*1000))*100 ).toFixed(0);
		else document.getElementById('seue').innerHTML = "-";
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
	//check new day = refresh
	let DayEpochTemp = Math.floor(new Date().getTime() / 86400000.0);
	if (DayEpoch != DayEpochTemp || hist_arrP.length < 4 ) {
		if ( EnableHist ) refreshHistData("Days"); //load data first visit
		DayEpoch = DayEpochTemp;
	}

		//-------CHECKS

		SetOnSettings(json);
		if (json.timestamp.value == "-") {
			console.log("timestamp missing : p1 data incorrect!");
			return;
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
		if ( json.peak_pwr_last_q.value != "-" ) document.getElementById("dash_peak").style.display = "block";

		//------- EID Planner
		if ( eid_planner_enabled ) objDAL.refreshEIDPlanner();
		
		//-------Kwartierpiek
		document.getElementById("peak_month").innerHTML = formatValue(json.highest_peak_pwr.value);
		document.getElementById("dash_peak_delta").innerText = formatValue(json.highest_peak_pwr.value - json.peak_pwr_last_q.value);
		trend_peak.data.datasets[0].data=[json.peak_pwr_last_q.value, json.highest_peak_pwr.value-json.peak_pwr_last_q.value];	
		trend_peak.update();
		if (json.peak_pwr_last_q.value > 0.75 * json.highest_peak_pwr.value ) trend_peak.data.datasets[0].backgroundColor = ["#dd0000", "rgba(0,0,0,0.1)"];
		else trend_peak.data.datasets[0].backgroundColor = ["#009900", "rgba(0,0,0,0.1)"];
		
		//-------VOLTAGE
		let v1 = 0, v2 = 0, v3 = 0;
		if (!isNaN(json.voltage_l1.value)) v1 = json.voltage_l1.value; 
		if (!isNaN(json.voltage_l2.value)) v2 = json.voltage_l2.value;
		if (!isNaN(json.voltage_l3.value)) v3 = json.voltage_l3.value;


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
// 			document.getElementById(`power_delivered_2max`).innerHTML = Number(maxV.toFixed(1)).toLocaleString("nl", {minimumFractionDigits: 1, maximumFractionDigits: 1} );
// 			document.getElementById(`power_delivered_2min`).innerHTML = Number(minV.toFixed(1)).toLocaleString("nl", {minimumFractionDigits: 1, maximumFractionDigits: 1} );

			//update gauge
			gaugeV.data.datasets[0].data=[v1-207,253-json.voltage_l1.value];
			if (v2) gaugeV.data.datasets[1].data=[v2-207,253-json.voltage_l2.value];
			if (v3) gaugeV.data.datasets[2].data=[v3-207,253-json.voltage_l3.value];
			gaugeV.update();
		}
		//-------ACTUAL
		//afname of teruglevering bepalen en signaleren
		let TotalKW	= json.power_delivered.value - json.power_returned.value;

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
		if ( !isNaN(json.current_l1.value ) ) {
		TotalAmps = (isNaN(json.current_l1.value)?0:json.current_l1.value) + 
			(isNaN(json.current_l2.value)?0:json.current_l2.value) + 
			(isNaN(json.current_l3.value)?0:json.current_l3.value);

		gauge3f.data.datasets[0].data=[json.current_l1.value,AMPS-json.current_l1.value];
		if (Phases == 2) {
			gauge3f.data.datasets[1].data=[json.current_l2.value,AMPS-json.current_l2.value];
			document.getElementById("f2").style.display = "inline-block";
			document.getElementById("v2").style.display = "inline-block";
			}
		if (Phases == 3) {
			gauge3f.data.datasets[1].data=[json.current_l2.value,AMPS-json.current_l2.value];
			document.getElementById("f2").style.display = "inline-block";
			document.getElementById("v2").style.display = "inline-block";
			
			gauge3f.data.datasets[2].data=[json.current_l3.value,AMPS-json.current_l3.value];
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
		

    //-------VERBRUIK METER	
		if (Dongle_Config != "p1-q") {

      //bereken verschillen afname, teruglevering en totaal
      let nPA = isNaN(json.energy_delivered_tariff1.value)? 0:json.energy_delivered_tariff1.value + isNaN(json.energy_delivered_tariff2.value)?0:json.energy_delivered_tariff2.value;
      let nPI = isNaN(json.energy_returned_tariff1.value) ? 0:json.energy_returned_tariff1.value  + isNaN(json.energy_returned_tariff2.value) ?0:json.energy_returned_tariff2.value;
      Parra = calculateDifferences( nPA, hist_arrPa, 1);
      Parri = calculateDifferences( nPI, hist_arrPi, 1);
      for(let i=0;i<3;i++){ Parr[i]=Parra[i] - Parri[i]; }

      //dataset berekenen voor Ptotaal      
      updateGaugeTrend(trend_p, Parr);
      document.getElementById("P").innerHTML = formatValue( Parr[0] );
      
	  Pi_today = Parri[0];
	  Pd_today = Parra[0];
	        
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
		if ( HeeftGas && (Dongle_Config != "p1-q") ) 
		{
      Garr = calculateDifferences(json.gas_delivered.value, hist_arrG, 1);
      updateGaugeTrend(trend_g, Garr);
			document.getElementById("G").innerHTML = formatValue(Garr[0]);
		}
		
		//-------WATER	
		if (HeeftWater) 
		{
      Warr = calculateDifferences(json.water.value, hist_arrW, 1000);
      updateGaugeTrend(trend_w, Warr);
			document.getElementById("W").innerHTML = Number(Warr[0]).toLocaleString();
		}
				
		//-------HEAT
		if (Dongle_Config == "p1-q") 
		{
      Garr = calculateDifferences(json.gas_delivered.value, hist_arrG, 1000);
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
				
			case "Redirect":
				handleRedirect();
				break;
	
			case "Updating":
				console.log("Updaten bezig...");
				break;
	
			default:
				//redirect to cmd
				const btn = document.getElementById("b"+cmd);
				if (btn) btn.click(); else console.log("Button b"+cmd+" not found");
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
   let col = tbl.getElementsByTagName('col')[col_no];
   if (col) {
     col.style.visibility=do_show?"":"collapse";
   }
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
    row.insertCell(2).innerHTML = `<a style='color:red' onclick='RemoteUpdate("public")' href='#'>${t('lbl-install')}</a>`;
    console.log("last version:", manifest.major * 10000 + manifest.minor * 100);
  }
  
    if (manifest.beta) {
    const row = tableRef.insertRow(-1);
    row.insertCell(0).innerHTML = t("lbl-beta-fwversion");
    row.insertCell(1).innerHTML = manifest.beta;
    row.insertCell(2).innerHTML = `<a style='color:red' onclick='RemoteUpdate("beta")' href='#'>${t('lbl-install')}</a>`;
  }

  // add dev info
  for (let k in obj) {
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
//     const updateCell = tableRef.rows[0].cells[2];
//     updateCell.innerHTML = (firmwareVersion < latest)
//       ? `<a style='color:red' onclick='RemoteUpdate()' href='#'>${t('lbl-click-update')}</a>`
//       : td("latest_version");
  }
}
    
function closeUpdate() {
	document.getElementById("updatePopup").style.visibility = "hidden";
	document.location.href="/";
	// location.reload();
}    

let progress = 0;
let progressBar, statusText, checkInterval, update_interval; 

function UpdateStart( msg ){
	console.log("Updatestatus msg: " + msg );
// 	console.log("Updatestatus error: " + msg.split('=')[1] );

	document.getElementById("updatePopup").style.visibility = "visible";
	progressBar = document.getElementById("progressBar");
	statusText = document.getElementById("updatestatus");
	
	if ( msg && msg.split('=')[0] == "error") {
		statusText.innerText = "Error: " + msg.split('=')[1];
	} else {
		statusText.innerText = t(lbl_update_start);
		progress = 0;
		updateProgress();
	}
}
        
function RemoteUpdate(type) {        
	if ( type == "beta") document.location.href = "/remote-update?version=" + objDAL.version_manifest.beta;
	else document.location.href = "/remote-update?version=" + objDAL.version_manifest.version;
}

function updateProgress() {
	 update_interval = setInterval(() => {
		if (progress < 90) {
			progress += 5; // 18 seconden naar 90%
			progressBar.style.width = progress + "%";
		} else {
			clearInterval(update_interval);
			checkESPOnline(); // Start controle ESP32 na 90%
		}
	}, 1000);
}

function checkESPOnline() {
	statusText.innerText = "Wait on reboot...";
	update_reconnected = false;
	checkInterval = setInterval(() => {
		objDAL.refreshTime();
		if ( update_reconnected ){
			clearInterval(checkInterval);
			progressBar.style.width = "100%";
			statusText.innerText = t(lbl_update_done);
		}
	}, 2000); // Check elke 2 seconden
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
          AMPS=json.Fuse;
          ShowVoltage=json.ShowVoltage;
          UseCDN=json.cdn;
          Injection=json.Injection;
          Phases=json.Phases;   
          HeeftGas=json.GasAvailable;
		  "Act_Watt" in json ? Act_Watt = json.Act_Watt : Act_Watt = false;
		  "AvoidSpikes" in json ? AvoidSpikes = json.AvoidSpikes : AvoidSpikes = false;
          "IgnoreInjection" in json ? IgnoreInjection = json.IgnoreInjection : IgnoreInjection = false;
          "IgnoreGas" in json ? IgnoreGas = json.IgnoreGas : IgnoreGas = false;
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
    for (let item in data) 
    {
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
          tableCells[2].innerHTML = formatIdentifier(data[item].value);
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
	if (data === "") {
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
				let tempslot = math.mod(act_slot - i, 15);
				let values = data.data[tempslot].values;
				hist_arrG[i] = values[4];
				hist_arrW[i] = values[5];
				hist_arrPa[i] = values[0] + values[1];
				hist_arrPi[i] = values[2] + values[3];
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
    
      
  //============================================================================  
  function showHistTable(data, type)
  { 
    console.log("showHistTable("+type+")");
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
      if (type == "Days") nCells += 1;
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
      
    };

    //--- hide canvas
    hideAllCharts();
	
    //--- show table
    document.getElementById("lastHours").style.display = "block";
    document.getElementById("lastDays").style.display  = "block";

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
    
  //============================================================================  
  function refreshSettings()
  {
    console.log("refreshSettings() ..");

	data = objDAL.getDeviceSettings();
	for( let i in data )
	{
	  if ( i == "conf") continue;
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

				let sInput = document.createElement("INPUT");
				sInput.setAttribute("id", "setFld_"+i);
				if ( data[i].type === undefined ) {
					sInput.setAttribute("type", "checkbox");
					sInput.checked = data[i];
					sInput.style.width = "auto";
					if ( i == "dev-pairing") sInput.disabled = true;
				}
				else {
				switch(data[i].type){
				case "s":
					sInput.setAttribute("type", "text");
					sInput.setAttribute("maxlength", data[i].max);
					sInput.setAttribute("placeholder", "<max " + data[i].max +">");
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
					sInput.step = (data[i].min + data[i].max) / 1000;
					sInput.step = 1;
					break;
				}
				sInput.setAttribute("value", data[i].value);
				}
				sInput.addEventListener('change',
							function() { setBackGround("setFld_"+i, "lightgray"); },
										false
							);
			  inputDiv.appendChild(sInput);
			  
	  if( EnableHist || !( ["ed_tariff1","ed_tariff2", "er_tariff1","er_tariff2" ,"gd_tariff","electr_netw_costs","gas_netw_costs"].includes(i) ) ) {
			rowDiv.appendChild(inputDiv);
			settings.appendChild(rowDiv);
		};
	  }
	  else
	  {
		document.getElementById("setFld_"+i).style.background = "white";
		document.getElementById("setFld_"+i).value = data[i].value;
	  }
	}

//       document.getElementById('message').innerHTML = newVersionMsg;
      
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
                      function() { setNewValue(i, "EEYY", "em_YY_"+i); }, false);
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
                      function() { setNewValue(i, "MM", "em_MM_"+i); }, false);
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
                    function() { setNewValue(i, "edt1", "em_in1_"+i); }, false );
              }
              else if (type == "ER")
              {
                sInput.addEventListener('change',
                    function() { setNewValue(i, "ert1", "em_in1_"+i); }, false);
              }
              else if (type == "GD")
              {
                sInput.addEventListener('change',
                    function() { setNewValue(i, "gdt", "em_in1_"+i); }, false);
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
                      function() { setNewValue(i, "edt2", "em_in2_"+i); }, false);
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
                      function() { setNewValue(i, "ert2", "em_in2_"+i); }, false);
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
    if ((row[i].MM *1) < 1 || (row[i].MM *1) > 12)
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
  } // sendPostReading()
  /*
  ****************************** UTILS *******************************************
  */

  function hideAllCharts(){//--- hide canvas -------
    document.getElementById("dataChart" ).style.display = "none";
    document.getElementById("gasChart"  ).style.display = "none";
    document.getElementById("waterChart").style.display = "none";
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
  function setNewValue(i, dField, field) {
    document.getElementById(field).style.background = "lightgray";
    //--- this is ugly!!!! but don't know how to do it better ---
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