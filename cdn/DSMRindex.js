/*
***************************************************************************  
**  Program  : DSMRindex.js, part of DSMRfirmwareAPI
**  Version  : v2.3.0
**
**  Copyright (c) 2021 Martijn Hendriks / based on DSMR Api Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/
  const APIGW='http://'+window.location.host+'/api/';

  "use strict";

  let activeTab             = "bDashTab";
  let presentationType      = "TAB";
  let tabTimer              = 0;
  let actualTimer           = 0;
  let timeTimer             = 0;
  var GitHubVersion         = 0;
  var GitHubVersion_dspl    = "-";
  var devVersion			= 0;
  var firmwareVersion       = 0;
  var firmwareVersion_dspl  = "-";
  var newVersionMsg         = "";
  let MainNav				= true;
  
  var tlgrmInterval         = 10;
  var ed_tariff1            = 0;
  var ed_tariff2            = 0;
  var er_tariff1            = 0;
  var er_tariff2            = 0;
  var gd_tariff             = 0;
  var electr_netw_costs     = 0;
  var gas_netw_costs        = 0;
  var hostName            	=  "-";  
  var data       			= [];
                  
  let monthType        = "ED";
  let settingFontColor = 'white'
                    
  var monthNames = [ "indxNul","Januari","Februari","Maart","April","Mei","Juni"
                    ,"Juli","Augustus","September","Oktober","November","December"
                    ,"\0"
                   ];
                   
  var FS_no_delete = [ "FSexplorer.html", "DSMRindex.html","DSMRsettings.json","DSMRindexEDGE.html","\0"];
  const spinner = document.getElementById("loader");

// ---- DASH
const AMPS=25;
var MaxAmps = 0.0;
var TotalAmps=0.0,minKW = 0.0, maxKW = 0.0,minV = 0.0, maxV = 0.0, Pmax,Gmax;
var hist_arrG=[4], hist_arrP=[4]; //berekening verbruik
var day = 0;

let GaugeOptions = {
		id: "gauge", // the id of the html element
		value: 0,
		min: 0,
		max: 25,
		decimals: 2,
		gaugeWidthScale: 0.6,
		pointer: true,
		pointerOptions: {
			toplength: -15,
			bottomlength: 12,
			bottomwidth: 12,
			color: '#8e8e93',
			stroke: '#ffffff',
			stroke_width: 3,
			stroke_linecap: 'round'
		}, counter: true,
		customSectors: {
			percents: true, // lo and hi values are in %
			ranges: [{
				color : "#43bf58",
				lo : 0,
				hi : 33
				},
				{
				color : "#fff130",
				lo : 34,
				hi : 75
				},
				{
				color : "#ff3b30",
				lo : 76,
				hi : 100
				}]
		}, 
		label: "Ampere" };

let GaugeOptionsV = {
		id: "gauge-v", // the id of the html element
		value: 230,
		min: 207,
		max: 253,
		decimals: 0,
		gaugeWidthScale: 0.6,
		pointer: true,
		pointerOptions: {
			toplength: -15,
			bottomlength: 12,
			bottomwidth: 12,
			color: '#8e8e93',
			stroke: '#ffffff',
			stroke_width: 3,
			stroke_linecap: 'round'
		}, counter: true,
		customSectors: {
			percents: true, // lo and hi values are in %
			ranges: [{
				color : "#ff3b30",
				lo : 0,
				hi : 10
				},
				{
				color : "#43bf58",
				lo : 11,
				hi : 90
				},
				{
				color : "#ff3b30",
				lo : 91,
				hi : 100
				}]
		}, 
		label: "Volt" };

let TrendG = {
    type: 'doughnut',
    data: {
      labels: ["gasverbruik", "verschil met hoogste"],
      datasets: [
        {
          label: "vandaag",
          backgroundColor: ["#314b77", "rgba(0,0,0,0.1)"],
          data: [0,1],
        },
        {
          label: "gisteren",
          backgroundColor: ["#316b77", "rgba(0,0,0,0.1)"],
          data: [0,1]
        },
        {
          label: "eergisteren",
          backgroundColor: ["#318b77", "rgba(0,0,0,0.1)"],
          data: [0,1]
        }
      ]
    },
    options: {
      circumference: Math.PI,
			rotation: -Math.PI,
      plugins: {
      labels: {
        render: function (args) {
          return args.value + " \u33A5";
        },//render
        arc: true,
        fontColor: ["#fff","rgba(0,0,0,0)"],
      },//labels      
    }, //plugins
    legend: {display: false},
    }, //options
};

let TrendP = {
    type: 'doughnut',
    data: {
      labels: ["gasverbruik", "verschil met hoogste"],
      datasets: [
        {
          label: "vandaag",
          backgroundColor: ["#314b77", "rgba(0,0,0,0.1)"],
          data: [0,1],
        },
        {
          label: "gisteren",
          backgroundColor: ["#316b77", "rgba(0,0,0,0.1)"],
          data: [0,1]
        },
        {
          label: "eergisteren",
          backgroundColor: ["#318b77", "rgba(0,0,0,0.1)"],
          data: [0,1]
        }
      ]
    },
    options: {
      circumference: Math.PI,
		rotation:  - Math.PI,
      plugins: {
      labels: {
        render: function (args) {
          return args.value + " kWh";
        },//render
        arc: true,
        fontColor: ["#fff","rgba(0,0,0,0)"],
      },//labels      
    }, //plugins
      legend: {display: false},
    } //options
};

function updataHist(){

}

//============================================================================  
  
function UpdateDash()
{	
	var Parr=[3], Garr=[3];
	var now = Math.floor(new Date().getTime()/8.64e7);
	console.log("Update dash");

	if (day != now) { //check first start and day change
		refreshDays();
		day = now;
	}
	showSpinner();
	fetch(APIGW+"v2/sm/fields", {"setTimeout": 2000})
	  .then(response => response.json())
	  .then(json => {
		// console.log(json);
     	// console.log(json.power_delivered.value);
// 		if (hist_arrP[0] != 4){ 
		for(let i=0;i<3;i++){
			if (i==0) {
				Parr[i]=Number(json.energy_delivered_tariff1.value + json.energy_delivered_tariff2.value - hist_arrP[i+1]).toFixed(3);
				Garr[i]=Number(json.gas_delivered.value - hist_arrG[i+1]).toFixed(3) ;
			} else {
				Parr[i]=Number(hist_arrP[i] - hist_arrP[i+1]).toFixed(3);
				Garr[i]=Number(hist_arrG[i] - hist_arrG[i+1]).toFixed(3);
			}
		}
		// maximale waarde bepalen voor de gauge
		Pmax = math.max(Parr);
		Gmax = math.max(Garr);

		// 		console.log(Pmax);
		// 		console.log(Gmax);
// 		} else {
// 			Parr = [0,0,0];
// 			Garr = [0,0,0];
// 			Pmax =0, Gmax=0;
// 		}
		//data sets berekenen voor de gauges
		for(let i=0;i<3;i++){
			trend_p.data.datasets[i].data=[Number(Parr[i]).toFixed(1),Number(Pmax-Parr[i]).toFixed(1)];
			trend_g.data.datasets[i].data=[Number(Garr[i]).toFixed(1),Number(Gmax-Garr[i]).toFixed(1)];
		};
		trend_p.update();
		trend_g.update();
	
		//check if gasmeter is available
		 if (isNaN(json.gas_delivered.value)) document.getElementById("l4").style.display = "none";
		
		 if (json.power_delivered.value > 0) 
		{	
			var fases = 1;
			let cvKW=document.getElementById("power_delivered").innerHTML;
			let nvKW= json.power_delivered.value; 
			let nvA=  json.current_l1.value;
			
			if (!isNaN(json.current_l2.value)) fases++;
			if (!isNaN(json.current_l3.value)) fases++;
			
			document.getElementById("fases").innerHTML = fases;
			
			TotalAmps = (isNaN(json.current_l1.value)?0:json.current_l1.value) + 
				(isNaN(json.current_l2.value)?0:json.current_l2.value) + 
				(isNaN(json.current_l3.value)?0:json.current_l3.value);

			let TotalU = (isNaN(json.voltage_l1.value)?0:json.voltage_l1.value) + 
				(isNaN(json.voltage_l2.value)?0:json.voltage_l2.value) + 
				(isNaN(json.voltage_l3.value)?0:json.voltage_l3.value);

			let Vgem=TotalU/fases;
			let TotalKW = json.power_delivered.value;

			gauge.refresh(TotalAmps, AMPS * fases);
			gauge_v.refresh(Vgem);

			document.getElementById("power_delivered").innerHTML = TotalKW.toLocaleString();
			document.getElementById("P").innerHTML = Number(Parr[0]).toLocaleString();
			document.getElementById("G").innerHTML = Number(Garr[0]).toLocaleString();

			//vermogen(P)
			if (minKW == 0.0 || nvKW < minKW) { minKW = nvKW;}
			if (nvKW> maxKW){ maxKW = nvKW; }
			document.getElementById(`power_delivered_1max`).innerHTML = Number(maxKW.toFixed(3)).toLocaleString();                    
			document.getElementById(`power_delivered_1min`).innerHTML = Number(minKW.toFixed(3)).toLocaleString();                        
			
			//Spanning(V)
			if (minV == 0.0 || Vgem < minV) { minV = Vgem; }
			if (Vgem > maxV) { maxV = Vgem; }
			document.getElementById(`power_delivered_2max`).innerHTML = Number(maxV.toFixed(0)).toLocaleString();
			document.getElementById(`power_delivered_2min`).innerHTML = Number(minV.toFixed(0)).toLocaleString();   
			
			//verbruik P
			document.getElementById(`Pmax`).innerHTML = Number(Pmax).toLocaleString();
			document.getElementById(`Pmin`).innerHTML = Math.min.apply(Math, Parr).toLocaleString();
			
			//verbruik G    
			document.getElementById(`Gmax`).innerHTML = Number(Gmax).toLocaleString();
			document.getElementById(`Gmin`).innerHTML = Math.min.apply(Math, Garr).toLocaleString();
												
		};//end if
		hideSpinner();
		}); //end fetch fields
}
	
// -------------- END DASH

  window.onload=bootsTrapMain;
  /*
  window.onfocus = function() {
    if (needBootsTrapMain) {
      window.location.reload(true);
    }
  };
  */
    
  //============================================================================  
  function bootsTrapMain() {
    console.log("bootsTrapMain()");
	console.log("hash:"+ location.hash);
	gauge = new JustGage(GaugeOptions); // initialize gauge
	gauge_v = new JustGage(GaugeOptionsV); // initialize gauge
	trend_g = new Chart(document.getElementById("container-4"), TrendG);
	trend_p = new Chart(document.getElementById("container-3"), TrendP);

	//handle Clickevents - main-menu
	var btns = document.getElementsByClassName("nav-item");
	for (let i = 0; i < btns.length; i++) {
  		btns[i].addEventListener("click", function() {
			activeTab = this.id;
			var current = document.getElementsByClassName("nav-item active");
			if (current.length > 0) { current[0].className = current[0].className.replace(" active", ""); }
			//console.log("ActiveID - " + activeTab );
			this.className += " active";
			MainNav = true;
			openTab();  		
  		});
	}
	//handle Clickevents - sub-menu's
	var btns = document.getElementsByClassName("subnav-item");
	for (let i = 0; i < btns.length; i++) {
  		btns[i].addEventListener("click", function() {
			activeTab = this.id;
			var current = document.getElementsByClassName("subnav-item active");
			for (let i = current.length;i > 0;i--) { current[i-1].className = current[i-1].className.replace(" active", ""); }
			//console.log("Subnav ActiveID - " + activeTab );
			this.className += " active";
			MainNav = false;	
			openTab();  	
  		});
	}
 
    refreshDevTime();
    clearInterval(timeTimer);  
    timeTimer = setInterval(refreshDevTime, 10 * 1000); // repeat every 10s

    setMonthTableType();
	getDevSettings();
    refreshDevInfo();
    openTab();
    initActualGraph();
    setPresentationType('TAB');
	//after loading ... flow the #target url just for FSExplorer
	consol// e.log("location-hash: " + location.hash );
// 	console.log("location-pathname: " + location.pathname );
// 	console.log("location-msg: " + location.hash.split('msg=')[1]);
// 	console.log("location-hash-split: " + location.hash.split('#')[1].split('?')[0]);
	//goto tab after reload 
	if (location.hash == "#FileExplorer") { document.getElementById('bFSexplorer').click(); }
	if (location.hash.split('#')[1].split('?')[0] == "Redirect") { handleRedirect(); }

	//reselect Dash when Home icon has been clicked
 	document.getElementById("Home").addEventListener("click", function() { document.getElementById('bDashTab').click(); });
  } // bootsTrapMain()
  
  //============================================================================  
  function handleRedirect(){
	console.log("location-handle: " + location.hash.split('msg=')[1]);
	//close all sections
   	var elements = document.getElementsByClassName("tabName");
    for (var i = 0; i < elements.length; i++){
        elements[i].style.display = "none";
    }
    //open only redirect section
	document.getElementById("Redirect").style.display = "block";
   	
   	if (location.hash.split('msg=')[1] == "RebootOnly"){  document.getElementById("RedirectMessage").innerHTML = "Het systeem wordt opnieuw gestart...";}
   	else if (location.hash.split('msg=')[1] == "RebootResetWifi"){  document.getElementById("RedirectMessage").innerHTML = "Het systeem wordt opnieuw gestart... Stel hierna het wifi netwerk opnieuw in. zie handleiding.";}   	
   	else if (location.hash.split('msg=')[1] == "NoUpdateServer"){  document.getElementById("RedirectMessage").innerHTML = "Het systeem wordt opnieuw gestart... Er is geen update mogelijkheid beschikbaar";} 
   	else if (location.hash.split('msg=')[1] == "Update"){  document.getElementById("RedirectMessage").innerHTML = "Het systeem wordt vernieuwd en daarna opnieuw gestart";} 

	setInterval(function() {
		var div = document.querySelector('#counter');
		var count = div.textContent * 1 - 1;
		div.textContent = count;
		if (count <= 0) {
			window.location.replace("/");
		}
	}, 1000);
  	
  }
  
  //============================================================================  

  function showSpinner() {
	document.getElementById("loader").removeAttribute('hidden');
	setTimeout(() => { document.getElementById("loader").setAttribute('hidden', '');}, 5000);
  }
  //============================================================================  

 function hideSpinner() {
  document.getElementById("loader").setAttribute('hidden', '');
 }

  //============================================================================  

  function update_nav(){
  	//hide all submenu's
	if (MainNav) { 
		document.getElementById("nav-settings").style.display = "none";
		document.getElementById("nav-info").style.display = "none";
		//reset subnav active
		var current = document.getElementsByClassName("subnav-item active");
		for (let i = current.length;i > 0;i--) { current[i-1].className = current[i-1].className.replace(" active", ""); }
	}
	//activate submenu when applicable
	if (activeTab == "bSettings" ) {
		document.getElementById("nav-settings").style.display = "flex";  
		document.getElementById("bEditSettings").className += " active"
	}
	if (activeTab == "bSysInfoTab") {
		document.getElementById("nav-info").style.display = "flex";  
		document.getElementById("bInfo_SysInfo").className += " active"
	}
  } // update_nav()

  //============================================================================  
  function openTab() {
	if (activeTab == "Home") activeTab = "bDashTab";
    console.log("openTab : " + activeTab );
  	update_nav();
    clearInterval(tabTimer);  
    clearInterval(actualTimer);  
	
	//--- hide canvas -------
    document.getElementById("dataChart").style.display = "none";
    document.getElementById("gasChart").style.display  = "none";
    
    if (activeTab != "bActualTab") {
      //clearInterval(actualTimer);
      actualTimer = setInterval(refreshSmActual, 60 * 1000);                  // repeat every 60s
    }
    if (activeTab == "bActualTab") {
      readGitHubVersion();
      refreshSmActual();
      //clearInterval(actualTimer);   
      if (tlgrmInterval < 10)
            actualTimer = setInterval(refreshSmActual, 10 * 1000);            // repeat every 10s
      else  actualTimer = setInterval(refreshSmActual, tlgrmInterval * 1000); // repeat every tlgrmInterval seconds

    } else if (activeTab == "bHoursTab") {
      refreshHours();
      clearInterval(tabTimer);
      tabTimer = setInterval(refreshHours, 58 * 1000); // repeat every 58s

    } else if (activeTab == "bDaysTab") {
      refreshDays();
      clearInterval(tabTimer);
      tabTimer = setInterval(refreshDays, 58 * 1000); // repeat every 58s

    } else if (activeTab == "bMonthsTab") {
      refreshMonths();
      clearInterval(tabTimer);
      tabTimer = setInterval(refreshMonths, 118 * 1000); // repeat every 118s
    
    } else if (activeTab == "bInfo_SysInfo") {
      refreshDevInfo();
      clearInterval(tabTimer);
      tabTimer = setInterval(refreshDevInfo, 58 * 1000); // repeat every 58s

    } else if (activeTab == "bInfo_Fields") {
      refreshSmFields();
      clearInterval(tabTimer);
      tabTimer = setInterval(refreshSmFields, 58 * 1000); // repeat every 58s

    } else if (activeTab == "bInfo_Telegram") {
      refreshSmTelegram();
      clearInterval(tabTimer); // do not repeat!

    } else if (activeTab == "bInfo_APIdoc") {
      //do nothing = static html
    } else if (activeTab == "bDashTab") {
       UpdateDash();
       clearInterval(tabTimer);
       tabTimer = setInterval(UpdateDash, 10 * 1000); // repeat every 10s
    } else if (activeTab == "bFSexplorer") {
      FSExplorer();
    } else if (activeTab == "bEditMonths") {
      document.getElementById('tabEditMonths').style.display = "block";
      document.getElementById('tabEditSettings').style.display = "none";
      clearInterval(actualTimer); //otherwise the data blok is overwritten bij actual data
      getMonths();

    } else if (activeTab == "bSettings" || activeTab == "bEditSettings") {
	  refreshDevTime();
	  //refreshDevInfo();
	  data = {};
      document.getElementById('tabEditSettings').style.display = 'block';
      document.getElementById('tabEditMonths').style.display = "none";
	  clearInterval(actualTimer); //otherwise the data blok is overwritten bij actual data
      refreshSettings();
      getDevSettings();
      activeTab = "bEditSettings";
    } 
  } // openTab()
  
    //============================================================================  

  function FSExplorer() {
	 let span = document.querySelector('span');
	 let main = document.querySelector('main');
	 let fileSize = document.querySelector('fileSize');
	showSpinner();

	 fetch('api/listfiles').then(function (response) {
		 return response.json();
	 }).then(function (json) {
	
	//clear previous content	 
	 var list = document.getElementById("FSmain");
	 while (list.hasChildNodes()) {  
	   list.removeChild(list.firstChild);
	 }

	   let dir = '<table id="FSTable" width=90%>';
	   for (var i = 0; i < json.length - 1; i++) {
		 dir += "<tr>";
		 dir += `<td width=250px nowrap><a href ="${json[i].name}" target="_blank">${json[i].name}</a></td>`;
		 dir += `<td width=100px nowrap><small>${json[i].size}</small></td>`;
		 dir += `<td width=100px nowrap><a href ="${json[i].name}"download="${json[i].name}"> Download </a></td>`;
		 if (  FS_no_delete.indexOf(json[i].name) < 0 ) {
		   dir += `<td width=100px nowrap><a href ="${json[i].name}?delete=/${json[i].name}"> Delete </a></td>`;
		   if (json[i].name == '!format') 
		   {
			 document.getElementById('FormatSPIFFS').disabled = false;
		   }
		 } else {
		   dir += `<td width=100px nowrap> </td>`;
		 }                 	 
		 dir += "</tr>";
	   }	// for ..
	   main.insertAdjacentHTML('beforeend', dir);
	   document.querySelectorAll('[href*=delete]').forEach((node) => {
			 node.addEventListener('click', () => {
					 if (!confirm('Weet je zeker dat je dit bestand wilt verwijderen?!')) event.preventDefault();  
			 });
	   });
	   main.insertAdjacentHTML('beforeend', '</table>');
	   main.insertAdjacentHTML('beforeend', `<p id="FSFree"><b>SPIFFS</b> gebruikt ${json[i].usedBytes} van ${json[i].totalBytes}`);
	   free = json[i].freeBytes;
	   fileSize.innerHTML = "<b> &nbsp; </b><p>";    // spacer                
	   hideSpinner();
	 });	// function(json)
	 
	 document.getElementById('Ifile').addEventListener('change', () => {
		 let nBytes = document.getElementById('Ifile').files[0].size, output = `${nBytes} Byte`;
		 for (var aMultiples = [
			 ' KB',
			 ' MB'
			], i = 0, nApprox = nBytes / 1024; nApprox > 1; nApprox /= 1024, i++) {
			  output = nApprox.toFixed(2) + aMultiples[i];
			}
			if (nBytes > free) {
			  fileSize.innerHTML = `<p><small> Bestand Grootte: ${output}</small><strong style="color: red;"> niet genoeg ruimte! </strong><p>`;
			  document.getElementById('Iupload').setAttribute('disabled', 'disabled');
			} 
			else {
			  fileSize.innerHTML = `<b>Bestand grootte:</b> ${output}<p>`;
			  document.getElementById('Iupload').removeAttribute('disabled');
			}
	 });	
  }
  
  //============================================================================  
  function refreshDevInfo()
  { showSpinner();
    fetch(APIGW+"v2/dev/info")
      .then(response => response.json())
      .then(json => {
        console.log("parsed .., data is ["+ JSON.stringify(json)+"]");
		hideSpinner();
        obj = json;
        var tableRef = document.getElementById('tb_info');
        //clear table
		while (tableRef.hasChildNodes()) { tableRef.removeChild(tableRef.lastChild);}
		//fill table
        for( let k in obj ) {
			var newRow=tableRef.insertRow(-1);
			var MyCell1 = newRow.insertCell(0);
			var MyCell2 = newRow.insertCell(1);
			var MyCell3 = newRow.insertCell(2);
			MyCell1.innerHTML=translateToHuman(k);
            if(obj[k] instanceof Object) {
				MyCell2.innerHTML=obj[k].value;
				MyCell3.innerHTML=obj[k].unit;
				MyCell2.style.textAlign = "right";
             
            } else { 
            	MyCell2.innerHTML=obj[k]; 
            };
           if (k == "fwversion"){
			   console.log("fwversion: " + obj[k] );
			   devVersion = obj[k];
           }
           
        } //for loop
      
	  //new fwversion detection
  	  document.getElementById('devVersion').innerHTML = obj.fwversion;
	  var tmpFW = devVersion;
	  firmwareVersion_dspl = tmpFW;
	  tmpX = tmpFW.substring(1, tmpFW.indexOf(' '));
	  tmpN = tmpX.split(".");
	  firmwareVersion = tmpN[0]*10000+tmpN[1]*100+tmpN[2]*1;
	  console.log("firmwareVersion["+firmwareVersion+"] >= GitHubVersion["+GitHubVersion+"]");
	  if (GitHubVersion == 0 || firmwareVersion >= GitHubVersion)
			newVersionMsg = "";
	  else  newVersionMsg = firmwareVersion_dspl + " nieuwere versie ("+GitHubVersion_dspl+") beschikbaar";
	  document.getElementById('message').innerHTML = newVersionMsg;
	  console.log(newVersionMsg);

	  tlgrmInterval = obj.telegraminterval;
      if (firmwareVersion > 20000) document.getElementById("resetWifi").removeAttribute('hidden');
      if (firmwareVersion > 20102) document.getElementById("update").removeAttribute('hidden');

      })
      .catch(function(error) {
        var p = document.createElement('p');
        p.appendChild(
          document.createTextNode('Error: ' + error.message)
        );
      });
  } // refreshDevInfo()



  //============================================================================  
  function refreshDevTime()
  {
    //console.log("Refresh api/v2/dev/time ..");
    fetch(APIGW+"v2/dev/time")
      .then(response => response.json())
      .then(json => {
              document.getElementById('theTime').innerHTML = json.time;
              //console.log("parsed .., data is ["+ JSON.stringify(json)+"]");

	  //after reboot checks of the server is up and running and redirects to home
      if ((document.querySelector('#counter').textContent < 40) && (document.querySelector('#counter').textContent > 0)) window.location.replace("/");
      })
      .catch(function(error) {
        var p = document.createElement('p');
        p.appendChild(
          document.createTextNode('Error: ' + error.message)
        );
      });     
      
    document.getElementById('message').innerHTML = newVersionMsg;

  } // refreshDevTime()
  
  
  //============================================================================  
  function refreshSmActual()
  { showSpinner();
    fetch(APIGW+"v2/sm/actual", {"setTimeout": 2000})
      .then(response => response.json())
      .then(json => {
          console.log("actual parsed .., fields is ["+ JSON.stringify(json)+"]");
          data = json;
          copyActualToChart(data);
          if (presentationType == "TAB")
                showActualTable(data);
          else  showActualGraph(data);
        //console.log("-->done..");
         hideSpinner();
      })
      .catch(function(error) {
        var p = document.createElement('p');
        p.appendChild(
          document.createTextNode('Error: ' + error.message)
        );
      }); 
  };  // refreshSmActual()
  
  
  //============================================================================  
  function refreshSmFields()
  { showSpinner();
    fetch(APIGW+"v2/sm/fields")
      .then(response => response.json())
      .then(json => {
          console.log("parsed .., fields is ["+ JSON.stringify(json)+"]");
          data = json;
          for (var item in data) 
          {
          	console.log("fields item: " +item);
          	console.log("fields data[item].value: " +data[item].value);
            data[item].humanName = translateToHuman(item);
            var tableRef = document.getElementById('fieldsTable').getElementsByTagName('tbody')[0];
            if( ( document.getElementById("fieldsTable_"+item)) == null )
            {
              var newRow   = tableRef.insertRow();
              newRow.setAttribute("id", "fieldsTable_"+item, 0);
              // Insert a cell in the row at index 0
              var newCell  = newRow.insertCell(0);                  // name
              var newText  = document.createTextNode('');
              newCell.appendChild(newText);
              newCell  = newRow.insertCell(1);                      // humanName
              newCell.appendChild(newText);
              newCell  = newRow.insertCell(2);                      // value
              newCell.appendChild(newText);
              newCell  = newRow.insertCell(3);                      // unit
              newCell.appendChild(newText);
            }
            tableCells = document.getElementById("fieldsTable_"+item).cells;
            tableCells[0].innerHTML = item;
            tableCells[1].innerHTML = data[item].humanName;
            if (item == "electricity_failure_log" && data[item].value.length > 50) 
            {
              tableCells[2].innerHTML = data[item].value.substring(0,50);
              var lLine = data[item].value.substring(50);
              while (lLine.length > 50)
              {
                tableCells[2].innerHTML += "<br>" + lLine.substring(0,50);
                lLine = lLine.substring(50);
              }
              tableCells[2].innerHTML += "<br>" + lLine;
              tableCells[0].setAttribute("style", "vertical-align: top");
              tableCells[1].setAttribute("style", "vertical-align: top");
            }
            else
            {
              tableCells[2].innerHTML = data[item].value;
            }
            if (data[item].hasOwnProperty('unit'))
            {
              tableCells[2].style.textAlign = "right";              // value
              tableCells[3].style.textAlign = "center";             // unit
              tableCells[3].innerHTML = data[item].unit;
            }
          }
          //console.log("-->done..");
        hideSpinner();

      })
      .catch(function(error) {
        var p = document.createElement('p');
        p.appendChild(
          document.createTextNode('Error: ' + error.message)
        );
      }); 
  };  // refreshSmFields()
  
  //============================================================================  
  function expandData(data)
  {
	//console.log("expandData2: data length:"+ data.data.length );
	//console.log("expandData2: actslot:"+ data.actSlot );
    var i;
    var slotbefore;
    	
    //--- first check op volgordelijkheid ------    
    if (activeTab == "bHoursTab") {  
    
    }
    for (let x=data.data.length + data.actSlot; x > data.actSlot; x--)
    {	i = x % data.data.length;
        slotbefore = math.mod(i-1, data.data.length);
		//console.log("ExpandData2 - x: "+x); 
		//console.log("ExpandData2 - i: "+i);
		//console.log("ExpandData2 - slotbefore: "+slotbefore); 
		//console.log("gd_tariff: "+gd_tariff); 
		//console.log("ed_tariff: "+ed_tariff1); 
      var     costs     = 0;
      if (x != data.actSlot 	)
      { 
        data.data[i].p_ed  = ((data.data[i].values[0] + data.data[i].values[1])-(data.data[slotbefore].values[0] +data.data[slotbefore].values[1])).toFixed(3);
        data.data[i].p_edw = (data.data[i].p_ed * 1000).toFixed(0);
        data.data[i].p_er  = ((data.data[i].values[2] + data.data[i].values[3])-(data.data[slotbefore].values[2] +data.data[slotbefore].values[3])).toFixed(3);
        data.data[i].p_erw = (data.data[i].p_er * 1000).toFixed(0);
        data.data[i].p_gd  = (data.data[i].values[4]  - data.data[slotbefore].values[4]).toFixed(3);
        //-- calculate Energy Delivered costs
        costs = ( (data.data[i].values[0] - data.data[slotbefore].values[0]) * ed_tariff1 );
        costs = costs + ( (data.data[i].values[1] - data.data[slotbefore].values[1]) * ed_tariff2 );
        //-- subtract Energy Returned costs
        costs = costs - ( (data.data[i].values[2] - data.data[slotbefore].values[2]) * er_tariff1 );
        costs = costs - ( (data.data[i].values[3] - data.data[slotbefore].values[3]) * er_tariff2 );
        data.data[i].costs_e = costs;
        //-- add Gas Delivered costs
        data.data[i].costs_g = ( (data.data[i].values[4]  - data.data[slotbefore].values[4])  * gd_tariff );
        
        //-- compute network costs
        data.data[i].costs_nw = (electr_netw_costs + gas_netw_costs);
        //-- compute total costs
        data.data[i].costs_tt = ( (data.data[i].costs_e + data.data[i].costs_g + data.data[i].costs_nw) * 1.0);
      }
      else
      {
        costs             = 0;
        data.data[i].p_ed      = (data.data[i].values[0] +data.data[i].values[1]).toFixed(3);
        data.data[i].p_edw     = (data.data[i].p_ed * 1000).toFixed(0);
        data.data[i].p_er      = (data.data[i].values[2] +data.data[i].values[3]).toFixed(3);
        data.data[i].p_erw     = (data.data[i].p_er * 1000).toFixed(0);
        data.data[i].p_gd      = (data.data[i].values[4]).toFixed(3);
        data.data[i].costs_e   = 0.0;
        data.data[i].costs_g   = 0.0;
        data.data[i].costs_nw  = 0.0;
        data.data[i].costs_tt  = 0.0;
      }
    } // for i ..
    //console.log("leaving expandData() ..");
	//console.log("expandData2: eind data "+ JSON.stringify(data));
  } // expandData()
  
  //============================================================================  
  function refreshHours()
  { showSpinner();
    console.log("fetch("+APIGW+"v2/hist/hours)");

    fetch(APIGW+"v2/hist/hours", {"setTimeout": 2000})
      .then(response => response.json())
      .then(json => {
        //console.log(json);
        data = json;
        expandData(data);
        if (presentationType == "TAB")
              showHistTable(data, "Hours");
        else  showHistGraph(data, "Hours");
	 hideSpinner();

      })
      .catch(function(error) {
        var p = document.createElement('p');
        p.appendChild(
          document.createTextNode('Error: ' + error.message)
        );
      }); 
  } // resfreshHours()
  
  
  //============================================================================  
  function refreshDays()
  {
  	showSpinner();
    console.log("fetch("+APIGW+"v2/hist/days)");
    fetch(APIGW+"v2/hist/days", {"setTimeout": 2000})
      .then(response => response.json())
      .then(json => {
        data = json;
        expandData(data);
        if (presentationType == "TAB")
              showHistTable(data, "Days");
        else  showHistGraph(data, "Days");
		//voor dashboard
        var act_slot = data.actSlot;
		for (let i=0;i<4;i++)
		{	
			hist_arrG[i] =json.data[math.mod(act_slot-i,15)].values[4];
			hist_arrP[i] = json.data[math.mod(act_slot-i,15)].values[0] 
				+ json.data[math.mod(act_slot-i,15)].values[1];
		};
		// 		console.log(hist_arrG);
		// 		console.log(hist_arrP);

	    hideSpinner();

      })
      .catch(function(error) {
        var p = document.createElement('p');
        p.appendChild(
          document.createTextNode('Error: ' + error.message)
        );
      });
  } // resfreshDays()
  
  
  //============================================================================  
  function refreshMonths()
  {
  	showSpinner();
    console.log("fetch("+APIGW+"v2/hist/months)");
    fetch(APIGW+"v2/hist/months", {"setTimeout": 2000})
      .then(response => response.json())
      .then(json => {
        //console.log(response);
        data = json;
        expandData(data);
        if (presentationType == "TAB")
        {
          if (document.getElementById('mCOST').checked)
                showMonthsCosts(data);
          else  showMonthsHist(data);
        }
        else  showMonthsGraph(data);
        hideSpinner();
      })
      .catch(function(error) {
        var p = document.createElement('p');
        p.appendChild(
          document.createTextNode('Error: ' + error.message)
        );
      });
  } // resfreshMonths()

    
  //============================================================================  
  function refreshSmTelegram()
  { showSpinner();
    fetch(APIGW+"v2/sm/telegram")
      .then(response => response.text())
      .then(response => {
        //console.log("parsed .., data is ["+ response+"]");
    	//console.log('-------------------');
        var divT = document.getElementById('rawTelegram');
        if ( document.getElementById("TelData") == null )
        {
            console.log("CreateElement(pre)..");
            var preT = document.createElement('pre');
            preT.setAttribute("id", "TelData", 0);
            preT.setAttribute('class', 'telegram');
            preT.textContent = response;
            divT.appendChild(preT);
        }
        preT = document.getElementById("TelData");
        preT.textContent = response;
        hideSpinner();
      })
      .catch(function(error) {
        var p = document.createElement('p');
        p.appendChild(
          document.createTextNode('Error: ' + error.message)
        );
      });     
  } // refreshSmTelegram()

  //============================================================================  
  function showActualTable(data)
  { 
    if (activeTab != "bActualTab") return;

    console.log("showActual()");

    for (var item in data) 
    {
       	//console.log("showActualTableV2 i: "+item);
    	//console.log("showActualTableV2 data[i]: "+data[item]);
    	//console.log("showActualTableV2 data[i].value: "+data[item].value);
    	
      data[item].humanName = translateToHuman(item);
      var tableRef = document.getElementById('actualTable').getElementsByTagName('tbody')[0];
      if( ( document.getElementById("actualTable_"+item)) == null )
      {
        var newRow   = tableRef.insertRow();
        newRow.setAttribute("id", "actualTable_"+item, 0);
        // Insert a cell in the row at index 0
        var newCell  = newRow.insertCell(0);            // (short)name
        var newText  = document.createTextNode('');
        newCell.appendChild(newText);
        newCell  = newRow.insertCell(1);                // value
        newCell.appendChild(newText);
        newCell  = newRow.insertCell(2);                // unit
        newCell.appendChild(newText);
      }
      tableCells = document.getElementById("actualTable_"+item).cells;
      tableCells[0].innerHTML = data[item].humanName;
      tableCells[1].innerHTML = data[item].value;
      if (data[item].hasOwnProperty('unit')) tableCells[2].innerHTML = data[item].unit;
      
    }

    //--- hide canvas
    document.getElementById("dataChart").style.display = "none";
    document.getElementById("gasChart").style.display  = "none";
    //--- show table
    document.getElementById("actual").style.display    = "block";

  } // showActualTable()
    
      
  //============================================================================  
  function showHistTable(data, type)
  { 
    console.log("showHistTable("+type+")");
    // the last element has the metervalue, so skip it
    var stop = data.actSlot + 1;
    var start = data.data.length + data.actSlot ;
    var index;
    	//console.log("showHistTable start: "+start);
    	//console.log("showHistTable stop: "+stop);
    
    for (let i=start; i>stop; i--)
    {  index = i % data.data.length;
		//console.log("showHistTable index: "+index);
		//console.log("showHistTable("+type+"): data["+i+"] => data["+i+"]name["+data[i].recid+"]");

      var tableRef = document.getElementById('last'+type+'Table').getElementsByTagName('tbody')[0];
      if( ( document.getElementById(type +"Table_"+type+"_R"+index)) == null )
      {
        var newRow   = tableRef.insertRow();
        //newRow.setAttribute("id", type+"Table_"+data[i].recid, 0);
        newRow.setAttribute("id", type+"Table_"+type+"_R"+index, 0);
        // Insert a cell in the row at index 0
        var newCell  = newRow.insertCell(0);
        var newText  = document.createTextNode('-');
        newCell.appendChild(newText);
        newCell  = newRow.insertCell(1);
        newCell.appendChild(newText);
        newCell  = newRow.insertCell(2);
        newCell.appendChild(newText);
        newCell  = newRow.insertCell(3);
        newCell.appendChild(newText);
        if (type == "Days")
        {
          newCell  = newRow.insertCell(4);
          newCell.appendChild(newText);
        }
      }
      	
      tableCells = document.getElementById(type+"Table_"+type+"_R"+index).cells;
      tableCells[0].innerHTML = formatDate(type, data.data[index].date);
      if (data.data[index].p_edw >= 0) tableCells[1].innerHTML = data.data[index].p_edw;
      else tableCells[1].innerHTML = "-";

      if (data.data[index].p_erw >= 0) tableCells[2].innerHTML = data.data[index].p_erw;
      else tableCells[2].innerHTML = "-";

      if (data.data[index].p_gd >= 0) tableCells[3].innerHTML = data.data[index].p_gd;

      if (type == "Days")
      {
        tableCells[4].innerHTML = ( (data.data[index].costs_e + data.data[index].costs_g) * 1.0).toFixed(2);
      }
    };

    //--- hide canvas
    document.getElementById("dataChart").style.display = "none";
    document.getElementById("gasChart").style.display  = "none";
    //--- show table
    document.getElementById("lastHours").style.display = "block";
    document.getElementById("lastDays").style.display  = "block";

  } // showHistTable()

//============================================================================  
  function showMonthsHist(data)
  { 
    //console.log("now in showMonthsHist() ..");
    var start = data.data.length + data.actSlot ; //  maar 1 jaar ivm berekening jaar verschil
    var stop = start - 12;
    var i;
    var slotyearbefore = 0;
  
    for (let index=start; index>stop; index--)
    {  i = index % data.data.length;
      	slotyearbefore = math.mod(i-12,24);
      var tableRef = document.getElementById('lastMonthsTable').getElementsByTagName('tbody')[0];
      if( ( document.getElementById("lastMonthsTable_R"+i)) == null )
      {
        var newRow   = tableRef.insertRow();
        newRow.setAttribute("id", "lastMonthsTable_R"+i, 0);
        // Insert a cell in the row at index 0
        var newCell  = newRow.insertCell(0);          // maand
        var newText  = document.createTextNode('-');
        newCell.appendChild(newText);
        newCell  = newRow.insertCell(1);              // jaar
        newCell.appendChild(newText);
        newCell  = newRow.insertCell(2);              // verbruik
        newCell.appendChild(newText);
        newCell  = newRow.insertCell(3);              // jaar
        newCell.appendChild(newText);
        newCell  = newRow.insertCell(4);              // verbruik
        newCell.appendChild(newText);

        newCell  = newRow.insertCell(5);              // jaar
        newCell.appendChild(newText);
        newCell  = newRow.insertCell(6);              // opgewekt
        newCell.appendChild(newText);
        newCell  = newRow.insertCell(7);              // jaar
        newCell.appendChild(newText);
        newCell  = newRow.insertCell(8);             // opgewekt
        newCell.appendChild(newText);
        
        newCell  = newRow.insertCell(9);             // jaar
        newCell.appendChild(newText);
        newCell  = newRow.insertCell(10);             // gas
        newCell.appendChild(newText);
        newCell  = newRow.insertCell(11);             // jaar
        newCell.appendChild(newText);
        newCell  = newRow.insertCell(12);             // gas
        newCell.appendChild(newText);
      }
      var mmNr = parseInt(data.data[i].date.substring(2,4), 10);

      tableCells = document.getElementById("lastMonthsTable_R"+i).cells;
      //fill with default values
      for (y=2; y <= 12; y=y+2){ tableCells[y].innerHTML = "-"; }
      
      tableCells[0].innerHTML = monthNames[mmNr];                           // maand
      
      tableCells[1].innerHTML = "20"+data.data[i].date.substring(0,2);          // jaar
      if (data.data[i].p_ed >= 0) tableCells[2].innerHTML = data.data[i].p_ed;                         // verbruik
      
      tableCells[3].innerHTML = "20"+data.data[slotyearbefore].date.substring(0,2);       // jaar
      if (data.data[slotyearbefore].p_ed >= 0) tableCells[4].innerHTML = data.data[slotyearbefore].p_ed;                      // verbruik
      
      tableCells[5].innerHTML = "20"+data.data[i].date.substring(0,2);          // jaar
      if (data.data[i].p_er >= 0) tableCells[6].innerHTML = data.data[i].p_er;                         // opgewekt
      
      tableCells[7].innerHTML = "20"+data.data[slotyearbefore].date.substring(0,2);       // jaar
      if (data.data[slotyearbefore].p_er >= 0) tableCells[8].innerHTML = data.data[slotyearbefore].p_er;                      // opgewekt
      
      tableCells[9].innerHTML = "20"+data.data[i].date.substring(0,2);          // jaar
      if (data.data[i].p_gd >= 0) tableCells[10].innerHTML = data.data[i].p_gd;                        // gas
      
      tableCells[11].innerHTML = "20"+data.data[slotyearbefore].date.substring(0,2);      // jaar
      if (data.data[slotyearbefore].p_gd >= 0) tableCells[12].innerHTML = data.data[slotyearbefore].p_gd;                     // gas
      
    };
    
    //--- hide canvas
    document.getElementById("dataChart").style.display  = "none";
    document.getElementById("gasChart").style.display   = "none";
    //--- show table
    document.getElementById("lastMonths").style.display = "block";

  } // showMonthsHist()
  
  //============================================================================  
  function showMonthsCosts(data)
  { 
    console.log("now in showMonthsCosts() ..");
    var totalCost   = 0;
    var totalCost_1 = 0;
    var start = data.data.length + data.actSlot ; //  maar 1 jaar ivm berekening jaar verschil
    var stop = start - 12;
    var i;
    var slotyearbefore = 0;
  
    for (let index=start; index>stop; index--)
    {  i = index % data.data.length;
      	slotyearbefore = math.mod(i-12,24);
      //console.log("showMonthsHist(): data["+i+"] => data["+i+"].name["+data[i].recid+"]");
      var tableRef = document.getElementById('lastMonthsTableCosts').getElementsByTagName('tbody')[0];
      if( ( document.getElementById("lastMonthsTableCosts_R"+i)) == null )
      {
        var newRow   = tableRef.insertRow();
        newRow.setAttribute("id", "lastMonthsTableCosts_R"+i, 0);
        // Insert a cell in the row at index 0
        var newCell  = newRow.insertCell(0);          // maand
        var newText  = document.createTextNode('-');
        newCell.appendChild(newText);
        newCell  = newRow.insertCell(1);              // jaar
        newCell.appendChild(newText);
        newCell  = newRow.insertCell(2);              // kosten electra
        newCell.appendChild(newText);
        newCell  = newRow.insertCell(3);              // kosten gas
        newCell.appendChild(newText);
        newCell  = newRow.insertCell(4);              // vast recht
        newCell.appendChild(newText);
        newCell  = newRow.insertCell(5);              // kosten totaal
        newCell.appendChild(newText);

        newCell  = newRow.insertCell(6);              // jaar
        newCell.appendChild(newText);
        newCell  = newRow.insertCell(7);              // kosten electra
        newCell.appendChild(newText);
        newCell  = newRow.insertCell(8);              // kosten gas
        newCell.appendChild(newText);
        newCell  = newRow.insertCell(9);              // vast recht
        newCell.appendChild(newText);
        newCell  = newRow.insertCell(10);              // kosten totaal
        newCell.appendChild(newText);
      }
      var mmNr = parseInt(data.data[i].date.substring(2,4), 10);

      tableCells = document.getElementById("lastMonthsTableCosts_R"+i).cells;
      tableCells[0].style.textAlign = "right";
      tableCells[0].innerHTML = monthNames[mmNr];                           // maand
      
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
      var newRow   = tableRef.insertRow();                                // voorschot regel
      newRow.setAttribute("id", "periodicCosts", 0);
      // Insert a cell in the row at index 0
      var newCell  = newRow.insertCell(0);                                // maand
      var newText  = document.createTextNode('-');
      newCell.appendChild(newText);
      newCell  = newRow.insertCell(1);              // description
      newCell.setAttribute("colSpan", "4");
      newCell.appendChild(newText);
      newCell  = newRow.insertCell(2);              // voorschot
      newCell.appendChild(newText);
      newCell  = newRow.insertCell(3);              // description
      newCell.setAttribute("colSpan", "4");
      newCell.appendChild(newText);
      newCell  = newRow.insertCell(4);              // voorschot
      newCell.appendChild(newText);
    }
    tableCells = document.getElementById("periodicCosts").cells;
    tableCells[1].style.textAlign = "right";
    tableCells[1].innerHTML = "Voorschot Bedrag"
    tableCells[2].style.textAlign = "right";
    tableCells[2].innerHTML = "€ " + (totalCost / 12).toFixed(2);
    tableCells[3].style.textAlign = "right";
    tableCells[3].innerHTML = "Voorschot Bedrag"
    tableCells[4].style.textAlign = "right";
    tableCells[4].innerHTML = "€ " + (totalCost_1 / 12).toFixed(2);

    
    //--- hide canvas
    document.getElementById("dataChart").style.display  = "none";
    document.getElementById("gasChart").style.display   = "none";
    //--- show table
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
  function getDevSettings()
  { showSpinner();
    fetch(APIGW+"v2/dev/settings")
      .then(response => response.json())
      .then(json => {
        console.log("getDevSettings: parsed .., data is ["+ JSON.stringify(json)+"]");
        ed_tariff1 = json.ed_tariff1.value;
        ed_tariff2 = json.ed_tariff2.value;
        er_tariff1 = json.er_tariff1.value;
        er_tariff2 = json.er_tariff2.value;
        gd_tariff = json.gd_tariff.value;

        electr_netw_costs = json.electr_netw_costs.value;
        gas_netw_costs = json.gas_netw_costs.value;
        hostName = json.hostName.value;
        hideSpinner();
      })
      .catch(function(error) {
        var p = document.createElement('p');
        p.appendChild(
          document.createTextNode('Error: ' + error.message)
        );
      });   
  } // getDevSettings()
  
    
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
    if (activeTab == "bHoursTab")   refreshHours();
    if (activeTab == "bDaysTab")    refreshDays();
    if (activeTab == "bMonthsTab")  refreshMonths();

  } // setPresenationType()
  
    
  //============================================================================  
  function setMonthTableType() {
    console.log("Set Month Table Type");
    if (presentationType == 'GRAPH') 
    {
      document.getElementById('mCOST').checked = false;
      return;
    }
    if (document.getElementById('mCOST').checked)
    {
      document.getElementById("lastMonthsTableCosts").style.display = "block";
      document.getElementById("lastMonthsTable").style.display      = "none";
    }
    else
    {
      document.getElementById("lastMonthsTable").style.display      = "block";
      document.getElementById("lastMonthsTableCosts").style.display = "none";
    }
    document.getElementById('lastMonthsTableCosts').getElementsByTagName('tbody').innerHTML = "";
    refreshMonths();
      
  } // setMonthTableType()
    
  //============================================================================  
  function refreshSettings()
  {
    console.log("refreshSettings() ..");
    showSpinner();
    data = {};
    fetch(APIGW+"v2/dev/settings")
      .then(response => response.json())
      .then(json => {
        console.log("then(json => ..)");
        data = json;
        for( let i in data )
        {
          console.log("["+i+"]=>["+data[i].value+"]");
          var settings = document.getElementById('settings_table');
          if( ( document.getElementById("settingR_"+i)) == null )
          {
            var rowDiv = document.createElement("div");
            rowDiv.setAttribute("class", "settingDiv");
            rowDiv.setAttribute("id", "settingR_"+i);
            //--- field Name ---
              var fldDiv = document.createElement("div");
                  fldDiv.textContent = translateToHuman(i);
                  rowDiv.appendChild(fldDiv);
            //--- input ---
              var inputDiv = document.createElement("div");
                  inputDiv.setAttribute("class", "settings-right");

                    var sInput = document.createElement("INPUT");
                    sInput.setAttribute("id", "setFld_"+i);

					switch(data[i].type){
					case "s":
						sInput.setAttribute("type", "text");
						sInput.setAttribute("maxlength", data[i].maxlen);
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
                    sInput.addEventListener('change',
                                function() { setBackGround("setFld_"+i, "lightgray"); },
                                            false
                                );
                  inputDiv.appendChild(sInput);
                  
            rowDiv.appendChild(inputDiv);
            settings.appendChild(rowDiv);
          }
          else
          {
            document.getElementById("setFld_"+i).style.background = "white";
            document.getElementById("setFld_"+i).value = data[i].value;
          }
        }
        //console.log("-->done..");
         hideSpinner();

      })
      .catch(function(error) {
        var p = document.createElement('p');
        p.appendChild(
          document.createTextNode('Error: ' + error.message)
        );
      });     

      document.getElementById('message').innerHTML = newVersionMsg;
  } // refreshSettings()
  
  
  //============================================================================  
  function getMonths()
  {	showSpinner();
    console.log("fetch("+APIGW+"v2/hist/months)");
    fetch(APIGW+"v2/hist/months", {"setTimeout": 2000})
      .then(response => response.json())
      .then(json => {
        //console.log(response);
        data = json;
        expandDataSettings(data);
        showMonths(data, monthType);
        hideSpinner();
      })
      .catch(function(error) {
        var p = document.createElement('p');
        p.appendChild(
          document.createTextNode('Error: ' + error.message)
        );
      });

      document.getElementById('message').innerHTML = newVersionMsg;
  } // getMonths()

  
  //============================================================================  
  function showMonths(data, type)
  { 
    console.log("showMonths("+type+")");
    //--- first remove all Children ----
    var allChildren = document.getElementById('editMonths');
    while (allChildren.firstChild) {
      allChildren.removeChild(allChildren.firstChild);
    }
    var dlength = data.data.length;
    console.log("Now fill the DOM!");    
    console.log("data.data.length: " + dlength);
  
    for (let index=data.actSlot + dlength; index>data.actSlot; index--)
    {  let i = index % dlength;

      //console.log("["+i+"] >>>["+data.data[i].EEYY+"-"+data.data[i].MM+"]");
      
      var em = document.getElementById('editMonths');

      if( ( document.getElementById("em_R"+i)) == null )
      {
        var div1 = document.createElement("div");
            div1.setAttribute("class", "settingDiv");
            div1.setAttribute("id", "em_R"+i);
            if (i == (data.data.length -1))  // last row
            {
            }
            var span2 = document.createElement("span");
              //--- create input for EEYY
              var sInput = document.createElement("INPUT");
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
              var sInput = document.createElement("INPUT");
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
                var sInput = document.createElement("INPUT");
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
                var sInput = document.createElement("INPUT");
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

    var changed = false;
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

  } // showMonths()

  
  //============================================================================  
  function expandDataSettings(data)
  { 
    for (let i=0; i<data.data.length; i++)
    {
      data.data[i].EEYY = {};
      data.data[i].MM   = {};
      data.data[i].EEYY = parseInt("20"+data.data[i].date.substring(0,2));
      data.data[i].MM   = parseInt(data.data[i].date.substring(2,4));
    }
      console.log("expandDataSettings(): "+JSON.stringify(data));


  } // expandDataSettings()
  
      
  //============================================================================  
  function undoReload()
  {
    if (activeTab == "bEditMonths") {
      console.log("getMonths");
      getMonths();
    } else if (activeTab == "bEditSettings") {
      console.log("undoReload(): reload Settings..");
      data = {};
      refreshSettings();

    } else {
      console.log("undoReload(): I don't knwo what to do ..");
    }

  } // undoReload()
  
  
  //============================================================================  
  function saveData() 
  {
    document.getElementById('message').innerHTML = "Gegevens worden opgeslagen ..";

    if (activeTab == "bEditSettings")
    {
      saveSettings();
    } 
    else if (activeTab == "bEditMonths")
    {
      saveMeterReadings();
    }
    
  } // saveData()
  
  
  //============================================================================  
  function saveSettings() 
  {
    for(var i in data)
    {
      var fldId  = i;
      var newVal = document.getElementById("setFld_"+fldId).value;
      if (data[i].value != newVal)
      {
        console.log("save data ["+i+"] => from["+data[i].value+"] to["+newVal+"]");
        sendPostSetting(fldId, newVal);
      }
    }    
    // delay refresh as all fetch functions are asynchroon!!
    setTimeout(function() {
      refreshSettings();
    }, 1000);
    
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
    const jsonString = {"name" : field, "value" : value};
    console.log("send JSON:["+JSON.stringify(jsonString)+"]");
    const other_params = {
        headers : { "content-type" : "application/json; charset=UTF-8"},
        body : JSON.stringify(jsonString),
        method : "POST",
        //aangepast cors -> no-cors
        mode : "no-cors"
    };
	showSpinner();
    fetch(APIGW+"v2/dev/settings", other_params)
      .then(function(response) {
            //console.log(response.status );    //=> number 100–599
            //console.log(response.statusText); //=> String
            //console.log(response.headers);    //=> Headers
            //console.log(response.url);        //=> String
            //console.log(response.text());
            //return response.text()
            hideSpinner();
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
    showSpinner();
    fetch(APIGW+"v2/hist/months", other_params)
      .then(function(response) {
      hideSpinner();
      }, function(error) {
        console.log("Error["+error.message+"]"); //=> String
      });
      
  } // sendPostReading()

  
  //============================================================================  
  function readGitHubVersion()
  {
    if (GitHubVersion != 0) return;
    fetch("https://cdn.jsdelivr.net/gh/mhendriks/DSMR-API-V2@master/edge/DSMRversion.dat")
      .then(response => {
        if (response.ok) {
          return response.text();
        } else {
          console.log('Something went wrong');
          return "";
        }
      })
      .then(text => {
        var tmpGHF     = text.replace(/(\r\n|\n|\r)/gm, "");
        GitHubVersion_dspl = tmpGHF;
        console.log("parsed: GitHubVersion is ["+GitHubVersion_dspl+"]");
        tmpX = tmpGHF.substring(1, tmpGHF.indexOf(' '));
        tmpN = tmpX.split(".");
        GitHubVersion = tmpN[0]*10000 + tmpN[1]*100 + tmpN[2]*1;
        
        console.log("firmwareVersion["+firmwareVersion+"] >= GitHubVersion["+GitHubVersion+"]");
        if (firmwareVersion == 0 || firmwareVersion >= GitHubVersion)
              newVersionMsg = "";
        else  newVersionMsg = firmwareVersion_dspl + " nieuwere versie ("+GitHubVersion_dspl+") beschikbaar";
        document.getElementById('message').innerHTML = newVersionMsg;
        console.log(newVersionMsg);

      })
      .catch(function(error) {
        console.log(error);
        GitHubVersion_dspl   = "";
        GitHubVersion        = 0;
      });     

  } // readGitHubVersion()

    
  //============================================================================  
  function setEditType(eType) {
    if (eType == "ED") {
      console.log("Edit Energy Delivered!");
      monthType = eType;
      getMonths()
      showMonths(data, monthType);
    } else if (eType == "ER") {
      console.log("Edit Energy Returned!");
      monthType = eType;
      getMonths()
      showMonths(data, monthType);
    } else if (eType == "GD") {
      console.log("Edit Gas Delivered!");
      monthType = eType;
      getMonths()
      showMonths(data, monthType);
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
      var pattern = /^\d{1,1}(\.\d{1,5})?$/;
      var max = 1.99999;
    } else {
      var pattern = /^\d{1,2}(\.\d{1,2})?$/;
      var max = 99.99;
    }
    var newVal = document.getElementById(field).value;
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
  function translateToHuman(longName) {
    //for(var index = 0; index < (translateFields.length -1); index++) 
    for(var index = 0; index < translateFields.length; index++) 
    {
        if (translateFields[index][0] == longName)
        {
          return translateFields[index][1];
        }
    };
    return longName;
    
  } // translateToHuman()

  
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
    var YY = "20"+dateIn.substring(0,2);
    console.log("["+YY+"]["+(dateIn.substring(2,4)-1)+"]["+dateIn.substring(4,6)+"]");
    //-------------------YY-------------------(MM-1)----------------------DD---------------------HH--MM--SS
    var epoch = Date.UTC(YY, (dateIn.substring(2,4)-1), dateIn.substring(4,6), dateIn.substring(6,8), 1, 1);
    //console.log("epoch is ["+epoch+"]");

    return epoch;
    
  } // recidToEpoch()
  
  
  //============================================================================  
  function recidToWeekday(dateIn)
  {
    var YY = "20"+dateIn.substring(0,2);
    //-------------------YY-------------------(MM-1)----------------------DD---------------------HH--MM--SS
    var dt = new Date(Date.UTC(YY, (dateIn.substring(2,4)-1), dateIn.substring(4,6), 1, 1, 1));

    return dt.toLocaleDateString('nl-NL', {weekday: 'long'});
    
  } // epochToWeekday()
  
    
  //============================================================================  
  function round(value, precision) 
  {
    var multiplier = Math.pow(10, precision || 0);
    return Math.round(value * multiplier) / multiplier;
  }
    
  var translateFields = [
           [ "author",                    "Auteur" ]
          ,[ "identification",            "Slimme Meter ID" ]
          ,[ "p1_version",                "P1 Versie" ]
          ,[ "energy_delivered_tariff1",  "Energie Gebruikt tarief 1" ]
          ,[ "energy_delivered_tariff2",  "Energie Gebruikt tarief 2" ]
          ,[ "energy_returned_tariff1",   "Energie Opgewekt tarief 1" ]
          ,[ "energy_returned_tariff2",   "Energie Opgewekt tarief 2" ]
          ,[ "electricity_tariff",        "Electriciteit tarief" ]
          ,[ "power_delivered",           "Vermogen Gebruikt" ]
          ,[ "power_returned",            "Vermogen Opgewekt" ]
          ,[ "electricity_threshold",     "Electricity Threshold" ]
          ,[ "electricity_switch_position","Electricity Switch Position" ]
          ,[ "electricity_failures",      "Electricity Failures" ]
          ,[ "electricity_long_failures", "Electricity Long Failures" ]
          ,[ "electricity_failure_log",   "Electricity Failure log" ]
          ,[ "electricity_sags_l1",       "Electricity Sags l1" ]
          ,[ "electricity_sags_l2",       "Electricity Sags l2" ]
          ,[ "electricity_sags_l3",       "Electricity Sags l3" ]
          ,[ "electricity_swells_l1",     "Electricity Swells l1" ]
          ,[ "electricity_swells_l2",     "Electricity Swells l2" ]
          ,[ "electricity_swells_l3",     "Electricity Swells l3" ]
          ,[ "message_short",             "Korte Boodschap" ]
          ,[ "message_long",              "Lange Boodschap" ]
          ,[ "voltage_l1",                "Voltage l1" ]
          ,[ "voltage_l2",                "Voltage l2" ]
          ,[ "voltage_l3",                "Voltage l3" ]
          ,[ "current_l1",                "Current l1" ]
          ,[ "current_l2",                "Current l2" ]
          ,[ "current_l3",                "Current l3" ]
          ,[ "power_delivered_l1",        "Vermogen Gebruikt l1" ]
          ,[ "power_delivered_l2",        "Vermogen Gebruikt l2" ]
          ,[ "power_delivered_l3",        "Vermogen Gebruikt l3" ]
          ,[ "power_returned_l1",         "Vermogen Opgewekt l1" ]
          ,[ "power_returned_l2",         "Vermogen Opgewekt l2" ]
          ,[ "power_returned_l3",         "Vermogen Opgewekt l3" ]
          ,[ "gas_device_type",           "Gas Device Type" ]
          ,[ "gas_equipment_id",          "Gas Equipment ID" ]
          ,[ "gas_valve_position",        "Gas Klep Positie" ]
          ,[ "gas_delivered",             "Gas Gebruikt" ]
          ,[ "thermal_device_type",       "Thermal Device Type" ]
          ,[ "thermal_equipment_id",      "Thermal Equipment ID" ]
          ,[ "thermal_valve_position",    "Thermal Klep Positie" ]
          ,[ "thermal_delivered",         "Thermal Gebruikt" ]
          ,[ "water_device_type" ,        "Water Device Type" ]
          ,[ "water_equipment_id",        "Water Equipment ID" ]
          ,[ "water_valve_position",      "Water Klep Positie" ]
          ,[ "water_delivered",           "Water Gebruikt" ]
          ,[ "slave_device_type",         "Slave Device Type" ]
          ,[ "slave_equipment_id",        "Slave Equipment ID" ]
          ,[ "slave_valve_position",      "Slave Klep Positie" ]
          ,[ "slave_delivered",           "Slave Gebruikt" ]
          ,[ "ed_tariff1",                "Energy Verbruik Tarief-1/kWh" ]
          ,[ "ed_tariff2",                "Energy Verbruik Tarief-2/kWh" ]
          ,[ "er_tariff1",                "Energy Opgewekt Tarief-1/kWh" ]
          ,[ "er_tariff2",                "Energy Opgewekt Tarief-2/kWh" ]
          ,[ "gd_tariff" ,                "Gas Verbruik Tarief/m3" ]
          ,[ "electr_netw_costs",         "Netwerkkosten Energie/maand" ]
          ,[ "gas_netw_costs",            "Netwerkkosten Gas/maand" ]
          
          ,[ "smhasfaseinfo",             "SM Has Fase Info (0=No, 1=Yes)" ]
          ,[ "sm_has_fase_info",          "SM Has Fase Info (0=No, 1=Yes)" ]
          ,[ "oled_type",                 "OLED type (0=None, 1=SDD1306, 2=SH1106)" ]
          ,[ "oled_flip_screen",          "Flip OLED scherm (0=No, 1=Yes)" ]
          ,[ "tlgrm_interval",            "Telegram Lees Interval (Sec.)" ]
          ,[ "telegraminterval",          "Telegram Lees Interval (Sec.)" ]
          ,[ "index_page",                "Te Gebruiken index.html Pagina" ]
          ,[ "oled_screen_time",          "Oled Screen Time (Min., 0=infinite)" ]
          ,[ "mqttbroker",                "MQTT Broker IP/URL" ]
          ,[ "mqtt_broker",               "MQTT Broker IP/URL" ]
          ,[ "mqttbrokerport",            "MQTT Broker Poort" ]
          ,[ "mqtt_broker_port",          "MQTT Broker Poort" ]
          ,[ "mqttuser",                  "MQTT Gebruiker" ]
          ,[ "mqtt_user",                 "MQTT Gebruiker" ]
          ,[ "mqttpasswd",                "Password MQTT Gebruiker" ]
          ,[ "mqtt_passwd",               "Password MQTT Gebruiker" ]
          ,[ "mqtttoptopic",              "MQTT Top Topic" ]
          ,[ "mqtt_toptopic",             "MQTT Top Topic" ]
          ,[ "mqttinterval",              "Verzend MQTT Berichten (Sec.)" ]
          ,[ "mqtt_interval",             "Verzend MQTT Berichten (Sec.)" ]
          ,[ "mqttbroker_connected",      "MQTT broker connected" ]
          ,[ "mindergas_token",           "Mindergas Token" ]
          ,[ "mindergas_response",        "Mindergas Terugkoppeling" ]
          ,[ "mindergas_status",          "Mindergas Status (@dag | tijd)" ]

          ,[ "telegramcount",             "Telegrammen verwerkt" ]
          ,[ "telegramerrors",            "Telegrammen met fouten" ]          
          ,[ "fwversion",                 "Firmware Versie" ]
          ,[ "compiled",                  "Gecompileerd" ]
          ,[ "hostname",                  "HostName" ]
          ,[ "ipaddress",                 "IP adres" ]
          ,[ "macaddress",                "MAC adres" ]
          ,[ "indexfile",                 "Te Gebruiken Index.html Pagina" ]
          ,[ "freeheap",                  "Free Heap Space" ]
          ,[ "maxfreeblock",              "Max. Free Heap Blok" ]
          ,[ "chipid",                    "Chip ID" ]
          ,[ "coreversion",               "Core Versie" ]
          ,[ "sdkversion",                "SDK versie" ]
          ,[ "cpufreq",                   "CPU Frequency" ]
          ,[ "sketchsize",                "Sketch Size" ]
          ,[ "freesketchspace",           "Free Sketch Space" ]
          ,[ "flashchipid",               "Flash Chip ID" ]
          ,[ "flashchipsize",             "Flash Chip Size" ]
          ,[ "flashchiprealsize",         "Flash Chip Real Size" ]
          ,[ "spiffssize",                "SPIFFS Size" ]
          ,[ "flashchipspeed",            "Flash Chip Speed" ]
          ,[ "flashchipmode",             "Flash Chip Mode" ]
          ,[ "boardtype",                 "Board Type" ]
          ,[ "compileoptions",            "Compiler Opties" ]
          ,[ "ssid",                      "WiFi SSID" ]
          ,[ "wifirssi",                  "WiFi RSSI" ]
          ,[ "uptime",                    "Up Time [dagen] - [hh:mm]" ]
          ,[ "reboots",                   "Aantal keer opnieuw opgestart" ]
          ,[ "lastreset",                 "Laatste Reset reden" ]
];

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