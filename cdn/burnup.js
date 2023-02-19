/*
	
	P1-Dongle Pro
	plugin by Mar10us
*/

// const APIGW = window.location.protocol+'//'+window.location.host+'/api/';
const URL_HISTORY_HOURS = APIGW + "../RNGhours.json";
const URL_HISTORY_DAYS = APIGW + "../RNGdays.json";
const URL_HISTORY_MONTHS = APIGW + "../RNGmonths.json";
const URL_HISTORY_ACTUAL = APIGW + "v2/sm/actual";
const MAX_ACTUAL_HISTORY = 15*6;	//store last 15 minutes (with a call per 10 sec)
const listMONTHS_SHORT = ["JAN", "FEB", "MRT", "APR", "MEI", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"];

//Deze data is gebaseerd op een gemiddeld gebruikprofiel. 
// zie o.a.
//  https://www.anwb.nl/huis/energie/het-prijsplafond-voor-energie
//  https://www.vattenfall.nl/nieuws/verbruiksplafond/
const listValuesCeilingE = [339, 280, 267, 207, 181, 159, 161, 176, 199, 266, 306, 356];	//totaal 2900 kWh	
const listValuesCeilingG = [221, 188, 159,  86,  35,  19,  17,  17,  24,  81, 146, 207];	//totaal 1200 m3


let timerRefresh = 0;
let sCurrentChart = "YEAR"; //YEAR
var objStorage;
var fLoadedBurnup = false;
var objChart1;
var objChart2;

//DSMR DataAccessLayer
//TODO: merge with DAL from index.js
class dsmr_dal_burnup{
	constructor() {
        this.months=[];
        this.days=[];
        this.hours=[];
        this.minutes=[];	//not used for now
		this.timerREFRESH = 0;
		this.actual=[];
		this.actual_history = [];
		this.timerREFRESH_ACTUAL = 0;
		this.init();
    }

	fetchDataJSON(url, fnHandleData)
	{
		console.log("fetchDataJSON("+url+")");		
		fetch(url)
		.then(response => response.json())
		.then(json => { fnHandleData(json); })
		.catch(function (error) {
			var p = document.createElement('p');
			p.appendChild( document.createTextNode('Error: ' + error.message) );
		});
	}	

	init()
	{
		this.refresh();
		//this.refreshActual();
	}

    // refresh and parse the ringfiles
	// refresh every 60 sec
    refresh()
	{
		console.log("refresh");
		clearInterval(this.timerREFRESH);
		this.fetchDataJSON( URL_HISTORY_MONTHS, this.parseMonths.bind(this));
		this.fetchDataJSON( URL_HISTORY_DAYS, this.parseDays.bind(this));
		this.fetchDataJSON( URL_HISTORY_HOURS, this.parseHours.bind(this));
    	this.timerREFRESH = setInterval(this.refresh.bind(this), 60 * 1000);
	}
	parseMonths(json)
	{
		console.log("parseMonths");
		console.log(json);
		//TODO: filter out the corrupt row
		this.months = json;
	}
	parseDays(json)
	{
		console.log("parseDays");
		console.log(json);
		//TODO: filter out the corrupt row
		this.days = json;
	}
	parseHours(json)
	{
		console.log("parseHours");
		console.log(json);
		//TODO: filter out the corrupt row
		this.hours = json;
	}

	// refresh and parse actual data, store and add to history
	//refresh every 10 sec
	refreshActual()
	{
		console.log("refreshActual");
		clearInterval(this.timerREFRESH_ACTUAL);
		this.fetchDataJSON( URL_HISTORY_ACTUAL, this.parseActual.bind(this));
    	this.timerREFRESH_ACTUAL = setInterval(this.refreshActual.bind(this), 10 * 1000);
	}
	parseActual(json)
	{
		this.actual = json;
		this.addActualHistory( json );
	}
	addActualHistory(json)
	{
		if( this.actual_history.length >= MAX_ACTUAL_HISTORY) this.actual_history.shift();
		this.actual_history.push(json);
	}

	//
	// get functions
	getMonths()
	{
		return this.months;
	}
	getDays()
	{
		return this.days;
	}
	getHours()
	{
		return this.hours;
	}

	//get a specific YYMM row
	getMonth(nYY, nMM)
	{
		var ret = [];
		var sYYMM = (""+nYY)+((nMM<=9)?"0"+nMM:nMM);
		for(var i=0;i<this.months.data.length; i++)
		{
			var item = this.months.data[i];
			var date = item.date;
			if( sYYMM == date.substring(0,4))
			{
				ret.push( item )
			}
		}		
		return ret;
	}

	//get every row from a specific year
	getYear(nYY)
	{
		var ret = [];
		var sYY = ""+nYY;
		for(var i=0;i<this.months.data.length; i++)
		{
			var item = this.months.data[i];
			var date = item.date;
			if( sYY == date.substring(0,2))
			{
				ret.push( item )
			}
		}
		ret.sort((a, b) => {
			return a.date.localeCompare(b.date);
		});
		return ret;
	}

	getActual()
	{
		return this.actual;
	}
	
	//the last (MAX_ACTUAL_HISTORY) actuals
	getActualHistory()
	{
		return this.actual_history;
	}
}

function createCharts() {
	const ctx1 = document.getElementById('myChart1').getContext('2d');
	const ctx2 = document.getElementById('myChart2').getContext('2d');

	objChart1 = new Chart(ctx1, {
		type: 'line',
		data: [],
		options: {
			title: {
					display: true,
					text: 'BURNUP ELEKTRA',
				},
			legend: {
					position: 'right',
				},			
			responsive: true,
			maintainAspectRatio: true,
			scales: {
				x: {
					display: true,
					title: {
					  display: true,
					  text: 'Maanden'
					}
				},
				y: {
					display: true,
					title: {
					  display: true,
					  text: 'kWh'
					}
				}
			} // scales
		}
	});
	objChart2 = new Chart(ctx2, {
		type: 'line',
		data: {},
		options: {
			legend: {
				position: 'right',
			},
			title: {
				display: true,
				text: 'BURNUP GAS',
			},
			responsive: true,
			maintainAspectRatio: true,
			scales: {
				x: {
					display: true,
					title: {
					  display: true,
					  text: 'Maanden'
					}
				},
				y: {
					display: true,
					title: {
					  display: true,
					  text: 'm3'
					}
				}
			} // scales
		}
	});
	console.log("chart ready");
}

function createChartDataContainer() {
	let ds = {};
	ds.labels = [];
	ds.datasets = [];
	return ds;
}

function createDatasetLINE(fill, color, label) {
	let ds = {};
	ds.fill = fill;
	ds.borderColor = color;
	ds.backgroundColor = color;
	ds.data = [];
	ds.label = label;
	ds.tension = 0.2;
	//no stack
	return ds;
}

//accumulate list with values
//convert from relative to absolute values
// before [3,3,5]
// after [0,3,6,11]
function accumulateArray(listValues)
{
	var listACCU=[];
	var nTotal=0;	
	for (var i=0; i < listValues.length; i++) 
	{		
		listACCU.push(nTotal);
		//fix a gap in the data
		if(isNaN(nTotal)) nTotal = 0;
		nTotal += listValues[i];
	}
	//add remaining total
	listACCU.push(nTotal);
	return listACCU;
}

//convert from absolute to relative values since start
//by using a prev value, add a new cell0, calc diff with this prev
// before [10,15,25],[9]
// after [0,1,6,16]
function normalizeArray(a, prev)
{
	b = new Array( a.length + 1);
	b[0] = 0;
	for(var i=0; i<a.length; i++)
	{
		b[i+1] = a[i] - prev;
	}
	return b;
}

//create a history container with depth = 3
function createHistoryContainer()
{
	let dc = {};
	dc.current 	  ={name:"", valid:false, data: []};
	dc.previous	  ={name:"", valid:false, data: []};
	dc.preprevious={name:"", valid:false, data: []};
	return dc;
}

//helper function to set all values to a value in an array
function fillArray(array, value) {
	for (var idx = 0; idx < array.length; idx++) {
		array[idx] = value;
	}
}

function fillArrayNULL(array) {
	fillArray(array, null);
}

function BurnupBootstrap() 
{
	console.log("BurnupBootstrap");

	createCharts();

	//create storage
	objStorage = new dsmr_dal_burnup();

	//set a flag that we are ready
	fLoadedBurnup = true;

	//refresh and schedule every 60sec
	//refreshData();
	//clearInterval(timerRefresh);
    //timerRefresh = setInterval(refreshData, 30 * 1000); // repeat every 20s

	//default view
	setViewMONTH();
}

// when network is slow, the bootstrap function of this file is not available 
// at the time it gets called from the main func resulting in a empty page.
// To ensure the bootstrap is loaded, we load again if needed at the two other entry points
function ensureBurnupLoaded()
{
	if( !fLoadedBurnup ) BurnupBootstrap();
}

//entry point from radiobutton 'Maand' in HTML
function setViewMONTH()
{
	ensureBurnupLoaded();
	sCurrentChart = "MONTH";
	getDays();
	clearInterval(timerRefresh);
    timerRefresh = setInterval(refreshData, 30 * 1000); // repeat every 20s
}

//entry point from radiobutton 'Jaar' in HTML
function setViewYEAR()
{
	ensureBurnupLoaded();
	sCurrentChart = "YEAR";
	getMonths();
	clearInterval(timerRefresh);
    timerRefresh = setInterval(refreshData, 30 * 1000); // repeat every 20s
}

function refreshData()
{
	switch(sCurrentChart){
		case "YEAR" : setViewYEAR(); break;
		case "MONTH": setViewMONTH(); break;
	}
}

function getMonths()
{
	var json = objStorage.getMonths();
	parseMonths(json);
}

function parseMonths(json)
{
	data = expandData_v2(json);
	
	//show months
	showMonths(data);
}

//copy of the DMSRindex.js but without the costs
function expandData_v2(dataIN) {
	console.log("expandData_v2()");
	console.log(dataIN);

	var i;
	var slotbefore;
	var AvoidSpikes = true;

	//deepcopy dataIN to dataOUT
	var data = structuredClone(dataIN);

	for (let x = dataIN.data.length + dataIN.actSlot; x > dataIN.actSlot; x--) 
	{
		i = x % dataIN.data.length;
		slotbefore = math.mod(i - 1, dataIN.data.length);

		var costs = 0;
		if (x != dataIN.actSlot) {
			if (AvoidSpikes && (data.data[slotbefore].values[0] == 0))
				data.data[slotbefore].values = data.data[i].values;//avoid gaps and spikes

			//values			
			data.data[i].p_edt1 = (data.data[i].values[0] - data.data[slotbefore].values[0]);
			data.data[i].p_edt2 = (data.data[i].values[1] - data.data[slotbefore].values[1]);
			data.data[i].p_ert1 = (data.data[i].values[2] - data.data[slotbefore].values[2]);
			data.data[i].p_ert2 = (data.data[i].values[3] - data.data[slotbefore].values[3]);
			data.data[i].p_gd 	= (data.data[i].values[4] - data.data[slotbefore].values[4]);
			data.data[i].water 	= (data.data[i].values[5] - data.data[slotbefore].values[5]);
			//sums
			data.data[i].p_ed 	= (data.data[i].p_edt1 + data.data[i].p_edt2);			
			data.data[i].p_er 	= (data.data[i].p_ert1 + data.data[i].p_ert2);
			data.data[i].p_edw  = (data.data[i].p_ed * 1000);
			data.data[i].p_erw 	= (data.data[i].p_er * 1000);
		}
		else {
			data.data[i].p_edt1 = (data.data[i].values[0]);
			data.data[i].p_edt2 = (data.data[i].values[1]);
			data.data[i].p_ert1 = (data.data[i].values[2]);
			data.data[i].p_ert2 = (data.data[i].values[3]);
			data.data[i].p_gd 	= (data.data[i].values[4]);
			data.data[i].water 	= (data.data[i].values[5]);
			data.data[i].p_ed 	= (data.data[i].p_edt1 + data.data[i].p_edt2);
			data.data[i].p_er 	= (data.data[i].p_ert1 + data.data[i].p_ert2);
			data.data[i].p_edw 	= (data.data[i].p_ed * 1000);
			data.data[i].p_erw 	= (data.data[i].p_er * 1000);
			
		}
	} //endfor
	console.log(data);
	console.log("~expandData_v2()");
	return data;
} // expandData_v2()

function showMonths(histdata)
{
	const [dcE1, dcG1] = prepareDataBURNUP_MONTHSINYEAR(histdata);
	
	objChart1.options.scales.x.title.text = "Maanden";
	objChart1.data = dcE1;
	objChart1.update();
	
	objChart2.options.scales.x.title.text = "Maanden";
	objChart2.data = dcG1;
	objChart2.update();
}

function getDays() {
	json = objStorage.getDays();
	parseDays(json);
}

function parseDays(json) {

	//expand data
	data = expandData_v2(json);

	//show days
	showDays( data );
}

function showDays(histdata)
{
	const [dcE1,dcG1] = prepareDataBURNUP_DAYSINMONTH(histdata);
	
	objChart1.options.scales.x.title.text = "Dagen";
	objChart1.data = dcE1;
	objChart1.update();

	objChart2.options.scales.x.title.text = "Dagen";
	objChart2.data = dcG1;
	objChart2.update();
}

function prepareDataBURNUP_DAYSINMONTH(histdata)
{
	//only this month and no water in the ceiling
	const [hcED, hcGD, hcWD] = convertRingBufferToStaticHistoryContainerDIM( histdata);

	//get day 0 values
	objToday = new Date();
	nYY = objToday.getFullYear() - 2000;
	nMM = objToday.getMonth()+1;
	//current month start is previous month end
	var itemCM = objStorage.getMonth(nYY,nMM-1)[0];
	var nCED0 = itemCM.values[0] + itemCM.values[1];
	var nCGD0 = itemCM.values[4];
	var nCWD0 = itemCM.values[5];

	//get meterreadung previous month
	if( nMM-2 <= 0)
	{
		nYY -= 1;
		nMM = 12;
	}
	else
		nMM -= 2;
	var itemPM = objStorage.getMonth(nYY,nMM)[0];
	var nPED0 = itemPM.values[0] + itemPM.values[1];
	var nPGD0 = itemPM.values[4];
	var nPWD0 = itemPM.values[5];

	//add absolute reading for day0 and calc relative values
	hcED.current.data = normalizeArray(hcED.current.data, nCED0);
	hcED.previous.data = normalizeArray(hcED.previous.data, nPED0);
	hcGD.current.data = normalizeArray(hcGD.current.data, nCGD0);
	hcGD.previous.data = normalizeArray(hcGD.previous.data, nPGD0);

	//create chartdata for each
	cdED = createChartDataContainerBURNUP_DAYS( hcED, listValuesCeilingE );
	cdGD = createChartDataContainerBURNUP_DAYS( hcGD, listValuesCeilingG );

	return [cdED, cdGD];
}

function createChartDataContainerBURNUP_DAYS(hcDELIVERED, ceiling)
{
	var nCurrentMM = new Date().getMonth();
	var nCurrentYYYY = new Date().getFullYear();
	var nCeilingMonth = ceiling[nCurrentMM];

	//get day 0 will return the number of days of the prev month
	var nDaycount = new Date(nCurrentYYYY, nCurrentMM+1, 0).getDate();

	//create ceiling array
	var listCeiling = new Array(32);
	fillArrayNULL(listCeiling);
	listCeiling[0] = 0;
	listCeiling[nDaycount] = nCeilingMonth;

	//create label array
	var listLABELS =[];
	for(i=0; i<32; i++)
	{
		sNumber = i.toString();
		listLABELS[i] = sNumber;
	}

	//create datasets
	var dcBX = createDatasetsForBURNUP_DAYS(listLABELS, hcDELIVERED, listCeiling);

	return dcBX
}

function createDatasetsForBURNUP_DAYS(listLABELS, hcBURNUP, listCeiling) {
	//create a datacontainer
	var cdcBurnup = createChartDataContainer();
	cdcBurnup.labels = listLABELS;

	//create datasets
	var dsE1 = createDatasetLINE(false, 'rgba(0, 0, 139, 1)', hcBURNUP.current.name);
	var dsE2 = createDatasetLINE(false, 'rgba(0, 0, 139, .25)', hcBURNUP.previous.name);
	var dsE3 = createDatasetLINE(false, 'red', "plafond");

	//attach data
	dsE1.data = hcBURNUP.current.data;
	dsE2.data = hcBURNUP.previous.data;

	//fix decimals in array
	formatArrayDecimals(dsE1.data, 3);
	formatArrayDecimals(dsE2.data, 3);

	//set additional config
	dsE1.spanGaps = true;
	dsE2.spanGaps = true;		//this dataset is not complete, so straighten line from 0 to first datapoint
	dsE3.spanGaps = true;		//for DAYS_IN_MONTH we only provide start and end

	//hide previous years by default	
	dsE2.hidden = true;

	//config for ceiling
	dsE3.data = listCeiling;
	dsE3.fill = "start";
	dsE3.backgroundColor = 'rgba(0, 255, 0, .125)';

	//add datasets to chartdata
	cdcBurnup.datasets.push(dsE1);
	if(hcBURNUP.previous.valid)  cdcBurnup.datasets.push(dsE2);
	cdcBurnup.datasets.push(dsE3);

	//return chartdata
	return cdcBurnup;
}

//create chartdata for the history ringbuffer MONTHS
function prepareDataBURNUP_MONTHSINYEAR(data) 
{
	//labels for a year
	var listLABELS = ["0"].concat( listMONTHS_SHORT );
	//calculate chart data
	const [hcED, hcGD, hcWD] = convertRingBufferToStaticHistoryContainerMIY(data);
	//create datasets for ELEKTRA
	var dcE1 = createChartDataContainerBURNUP_MONTHS(listLABELS, listValuesCeilingE, hcED);
	//create datasets for GAS
	var dcG1 = createChartDataContainerBURNUP_MONTHS(listLABELS, listValuesCeilingG, hcGD);
	//create datasets for WATER
	//var dcW1 = createChartDataContainerBURNUP_MONTHS(listLABELS, listValuesCeilingW, hcWD);
	return [dcE1,dcG1];
}

//create a 3 year datacontainer with months in year
function createHistoryContainerMIY(listLEGEND)
{
	var hcED = createHistoryContainer();
	hcED.current.name = listLEGEND[0];
	hcED.current.data = new Array(12);
	hcED.previous.name = listLEGEND[1];
	hcED.previous.data = new Array(12);
	hcED.preprevious.name = listLEGEND[2];
	hcED.preprevious.data = new Array(12);
	return hcED;
}

//create a 2 month datacontainer with 31 days in a month
function createHistoryContainerDIM(listLEGEND)
{
	var hcED = createHistoryContainer();
	hcED.current.name = listLEGEND[0];
	hcED.current.data = new Array(31);
	hcED.previous.name = listLEGEND[1];
	hcED.previous.data = new Array(31);
	//no preprevious needed
	return hcED;
}

// convert from a 24 months ringbuffer to a 3 static, 12 months buffers.
function convertRingBufferToStaticHistoryContainerMIY(data)
{
	//calculate current year
	var nCurrentYYYY = new Date().getFullYear();
	var nCurrentYY = nCurrentYYYY - 2000;
	var nPreviousYY = nCurrentYY-1;
	var nPrePreviousYY = nCurrentYY-2;

	var listLEGEND = [];
	listLEGEND.push(nCurrentYYYY);
	listLEGEND.push(nCurrentYYYY-1);
	listLEGEND.push(nCurrentYYYY-2);

	var hcED = createHistoryContainerMIY(listLEGEND);
	var hcGD = createHistoryContainerMIY(listLEGEND);
	var hcWD = createHistoryContainerMIY(listLEGEND);

	//filter values ELEKTRA and store in correct month	
	for (var idx = 0; idx < data.data.length; idx++) 
	{
		var item = data.data[idx];
		var timestamp = item.date;
		var date = timestamp.substring(0, 6);
		var nYY = parseInt(date.substring(0, 2));
		var nMM = parseInt(date.substring(2, 4)) - 1;
		//var nDD = parseInt(date.substring(4, 6));
		//var nHH = parseInt(timestamp.substring(6, 8));

		//this is the corrupt dataslot, so skip
		if( idx == data.actSlot+1) continue;

		//select correct array based on the year
		if(nYY == nCurrentYY) 
		{			
			hcED.current.data[nMM] = item.p_ed * 1.0;
			hcGD.current.data[nMM] = item.p_gd * 1.0;
			hcWD.current.data[nMM] = item.p_wd * 1.0;

			hcED.current.valid = true;
			hcGD.current.valid = true;
			hcWD.current.valid = true;
		}
		if(nYY == nPreviousYY)
		{			
			hcED.previous.data[nMM] = item.p_ed * 1.0;
			hcGD.previous.data[nMM] = item.p_gd * 1.0;
			hcWD.previous.data[nMM] = item.p_wd * 1.0;

			hcED.previous.valid = true;
			hcGD.previous.valid = true;
			hcWD.previous.valid = true;
		}
		if(nYY == nPrePreviousYY)
		{			
			hcED.preprevious.data[nMM] = item.p_ed * 1.0;
			hcGD.preprevious.data[nMM] = item.p_gd * 1.0;
			hcWD.preprevious.data[nMM] = item.p_wd * 1.0;

			hcED.preprevious.valid = true;
			hcGD.preprevious.valid = true;
			hcWD.preprevious.valid = true;
		}
	} //end for

	return [hcED, hcGD, hcWD];
}

// convert from a 14 days ringbuffer to a 2 static, 31 days buffers.
function convertRingBufferToStaticHistoryContainerDIM(data)
{
	//calculate current & prev month
	var nCurrentMM = new Date().getMonth();
	var nPreviousMM = nCurrentMM-1;
	//TODO: fix for month JAN

	//list with months
	var listLABELS = listMONTHS_SHORT;

	var listLEGEND = [];
	listLEGEND.push( listLABELS[nCurrentMM] );
	listLEGEND.push( listLABELS[nPreviousMM] );

	var hcED = createHistoryContainerDIM(listLEGEND);
	var hcGD = createHistoryContainerDIM(listLEGEND);
	var hcWD = createHistoryContainerDIM(listLEGEND);

	//filter values ELEKTRA and store in correct month	
	for (var idx = 0; idx < data.data.length; idx++) 
	{
		var item = data.data[idx];
		var timestamp = item.date;
		var date = timestamp.substring(0, 6);
		var nYY = parseInt(date.substring(0, 2));
		var nMM = parseInt(date.substring(2, 4)) - 1;
		var nDD = parseInt(date.substring(4, 6)) - 1;
		//var nHH = parseInt(timestamp.substring(6, 8));

		//this is the corrupt dataslot, so skip
		if( idx == (data.actSlot+1) % data.data.length) continue;

		//select correct array based on the year
		if(nMM == nCurrentMM)
		{
			var nED = item.values[0] + item.values[1];
			var nGD = item.values[4];
			var nWD = item.values[5];
						
			hcED.current.data[nDD] = nED;
			hcGD.current.data[nDD] = nGD;
			hcWD.current.data[nDD] = nWD;

			hcED.current.valid = true;
			hcGD.current.valid = true;
			hcWD.current.valid = true;
		}
		if(nMM == nPreviousMM)
		{
			var nED = item.values[0] + item.values[1];
			var nGD = item.values[4];
			var nWD = item.values[5];			

			hcED.previous.data[nDD] = nED;
			hcGD.previous.data[nDD] = nGD;
			hcWD.previous.data[nDD] = nWD;

			hcED.previous.valid = true;
			hcGD.previous.valid = true;
			hcWD.previous.valid = true;
		}
	} //end for

	return [hcED, hcGD, hcWD];
}

//accumelate the historycontainer and create a dataset
function createChartDataContainerBURNUP_MONTHS(listLABELS, listValuesCeiling, hcDELIVERED)
{	
	console.log("createChartDataContainerBURNUP_MONTHS()");

	//fill ceiling
	var listCeiling = accumulateArray(listValuesCeiling);

	//calculate accumulated months
	var hcBURNUP = structuredClone(hcDELIVERED);
	hcBURNUP.current.data = accumulateArray(hcDELIVERED.current.data);
	hcBURNUP.previous.data = accumulateArray(hcDELIVERED.previous.data);
	hcBURNUP.preprevious.data = accumulateArray(hcDELIVERED.preprevious.data);

	var dcBX = createDatasetsForBURNUP_MONTHS(listLABELS, hcBURNUP, listCeiling);
	return dcBX;
}

//create a dataset for the historycontainer, with labels and ceiling
function createDatasetsForBURNUP_MONTHS(listLABELS, hcBURNUP, listCeiling)
{
	//create a chartdatacontainer
	var cdcBurnup = createChartDataContainer();
	cdcBurnup.labels = listLABELS;

	//create datasets
	var dsE1 = createDatasetLINE(false, 'rgba(0, 0, 139, 1)', hcBURNUP.current.name);
	var dsE2 = createDatasetLINE(false, 'rgba(0, 0, 139, .25)', hcBURNUP.previous.name);
	var dsE3 = createDatasetLINE(false, 'rgba(0, 0, 139, .0625)', hcBURNUP.preprevious.name);
	var dsE4 = createDatasetLINE(false, 'red', "plafond");

	//attach data
	dsE1.data = hcBURNUP.current.data;
	dsE2.data = hcBURNUP.previous.data;
	dsE3.data = hcBURNUP.preprevious.data;

	//fix decimals in array
	formatArrayDecimals(dsE1.data, 3);
	formatArrayDecimals(dsE2.data, 3);
	formatArrayDecimals(dsE3.data, 3);

	//set additional config
	dsE3.spanGaps = true;		//this dataset is not complete, so straighten line from 0 to first datapoint
	
	//hide previous years by default	
	dsE2.hidden = true;
	dsE3.hidden = true;

	//config for ceiling
	dsE4.data = listCeiling;
	dsE4.fill = "start";
	dsE4.backgroundColor = 'rgba(0,255, 0, .125)';
	
	//add datasets to chartdata
	cdcBurnup.datasets.push(dsE1);
	if(hcBURNUP.previous.valid) 	cdcBurnup.datasets.push(dsE2);
	if(hcBURNUP.preprevious.valid) 	cdcBurnup.datasets.push(dsE3);
	cdcBurnup.datasets.push(dsE4);

	//return chartdata
	return cdcBurnup;
}

//limit every value in the array to n decimals
//toFixed() will convert all into strings, so also add  convertion back to Number
function formatArrayDecimals(data, decimals){
	for( var i=0; i<data.length; i++){
		if( !isNaN(data[i]) ) data[i] = Number(data[i].toFixed(decimals));
	}
}