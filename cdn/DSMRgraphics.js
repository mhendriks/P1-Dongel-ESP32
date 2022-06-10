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

let TimerActual;
let actPoint        = 0;
let maxPoints       = 100;
var actLabel        = "-";
var gasDelivered    = 0;

var electrData          = {};     // declare an object
electrData.labels       = [];     //  add 'labels' element to object (X axis)
electrData.datasets     = [];     //  add 'datasets' array element to object

var actElectrData       = {};     // declare an object
actElectrData.labels    = [];     //  add 'labels' element to object (X axis)
actElectrData.datasets  = [];     //  add 'datasets' array element to object

var actGasData          = {};     // declare an object
actGasData.labels       = [];     //  add 'labels' element to object (X axis)
actGasData.datasets     = [];     //  add 'datasets' array element to object

var actWaterData          = {};     // declare an object
actWaterData.labels       = [];     //  add 'labels' element to object (X axis)
actWaterData.datasets     = [];     //  add 'datasets' array element to object


var actElectrOptions = {
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
              labelString: 'kilo Watt',
            },
          }]
        } // scales
      }; // options

var hourOptions = {
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
              labelString: 'Watt/Uur',
            },
          }]
        } // scales
      }; // options

var dayOptions = {
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
              labelString: 'kWh',
            },
          }]
        } // scales
      }; // options

var monthOptions = {
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
              labelString: 'kWh',
            },
          }]
        } // scales
      }; // options

//----------------Chart's-------------------------------------------------------
var myElectrChart;
var myGasChart;
var myWaterChart;

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
  function renderWaterChart(dataSet, labelString) {
    //console.log("Now in renderGasChart() ..");
    
    if (myWaterChart) {
      myWaterChart.destroy();
    }

    var ctxWater = document.getElementById("waterChart").getContext("2d");
    myWaterChart = new Chart(ctxWater, {
      type: 'line',
      data: dataSet,
      options: {
        responsive: true,
        maintainAspectRatio: true,
        scales: {
          yAxes: [{
            ticks : {
              beginAtZero : true,
            },
            scaleLabel: {
              display: true,
              labelString: labelString,
            },
          }]
        } // scales
      } // options

    });
    
  } // renderWaterChart()
  //============================================================================  
  function renderGasChart(dataSet, labelString) {
    //console.log("Now in renderGasChart() ..");
    
    if (myGasChart) {
      myGasChart.destroy();
    }

    var ctxGas = document.getElementById("gasChart").getContext("2d");
    myGasChart = new Chart(ctxGas, {
      type: 'line',
      data: dataSet,
      options: {
        responsive: true,
        maintainAspectRatio: true,
        scales: {
          yAxes: [{
            ticks : {
              beginAtZero : true,
            },
            scaleLabel: {
              display: true,
              labelString: labelString,
            },
          }]
        } // scales
      } // options

    });
    
  } // renderGasChart()
  
  
  //============================================================================  
  function showHistGraph(data, type)
  {
    //console.log("Now in showHistGraph()..");
    copyDataToChart(data, type);
    if (type == "Hours")
          renderElectrChart(electrData, hourOptions);
    else  renderElectrChart(electrData, dayOptions);
    myElectrChart.update();
    
    //renderGasChart(gasData, actGasOptions);
    if (HeeftGas) {
		renderGasChart(gasData, "dm3");
		myGasChart.update();
		document.getElementById("gasChart").style.display   = "block";
    }

    //renderWaterChart(gasData, actGasOptions);
    if (HeeftWater) {
        renderWaterChart(waterData, "dm3");
    	myWaterChart.update();
		document.getElementById("waterChart").style.display = "block";
     } 
    //--- hide table
    document.getElementById("lastHours").style.display  = "none";
    document.getElementById("lastDays").style.display   = "none";
    document.getElementById("lastMonths").style.display = "none";
    //--- show canvas
    document.getElementById("dataChart").style.display  = "block";


  } // showHistGraph()
  
  
  //============================================================================  
  function showMonthsGraph(data, type)
  {
    //console.log("Now in showMonthsGraph()..");
    copyMonthsToChart(data, type);
    renderElectrChart(electrData, monthOptions);
    myElectrChart.update();
    //renderGasChart(gasData, actGasOptions);
    if (HeeftGas) {
		renderGasChart(gasData, "m3");
		myGasChart.update();
		document.getElementById("gasChart").style.display = "block";
    }  
	if (HeeftWater) {
		renderWaterChart(waterData, "m3");
		myWaterChart.update();
		document.getElementById("waterChart").style.display = "block";
	}
  
    //--- hide table
    document.getElementById("lastHours").style.display  = "none";
    document.getElementById("lastDays").style.display   = "none";
    document.getElementById("lastMonths").style.display = "none";
    //--- show canvas
    document.getElementById("dataChart").style.display  = "block";    

    document.getElementById('mCOST').checked   = false;

  } // showMonthsGraph()
  
  
  //============================================================================  
  function copyDataToChart(data, type)
  {
//    console.log("Now in copyDataToChart()..");


    electrData    = {};     // empty electrData
    electrData.labels   = [];     // empty .labels
    electrData.stack    = [];     // empty .stack
    electrData.datasets = [];     // empty .datasets

    gasData       = {};     // empty gasData
    gasData.labels      = [];     // empty .labels
    gasData.stack       = [];     // empty .stack
    gasData.datasets    = [];     // empty .datasets

	waterData       = {};     // empty gasData[]
    waterData.labels      = [];     // empty .labels
    waterData.stack       = [];     // empty .stack
    waterData.datasets    = [];     // empty .datasets
    
    // idx 0 => ED
    electrData.datasets.push({}); //create a new dataset
    electrData.datasets[0].fill            = 'false';
    electrData.datasets[0].borderColor     = "red";
    electrData.datasets[0].backgroundColor = "red";
    electrData.datasets[0].data            = []; //contains the 'Y; axis data
    electrData.datasets[0].label           = "Gebruikt"; //"S"+s; //contains the 'Y; axis label
    electrData.datasets[0].stack           = "STACK"
    // idx 0 => ER
    electrData.datasets.push({}); //create a new dataset
    electrData.datasets[1].fill            = 'false';
    electrData.datasets[1].borderColor     = "green";
    electrData.datasets[1].backgroundColor = "green";
    electrData.datasets[1].data            = []; //contains the 'Y; axis data
    electrData.datasets[1].label           = "Opgewekt"; //"S"+s; //contains the 'Y; axis label
    electrData.datasets[1].stack           = "STACK"
    
    // idx 0 => GAS
    gasData.datasets.push({}); //create a new dataset
    gasData.datasets[0].fill               = 'false';
    gasData.datasets[0].borderColor        = "blue";
    gasData.datasets[0].backgroundColor    = "blue";
    gasData.datasets[0].data               = []; //contains the 'Y; axis data
    gasData.datasets[0].label              = "Gas Gebruikt"; //"S"+s; //contains the 'Y; axis label
 
 // idx 0 => WATER
    waterData.datasets.push({}); //create a new dataset
    waterData.datasets[0].fill               = 'false';
    waterData.datasets[0].borderColor        = "blue";
    waterData.datasets[0].backgroundColor    = "blue";
    waterData.datasets[0].data               = []; //contains the 'Y; axis data
    waterData.datasets[0].label              = "Water Gebruikt"; //"S"+s; //contains the 'Y; axis label
    
  //      console.log("data.actSlot "+data.actSlot);
  //      console.log("data.data.length "+data.data.length);
  
    var p = 0;                
    for (let y=data.data.length + data.actSlot; y > data.actSlot+1; y--)
    {	
      var i = y % data.data.length;

    //console.log("i: "+i);            
    //console.log("y: "+y);
    //console.log("slotbefore: "+slotbefore);
    
      //console.log("["+i+"] label["+data.data[i].date+"] => slotbefore["+slotbefore+"]");
      // adds x axis labels (timestamp)
      electrData.labels.push(formatGraphDate(type, data.data[i].date)); 
      gasData.labels.push(formatGraphDate(type, data.data[i].date)); 
	  waterData.labels.push(formatGraphDate(type, data.data[i].date));
      if (type == "Hours")
      {
        if (data.data[i].p_edw >= 0) electrData.datasets[0].data[p]  = (data.data[i].p_edw *  1.0);
        if (data.data[i].p_erw >= 0) electrData.datasets[1].data[p]  = (data.data[i].p_erw * -1.0);
      }
      else
      {
        if (data.data[i].p_ed >= 0) electrData.datasets[0].data[p]  = (data.data[i].p_ed *  1.0).toFixed(3);
        if (data.data[i].p_er >= 0) electrData.datasets[1].data[p]  = (data.data[i].p_er * -1.0).toFixed(3);
      }
      if (data.data[i].p_gd  >= 0) gasData.datasets[0].data[p]      = (data.data[i].p_gd * 1000.0).toFixed(0);
      if (data.data[i].water  >= 0) waterData.datasets[0].data[p]   = (data.data[i].water * 1000.0).toFixed(0);
	p++;
    } // for i ..

  } // copyDataToChart()
  
  
  //============================================================================  
  function copyMonthsToChart(data)
  {
    console.log("Now in copyMonthsToChart()..");
    
    electrData    = {};     // empty electrData
    electrData.labels   = [];     // empty .labels
    electrData.stack    = [];     // empty .stack
    electrData.datasets = [];     // empty .datasets
    
    gasData       = {};     // empty electrData
    gasData.labels      = [];     // empty .labels
    gasData.stack       = [];     // empty .stack
    gasData.datasets    = [];     // empty .datasets
    
	waterData       = {};     // empty gasData[]
    waterData.labels      = [];     // empty .labels
    waterData.stack       = [];     // empty .stack
    waterData.datasets    = [];     // empty .datasets
    
    // idx 0 => ED
    electrData.datasets.push({}); //create a new dataset
    electrData.datasets[0].fill            = 'false';
    electrData.datasets[0].borderColor     = "red";
    electrData.datasets[0].backgroundColor = "red";
    electrData.datasets[0].data            = []; //contains the 'Y; axis data
    electrData.datasets[0].label           = "Gebruikt deze periode"; //"S"+s; //contains the 'Y; axis label
    electrData.datasets[0].stack           = "DP"
    // idx 1 => ER
    electrData.datasets.push({}); //create a new dataset
    electrData.datasets[1].fill            = 'false';
    electrData.datasets[1].borderColor     = "green";
    electrData.datasets[1].backgroundColor = "green";
    electrData.datasets[1].data            = []; //contains the 'Y; axis data
    electrData.datasets[1].label           = "Opgewekt deze Periode"; //"S"+s; //contains the 'Y; axis label
    electrData.datasets[1].stack           = "DP"
    // idx 2 => ED -1
    electrData.datasets.push({}); //create a new dataset
    electrData.datasets[2].fill            = 'false';
    electrData.datasets[2].borderColor     = "orange";
    electrData.datasets[2].backgroundColor = "orange";
    electrData.datasets[2].data            = []; //contains the 'Y; axis data
    electrData.datasets[2].label           = "Gebruikt vorige periode"; //"S"+s; //contains the 'Y; axis label
    electrData.datasets[2].stack           = "RP"
    // idx 3 => ER -1
    electrData.datasets.push({}); //create a new dataset
    electrData.datasets[3].fill            = 'false';
    electrData.datasets[3].borderColor     = "lightgreen";
    electrData.datasets[3].backgroundColor = "lightgreen";
    electrData.datasets[3].data            = []; //contains the 'Y; axis data
    electrData.datasets[3].label           = "Opgewekt vorige Periode"; //"S"+s; //contains the 'Y; axis label
    electrData.datasets[3].stack           = "RP"
    // idx 0 => GD
    gasData.datasets.push({}); //create a new dataset
    gasData.datasets[0].fill            = 'false';
    gasData.datasets[0].borderColor     = "blue";
    gasData.datasets[0].backgroundColor = "blue";
    gasData.datasets[0].data            = []; //contains the 'Y; axis data
    gasData.datasets[0].label           = "Gas deze Periode"; //"S"+s; //contains the 'Y; axis label
    // idx 0 => GD -1
    gasData.datasets.push({}); //create a new dataset
    gasData.datasets[1].fill            = 'false';
    gasData.datasets[1].borderColor     = "lightblue";
    gasData.datasets[1].backgroundColor = "lightblue";
    gasData.datasets[1].data            = []; //contains the 'Y; axis data
    gasData.datasets[1].label           = "Gas vorige Periode"; //"S"+s; //contains the 'Y; axis label
    // idx 0 => WATER
    waterData.datasets.push({}); //create a new dataset
    waterData.datasets[0].fill            = 'false';
    waterData.datasets[0].borderColor     = "blue";
    waterData.datasets[0].backgroundColor = "blue";
    waterData.datasets[0].data            = []; //contains the 'Y; axis data
    waterData.datasets[0].label           = "Water deze Periode"; //"S"+s; //contains the 'Y; axis label
    // idx 0 => WATER -1    
	waterData.datasets.push({}); //create a new dataset
    waterData.datasets[1].fill            = 'false';
    waterData.datasets[1].borderColor     = "lightblue";
    waterData.datasets[1].backgroundColor = "lightblue";
    waterData.datasets[1].data            = []; //contains the 'Y; axis data
    waterData.datasets[1].label           = "Water vorige Periode"; //"S"+s; //contains the 'Y; axis label
    
    
    //console.log("there are ["+data.data.length+"] rows");
  
	var start = data.data.length + data.actSlot ; //  maar 1 jaar ivm berekening jaar verschil
    var stop = start - 12;
    var i;
    var slotyearbefore = 0;
    var p        = 0;
  	for (let index=start; index>stop; index--)
    {  i = index % data.data.length;
      	slotyearbefore = math.mod(i-12,data.data.length);

      electrData.labels.push(formatGraphDate("Months", data.data[i].date)); // adds x axis labels (timestamp)
      gasData.labels.push(formatGraphDate("Months", data.data[i].date)); // adds x axis labels (timestamp)
      waterData.labels.push(formatGraphDate("Months", data.data[i].date)); // adds x axis labels (timestamp)
      //electrData.labels.push(p); // adds x axis labels (timestamp)
      if (data.data[i].p_ed >= 0) {
      	electrData.datasets[0].data[p]  = (data.data[i].p_ed *  1.0).toFixed(3);
		electrData.datasets[2].data[p]  = (data.data[slotyearbefore].p_ed *  1.0).toFixed(3);
	  }
      
	  if (data.data[i].p_er >= 0) {
	  	electrData.datasets[1].data[p]  = (data.data[i].p_er * -1.0).toFixed(3);
      	electrData.datasets[3].data[p]  = (data.data[slotyearbefore].p_er * -1.0).toFixed(3);
      }
      
      if (data.data[i].p_gd >= 0) {
		  gasData.datasets[0].data[p]     = data.data[i].p_gd;
		  gasData.datasets[1].data[p]     = data.data[slotyearbefore].p_gd;
	  }
	  if (data.data[i].water >= 0) {
		  waterData.datasets[0].data[p]     = data.data[i].water;
		  waterData.datasets[1].data[p]     = data.data[slotyearbefore].water;
	  }
      p++;
    }
    //--- hide months Table
    document.getElementById("lastMonths").style.display = "none";
    //--- show canvas
    document.getElementById("dataChart").style.display  = "block";
//     document.getElementById("gasChart").style.display   = "block";
// 	document.getElementById("waterChart").style.display   = "block";


  } // copyMonthsToChart()
    
  
  //============================================================================  
  function copyActualToChart(data)
  {
    //console.log("Now in copyActualToChart()..");
    
    for (i in data)
    {
      //console.log("i ="+i+"] value["+data[i].value+"]");
      if (i == "timestamp")  
      {
        //console.log("i["+i+"] label["+data[i].value+"]");
        if (data[i].value == actLabel)
        {
          console.log("actLabel["+actLabel+"] == value["+data[i].value+"] =>break!");
          return;
        }
        actElectrData.labels.push(formatGraphDate("Actual", data[i].value)); // adds x axis labels (timestamp)
        actGasData.labels.push(formatGraphDate("Actual", data[i].value)); // adds x axis labels (timestamp)
        actLabel = data[i].value;
      }
      
      if (i == "power_delivered_l1") 
        actElectrData.datasets[0].data[actPoint]  = (data[i].value).toFixed(3);
      if (i == "power_delivered_l2") 
        actElectrData.datasets[1].data[actPoint]  = (data[i].value).toFixed(3);
      if (i == "power_delivered_l3") 
        actElectrData.datasets[2].data[actPoint]  = (data[i].value).toFixed(3);
      if (i == "power_returned_l1")  
        actElectrData.datasets[3].data[actPoint]  = (data[i].value * -1.0).toFixed(3);
      if (i == "power_returned_l2")  
        actElectrData.datasets[4].data[actPoint]  = (data[i].value * -1.0).toFixed(3);
      if (i == "power_returned_l3")  
        actElectrData.datasets[5].data[actPoint]  = (data[i].value * -1.0).toFixed(3);
      if (i == "gas_delivered") 
      {
        if (actPoint > 0)
              actGasData.datasets[0].data[actPoint] = ((data[i].value - gasDelivered) * 1000.0).toFixed(0);
        else  actGasData.datasets[0].data[actPoint] = 0.0;
        gasDelivered = data[i].value;
      }
    } // for i in data ..
    actPoint++;    
    
    if (actPoint > maxPoints) 
    {
      for (let s=0; s<6; s++)
      {
        actElectrData.labels.shift();
        actElectrData.datasets[0].data.shift();
        actElectrData.datasets[1].data.shift();
        actElectrData.datasets[2].data.shift();
        actElectrData.datasets[3].data.shift();
        actElectrData.datasets[4].data.shift();
        actElectrData.datasets[5].data.shift();
        actGasData.labels.shift();
        actGasData.datasets[0].data.shift();
        actWaterData.labels.shift();
        actWaterData.datasets[0].data.shift();
        actPoint--;
      } // for s ..
    } 
    
  } // copyActualToChart()

  
  //============================================================================  
  function initActualGraph()
  {
    //console.log("Now in initActualGraph()..");

    actElectrData     = {};     // empty actElectrData
    actElectrData.labels   = [];     // empty .labels
    actElectrData.stack    = [];     // empty .stack
    actElectrData.datasets = [];     // empty .datasets

    actGasData     = {};     // empty actElectrData
    actGasData.labels   = [];     // empty .labels
    actGasData.stack    = [];     // empty .stack
    actGasData.datasets = [];     // empty .datasets
    
    // idx 0 => EDL1
    actElectrData.datasets.push({}); //create a new dataset
    actElectrData.datasets[0].fill            = 'false';
    actElectrData.datasets[0].borderColor     = "red";
    actElectrData.datasets[0].backgroundColor = "red";
    actElectrData.datasets[0].data            = []; //contains the 'Y; axis data
    actElectrData.datasets[0].label           = "Gebruikt L1"; //"S"+s; //contains the 'Y; axis label
    actElectrData.datasets[0].stack           = "A"
    
    // idx 1 => EDL2
    actElectrData.datasets.push({}); //create a new dataset
    actElectrData.datasets[1].fill            = 'false';
    actElectrData.datasets[1].borderColor     = "tomato";
    actElectrData.datasets[1].backgroundColor = "tomato";
    actElectrData.datasets[1].data            = []; //contains the 'Y; axis data
    actElectrData.datasets[1].label           = "Gebruikt L2"; //"S"+s; //contains the 'Y; axis label
    actElectrData.datasets[1].stack           = "A"
    
    // idx 2 => EDL3
    actElectrData.datasets.push({}); //create a new dataset
    actElectrData.datasets[2].fill            = 'false';
    actElectrData.datasets[2].borderColor     = "salmon";
    actElectrData.datasets[2].backgroundColor = "salmon";
    actElectrData.datasets[2].data            = []; //contains the 'Y; axis data
    actElectrData.datasets[2].label           = "Gebruikt L3"; //"S"+s; //contains the 'Y; axis label
    actElectrData.datasets[2].stack           = "A"
    
    // idx 3 => ERL1
    actElectrData.datasets.push({}); //create a new dataset
    actElectrData.datasets[3].fill            = 'false';
    actElectrData.datasets[3].borderColor     = "yellowgreen";
    actElectrData.datasets[3].backgroundColor = "yellowgreen";
    actElectrData.datasets[3].data            = []; //contains the 'Y; axis data
    actElectrData.datasets[3].label           = "Opgewekt L1"; //"S"+s; //contains the 'Y; axis label
    actElectrData.datasets[3].stack           = "A"
    
    // idx 4 => ERL2
    actElectrData.datasets.push({}); //create a new dataset
    actElectrData.datasets[4].fill            = 'false';
    actElectrData.datasets[4].borderColor     = "springgreen";
    actElectrData.datasets[4].backgroundColor = "springgreen";
    actElectrData.datasets[4].data            = []; //contains the 'Y; axis data
    actElectrData.datasets[4].label           = "Opgewekt L2"; //"S"+s; //contains the 'Y; axis label
    actElectrData.datasets[4].stack           = "A"
    
    // idx 5 => ERL3
    actElectrData.datasets.push({}); //create a new dataset
    actElectrData.datasets[5].fill            = 'false';
    actElectrData.datasets[5].borderColor     = "green";
    actElectrData.datasets[5].backgroundColor = "green";
    actElectrData.datasets[5].data            = []; //contains the 'Y; axis data
    actElectrData.datasets[5].label           = "Opgewekt L3"; //"S"+s; //contains the 'Y; axis label
    actElectrData.datasets[5].stack           = "A"
    
    // idx 0 => GDT
    actGasData.datasets.push({}); //create a new dataset
    actGasData.datasets[0].fill            = 'false';
    actGasData.datasets[0].borderColor     = "blue";
    actGasData.datasets[0].backgroundColor = "blue";
    actGasData.datasets[0].data            = []; //contains the 'Y; axis data
    actGasData.datasets[0].label           = "Gas Verbruikt"; //"S"+s; //contains the 'Y; axis label

    actPoint = 0;
  
  } // initActualGraph()
  
  
  //============================================================================  
  function showActualGraph()
  {
    if (activeTab != "bActualTab") return;

    //console.log("Now in showActualGraph()..");

    //--- hide Table
    document.getElementById("actual").style.display    = "none";
    //--- show canvas
    document.getElementById("dataChart").style.display = "block";
    document.getElementById("gasChart").style.display  = "block";
    
    renderElectrChart(actElectrData, actElectrOptions);
    myElectrChart.update();
    
    //renderGasChart(actGasData, actGasOptions);
    renderGasChart(actGasData, "dm3");
    gasChart.update();

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
