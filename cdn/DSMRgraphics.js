/*
***************************************************************************  
**  Program  : DSMRgraphics.js, part of DSMRloggerAPI
**  Version  : v4.3.0
**
**  Copyright (c) 2020 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

//store last 90 TELEGRAM objects
const MAX_TELEGRAM_HISTORY = 15 * 6;
var listTELEGRAMS = [];
let TimerActual;
let actPoint        = 0;
let maxPoints       = 100;
var actLabel        = "-";
var gasDelivered    = 0;
var fGraphsReady = false;

var electrData = createChartDataContainer();
var actElectrData = createChartDataContainer();
var actGasData = createChartDataContainer();
var actWaterData = createChartDataContainer();

//helper functions for datacontainers and datasets
function createChartDataContainer(){
	let ds = {};	
	ds.labels = [];
	ds.datasets=[];
	return ds;
}

function createChartDataContainerWithStack()
{
	let ds = {};	
	ds.labels = [];
	ds.datasets=[];
  ds.stack = [];
	return ds;
}

function createDatasetBAR(fill, color, label, stack)
{
	let ds = {};
	ds.fill = fill;
	ds.borderColor = color;
	ds.backgroundColor = color;
	ds.data = [];
	ds.label = label;
	ds.stack = stack;
	return ds;
}

function createDatasetLINE(fill, color, label)
{
	let ds = {};
  ds.type= "line";
	ds.fill = fill;
	ds.borderColor = color;
	ds.backgroundColor = color;
	ds.data = [];
	ds.label = label;
	//no stack
	return ds;
}

var optionsGLOBAL = {
  plugins: {labels: false},
  responsive: true,
  maintainAspectRatio: true,
  scales: {
    yAxes: [{
      ticks : {
        beginAtZero : true
      },
      scaleLabel: {
        display: true,
        labelString: '---',
      },
    }]
  } // scales
}; // optionsGLOBAL

var optionsELEKTRA = structuredClone(optionsGLOBAL);
optionsELEKTRA.scales.yAxes[0].scaleLabel.labelString = "kWh";

var actElectrOptions = structuredClone(optionsGLOBAL);
actElectrOptions.scales.yAxes[0].scaleLabel.labelString = "kilo Watt";

var hourOptions = structuredClone(optionsGLOBAL);
hourOptions.scales.yAxes[0].scaleLabel.labelString = "Watt/Uur";

var dayOptions = structuredClone(optionsGLOBAL);
dayOptions.scales.yAxes[0].scaleLabel.labelString = "kWh";

var monthOptions = structuredClone(optionsGLOBAL);
monthOptions.scales.yAxes[0].scaleLabel.labelString = "kWh";

var optionsGAS = structuredClone(optionsGLOBAL);
optionsGAS.scales.yAxes[0].scaleLabel.labelString = "m3";

var optionsWATER = structuredClone(optionsGLOBAL);
optionsWATER.scales.yAxes[0].scaleLabel.labelString = "m3";


//----------------Chart's-------------------------------------------------------
var myElectrChart;
var myGasChart;
var myWaterChart;

function createChartsGRAPH()
{
  var ctx = null;
  ctx = document.getElementById("dataChart").getContext("2d");
  myElectrChart = new Chart(ctx, { type: 'bar', data: [], options: optionsELEKTRA});
  ctx = document.getElementById("gasChart").getContext("2d");
  myGasChart = new Chart(ctx, { type: 'line', data: [], options: optionsGAS });
  ctx = document.getElementById("waterChart").getContext("2d");
  myWaterChart = new Chart(ctx, { type: 'line', data: [], options: optionsWATER });
  fGraphsReady = true;
}
function ensureChartsReady()
{
  if( !fGraphsReady) createChartsGRAPH();
}

  //============================================================================  
  function renderElectrChart(dataSet, options) {
    //console.log("Now in renderElectrChart() ..");
    
    if (myElectrChart) {
      myElectrChart.destroy();
    }

    var ctxElectr = document.getElementById("dataChart").getContext("2d");
    myElectrChart = new Chart(ctxElectr, {
      type: 'bar',
      data: dataSet,
      options: options,
    });
    
  } // renderElectrChart()

  //============================================================================  
  function renderWaterChart(dataSet) {
    //console.log("Now in renderGasChart() ..");
    
    if (myWaterChart) {
      myWaterChart.destroy();
    }

    var ctxWater = document.getElementById("waterChart").getContext("2d");
    myWaterChart = new Chart(ctxWater, {
      type: 'line',
      data: dataSet,
      options: optionsWATER
    });
    
  } // renderWaterChart()
  
  //============================================================================  
  function renderGasChart(dataSet) {
    //console.log("Now in renderGasChart() ..");
    
    if (myGasChart) {
      myGasChart.destroy();
    }

    var ctxGas = document.getElementById("gasChart").getContext("2d");
    myGasChart = new Chart(ctxGas, {
      type: 'line',
      data: dataSet,
      options: optionsGAS
    });
    
  } // renderGasChart()
  
  
  //============================================================================  
  function showHistGraph(data, type)
  {
    ensureChartsReady();

    copyDataToChart(data, type);

    var labelString = "kWh";
    if( type == "Hours") labelString = "Watt";
    myElectrChart.options.scales.yAxes[0].scaleLabel.labelString = labelString;
    myElectrChart.data = electrData;
    myElectrChart.update();
    
    if (HeeftGas) {           
      myGasChart.data = gasData;
      labelString = "m3";
      if( type == "Hours") labelString = "dm3";
      if ( Dongle_Config == "p1-q") labelString = "kJ";
      myGasChart.options.scales.yAxes[0].scaleLabel.labelString = labelString;
      myGasChart.update();
      document.getElementById("gasChart").style.display   = "block";
    }

    if (HeeftWater) {
      myWaterChart.data = waterData;
      labelString = "m3";
      if( type == "Hours") labelString = "dm3";
      myWaterChart.options.scales.yAxes[0].scaleLabel.labelString = labelString;
    	myWaterChart.update();
		  document.getElementById("waterChart").style.display = "block";
    }

    //--- hide table
    document.getElementById("lastHours").style.display  = "none";
    document.getElementById("lastDays").style.display   = "none";
    document.getElementById("lastMonths").style.display = "none";
    //--- show canvas
	  document.getElementById("dataChart").style.display  = Dongle_Config == "p1-q" ? "none" : "block";

  } // showHistGraph()
    
  //============================================================================  
  function showMonthsGraph(data, type)
  {
    ensureChartsReady();
    
    //console.log("Now in showMonthsGraph()..");
    copyMonthsToChart(data, type);
    myElectrChart.data = electrData;
    myElectrChart.update();

    if (HeeftGas) {
      myGasChart.data = gasData;
      var labelString = SQUARE_M_CUBED;
      if ( Dongle_Config == "p1-q") labelString = "kJ";
      myGasChart.options.scales.yAxes[0].scaleLabel.labelString = labelString;
      myGasChart.update();
      document.getElementById("gasChart").style.display = "block";
    }

    if (HeeftWater) {
      myWaterChart.data = waterData;
      myWaterChart.options.scales.yAxes[0].scaleLabel.labelString = SQUARE_M_CUBED;
      myWaterChart.update();
      document.getElementById("waterChart").style.display = "block";
    }
  
    //--- hide table
    document.getElementById("lastHours").style.display  = "none";
    document.getElementById("lastDays").style.display   = "none";
    document.getElementById("lastMonths").style.display = "none";
    //--- show canvas
	  document.getElementById("dataChart").style.display  = Dongle_Config == "p1-q" ? "none" : "block";
    //reset cost checkbox
    document.getElementById('mCOST').checked   = false;

  } // showMonthsGraph()
  
  
  //============================================================================  
  function copyDataToChart(data, type)
  {
    electrData = createChartDataContainerWithStack();
    gasData = createChartDataContainerWithStack();
	  waterData = createChartDataContainerWithStack();
    
    // idx 0 => ED
    var dsE1 = createDatasetBAR('false', 'red', "Gebruikt", "STACK");
    
    // idx 0 => ER
    var dsE2 = createDatasetBAR('false', 'green', "Opgewekt", "STACK");
    
    // idx 0 => GAS
    var dsG1 = createDatasetLINE('false', 'blue', "Gas Gebruikt");
    if ( Dongle_Config == "p1-q") dsG1.label = "Warmte Gebruikt";
   
    // idx 0 => WATER
    var dsW1 = createDatasetLINE('false', 'blue', "Water Gebruikt");    
    
  //      console.log("data.actSlot "+data.actSlot);
  //      console.log("data.data.length "+data.data.length);
  
    var p = 0;                
    for (let y=data.data.length + data.actSlot; y > data.actSlot+1; y--)
    {	
      var i = y % data.data.length;
    
      // adds x axis labels (timestamp)
      electrData.labels.push(formatGraphDate(type, data.data[i].date)); 
      gasData.labels.push(   formatGraphDate(type, data.data[i].date)); 
	    waterData.labels.push( formatGraphDate(type, data.data[i].date));

      //add data to the sets
      if (type == "Hours")
      {
        if (data.data[i].p_edw >= 0) dsE1.data[p] = (data.data[i].p_edw *  1.0);
        if (data.data[i].p_erw >= 0) dsE2.data[p] = (data.data[i].p_erw * -1.0);
      }
      else
      {
        if (data.data[i].p_ed >= 0) dsE1.data[p] = (data.data[i].p_ed *  1.0).toFixed(3);
        if (data.data[i].p_er >= 0) dsE2.data[p] = (data.data[i].p_er * -1.0).toFixed(3);
      }
      if (data.data[i].p_gd  >= 0)  dsG1.data[p] = (data.data[i].p_gd * 1000.0).toFixed(0);
      if (data.data[i].water  >= 0) dsW1.data[p] = (data.data[i].water * 1000.0).toFixed(0);
	    p++;
    } // for i ..

    //push all the datasets to the container
    electrData.datasets.push(dsE1);
    electrData.datasets.push(dsE2);
    gasData.datasets.push(dsG1);
    waterData.datasets.push(dsW1);

  } // copyDataToChart()
  
  
  //============================================================================  
  function copyMonthsToChart(data)
  {
    console.log("Now in copyMonthsToChart()..");

    electrData= createChartDataContainerWithStack();    
    gasData   = createChartDataContainerWithStack();    
	  waterData = createChartDataContainerWithStack();
    
    // idx 0 => ED
    var dsED1 = createDatasetBAR('false', 'red', "Gebruikt deze periode", "DP");    

    // idx 1 => ER
    var dsER1 = createDatasetBAR('false', 'green', "Opgewekt deze periode", "DP");    
    
    // idx 2 => ED -1
    var dsED2 = createDatasetBAR('false', 'orange', "Gebruikt vorige periode", "RP");    

    // idx 3 => ER -1
    var dsER2 = createDatasetBAR('false', 'lightgreen', "Opgewekt vorige periode", "RP");

    // idx 0 => GD
    var dsGD1 =  createDatasetLINE('false', "blue", "Gas deze periode");
    if(Dongle_Config == "p1-q") dsGD1.label = "Warmte deze periode";    
    
    // idx 0 => GD -1
    var dsGD2 =  createDatasetLINE('false', "blue", "Gas vorige periode");
    if(Dongle_Config == "p1-q") dsGD2.label = "Warmte vorige periode";

    var dsW1 =  createDatasetLINE('false', "blue", "Water deze periode");
    var dsW2 =  createDatasetLINE('false', "lightblue", "Water vorige periode");
       
    //console.log("there are ["+data.data.length+"] rows");
  
	  var start = data.data.length + data.actSlot ; //  maar 1 jaar ivm berekening jaar verschil
    var stop = start - 12;
    var i;
    var slotyearbefore = 0;
    var p        = 0;
  	for (let index=start; index>stop; index--)
    {  
      i = index % data.data.length;
      slotyearbefore = math.mod(i-12,data.data.length);

      //add labels
      electrData.labels.push(formatGraphDate("Months", data.data[i].date));
      gasData.labels.push(   formatGraphDate("Months", data.data[i].date));
      waterData.labels.push( formatGraphDate("Months", data.data[i].date));
      
      //add data to the datatsets
      if (data.data[i].p_ed >= 0) {
      	dsED1.data[p]  = (data.data[i].p_ed *  1.0).toFixed(3);
		    dsED2.data[p]  = (data.data[slotyearbefore].p_ed *  1.0).toFixed(3);
	    }
      
	    if (data.data[i].p_er >= 0) {
	  	  dsER1.data[p]  = (data.data[i].p_er * -1.0).toFixed(3);
      	dsER2.data[p]  = (data.data[slotyearbefore].p_er * -1.0).toFixed(3);
      }
      
      if (data.data[i].p_gd >= 0) {
		    dsGD1.data[p]     = data.data[i].p_gd;
		    dsGD2.data[p]     = data.data[slotyearbefore].p_gd;
	    }
	    if (data.data[i].water >= 0) {
		    dsW1.data[p]     = data.data[i].water;
		    dsW2.data[p]     = data.data[slotyearbefore].water;
	    }
      p++;
    }//endfor

    //add datasets to the container
    electrData.datasets.push(dsED1);
    electrData.datasets.push(dsER1);
    electrData.datasets.push(dsED2);
    electrData.datasets.push(dsER2);
    gasData.datasets.push(dsGD1);
    gasData.datasets.push(dsGD2);
    waterData.datasets.push(dsW1);
    waterData.datasets.push(dsW2); 

    //--- hide months Table
    document.getElementById("lastMonths").style.display = "none";
    //--- show canvas
    document.getElementById("dataChart").style.display  = "block";

  } // copyMonthsToChart()
    
  //============================================================================  
  function copyActualToChart(data) 
  {
    //convert telegram
    var objTelegram = parseTelegramData(data);

    //add to telegramhistory
    if(listTELEGRAMS.length >= MAX_ACTUAL_HISTORY) listTELEGRAMS.shift();
    listTELEGRAMS.push(objTelegram);
  }
 
  //============================================================================  
  function initActualGraph()
  {
    //reset pos
    actPoint = 0;
    if(!fGraphsReady) createChartsGRAPH();
  
  } // initActualGraph()

  //============================================================================  
  function showActualGraph()
  {
    if (activeTab != "bActualTab") return;

    //--- hide Table
    document.getElementById("actual").style.display    = "none";
    //--- show canvas
    document.getElementById("dataChart").style.display = "block";
    document.getElementById("gasChart").style.display  = "block";

    //display current listTELEGRAMS
    const [dcEX, dcGX] = createDataContainersACTUAL( listTELEGRAMS );
    
    //update Elektra
    myElectrChart.data = dcEX;
    myElectrChart.update(0);
    
    //update Gas
    myGasChart.data = dcGX;
    var labelString = "dm3";
    if( Dongle_Config == "p1-q") labelString = "GJ * 1000";
    myGasChart.options.scales.yAxes[0].scaleLabel.labelString = labelString;    
    myGasChart.update(0);

    //update water???
    //TODO

  } // showActualGraph()
  
  
  //============================================================================  
  function formatGraphDate(type, dateIn) 
  {
    let dateOut = "";
    if (type == "Hours")
    {
      dateOut = "("+dateIn.substring(4,6)+") "+dateIn.substring(6,8);
    }
    else if (type == "Days")
      dateOut = [recidToWeekday(dateIn), dateIn.substring(4,6)+"-"+dateIn.substring(2,4)];
    else if (type == "Months")
    {
      let MM = parseInt(dateIn.substring(2,4))
      dateOut = monthNames[MM];
    }
    else if (type == "Actual")
    {
      dateOut = dateIn.substring(6,8)+":"+dateIn.substring(8,10)+":"+dateIn.substring(10,12);
    }
    else
      dateOut = "20"+dateIn.substring(0,2)+"-"+dateIn.substring(2,4)+"-"+dateIn.substring(4,6)+":"+dateIn.substring(6,8);
    
    return dateOut;
  }

  //format timestamp
  //input: 230215223319W
  //output: "22:33:19"
  function formatHHMMSS(itemvalue)
  {
    //230215 223319 W
    var time = itemvalue.slice(6,12);
    var nHH = time.slice(0,2);
    var nMM = time.slice(2,4);
    var nSS = time.slice(4,6);
    return ""+nHH+":"+nMM+":"+nSS;
  }

  function parseTelegramData( data )
  {
    var telegram = new Map();
    for (key in data) 
    {
      item = data[key];
      // key: {value:0, unit:""}
      switch( key )
      {
        case "timestamp": telegram.set('timestamp', formatHHMMSS(item.value) ); break;

        case "power_delivered_l1": telegram.set('pdl1', (item.value).toFixed(3) ); break;
        case "power_delivered_l2": telegram.set('pdl2', (item.value).toFixed(3) ); break;
        case "power_delivered_l3": telegram.set('pdl3', (item.value).toFixed(3) ); break;
        case "power_delivered":    telegram.set('pd',   (item.value).toFixed(3) ); break;
        
        case "power_returned_l1": telegram.set('prl1', (item.value).toFixed(3) ); break;
        case "power_returned_l2": telegram.set('prl2', (item.value).toFixed(3) ); break;
        case "power_returned_l3": telegram.set('prl3', (item.value).toFixed(3) ); break;
        case "power_returned":    telegram.set('pr',   (item.value).toFixed(3) ); break;

        case "gas_delivered_timestamp": 
          telegram.set('gas_timestamp', formatHHMMSS(item.value) );
          break;

        case "gas_delivered": 
          telegram.set("gd", item.value ); 
          break;

        default:
          //nothing

      }//endswitch
    }//endfor

    return telegram;
  }

  function copyActualHistoryToChart(histdata) 
  {
    listTELEGRAMS = [];
    for(var i=0; i<histdata.length; i++)
    {
      //convert telegram
      var objTelegram = parseTelegramData( histdata[i] );
    
      //add to telegramhistory
      listTELEGRAMS.push(objTelegram);
    }
  }

  //convert the telegram list to datasets
  function createDataContainersACTUAL( listTELEGRAMS )
  {
    var dcEX = createChartDataContainerWithStack();
    var dcGX = createChartDataContainerWithStack();

    //ED L1..3
    var dsED1 = createDatasetBAR('false', 'red', "Gebruikt L1", "A");    
    var dsED2 = createDatasetBAR('false', 'tomato', "Gebruikt L2", "A");    
    var dsED3 = createDatasetBAR('false', 'salmon', "Gebruikt L3", "A");

    //ER L1..3
    var dsER1 = createDatasetBAR('false', 'yellowgreen', "Opgewekt L1", "A");
    var dsER2 = createDatasetBAR('false', 'springgreen', "Opgewekt L2", "A");
    var dsER3 = createDatasetBAR('false', 'green',       "Opgewekt L3", "A");
    
    //GD
    var dsG1 = createDatasetLINE('false', 'blue', "Gas verbruikt");
    if(Dongle_Config == "p1-q") dsG1.label = "Warmte verbruikt";
    dsG1.spanGaps = true;

    // Fill datasets
    var gd_prev = "";
    var ts_prev = "";
    for( var i=0; i<listTELEGRAMS.length; i++)
    {
      telegram = listTELEGRAMS[i];

      dcEX.labels.push(telegram.get("timestamp"));

      dsED1.data.push( telegram.get("pdl1") );
      dsED2.data.push( telegram.get("pdl2") );
      dsED3.data.push( telegram.get("pdl3") );

      dsER1.data.push( telegram.get("prl1") );
      dsER2.data.push( telegram.get("prl2") );
      dsER3.data.push( telegram.get("prl3") );
      
      if(i==0){        
        gd_prev = telegram.get("gd");
        ts_prev = telegram.get("gas_timestamp");
        dsG1.data.push(0);
        dsG1.data.push(null);
        dcGX.labels.push( ts_prev);
        dcGX.labels.push( telegram.get("timestamp") );
      }
      else
      {
        gd_ts = telegram.get("gas_timestamp");        
        if( gd_ts != ts_prev)
        {
          var gd = telegram.get("gd");
          dsG1.data.push( gd - gd_prev );
          ts_prev = gd_ts;
          gd_prev = gd;
        }
        else{
          dsG1.data.push( null );
        }
        dcGX.labels.push( telegram.get("timestamp") );
      }
    }

    //add datasets
    dcEX.datasets.push(dsED1);
    if( Phases > 1) dcEX.datasets.push(dsED2);
    if( Phases > 2) dcEX.datasets.push(dsED3);
    if (Injection) {
      dcEX.datasets.push(dsER1);
      if( Phases > 1) dcEX.datasets.push(dsER2);
      if( Phases > 2) dcEX.datasets.push(dsER3);
    }
    dcGX.datasets.push(dsG1);

    return [dcEX, dcGX];
  }

  
/*
***************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the
* following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
* 
***************************************************************************
*/
