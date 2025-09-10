/*
***************************************************************************  
**  Copyright (c) 2025 Smartstuff
**
**  TERMS OF USE: MIT License.                                      
***************************************************************************      
*/

const DEBUG = false;

const UPDATE_HIST 		= DEBUG ? 1000 * 10 : 1000 * 300;
const UPDATE_ACTUAL 	= DEBUG ? 1000 * 5 	: 1000 * 5;
const UPDATE_SOLAR		= DEBUG ? 1000 * 30 : 1000 * 60;
const UPDATE_ACCU		= DEBUG ? 1000 * 30 : 1000 * 60;
const UPDATE_PLAN		= DEBUG ? 1000 * 10 : 1000 * 60;
	
if (!DEBUG) {	//production
  	console.log = function () {};
}

const APIHOST = window.location.protocol+'//'+window.location.host;
const APIGW = APIHOST+'/api/';

const URL_HISTORY_HOURS 	= "/RNGhours.json";
const URL_HISTORY_DAYS 		= "/RNGdays.json";
const URL_HISTORY_MONTHS 	= "/RNGmonths.json";
const URL_TIME    	 		= "/api/v2/dev/time";
const URL_SM_ACTUAL    	 	= APIGW + "v2/sm/actual";
const URL_SM_FIELDS			= APIGW + "v2/sm/fields";
const URL_SM_TELEGRAM		= APIGW + "v2/sm/telegram";
const URL_DEVICE_INFO   	= APIGW + "v2/dev/info";
const URL_DEVICE_SETTINGS   = APIGW + "v2/dev/settings";
const URL_SOLAR				= APIGW + "v2/gen";
const URL_ACCU				= APIGW + "v2/accu";
const URL_INSIGHTS			= APIGW + "v2/stats";
const URL_EID_CLAIM			= "/eid/getclaim";
const URL_EID_PLAN			= "/eid/planner";
const URL_NETSWITCH			= "/netswitch.json";
const URL_VERSION_MANIFEST 	= "http://ota.smart-stuff.nl/v5/version-manifest.json?dummy=" + Date.now();

const MAX_SM_ACTUAL     	= 15*60/5; //store the last 15 minutes (each interval is 10sec)
const MAX_FILECOUNT     	= 30;   //maximum filecount on the device is 30

var ota_url 				= "";  
let objDAL 					= null;
    
   //The Data Access Layer for the DSMR
  // - acts as an cache between frontend and server
  // - schedules refresh to keep data fresh
  // - stores data for the history functions
  class dsmr_dal_main{
    constructor() {
      this.devinfo=[];
      this.time=[];      
	  this.dev_settings=[];
	  this.version_manifest=[];
	  this.insights=[];
      this.actual=[];
	  this.telegram=[];
      this.fields=[];
	  this.solar=[];
	  this.accu=[];
  	  this.months=[];
	  this.days=[];
	  this.hours=[];
	  this.eid_claim=[];
	  this.netswitch=[];
	  this.eid_planner=[];
      this.actual_history = [];
      this.timerREFRESH_TIME = 0;
      this.timerREFRESH_HIST = 0;
      this.timerREFRESH_ACTUAL = 0;  
      this.timerREFRESH_DAYS = 0;  
	  this.timerREFRESH_MANIFEST = 0; 
	  this.timerREFRESH_SOLAR = 0;  	    
	  this.timerREFRESH_ACCU = 0;  
	  this.timerREFRESH_EIDPLAN = 0;  	  
      this.callback=null;
      }
  
    fetchDataJSON(url, fnHandleData) {
	  Spinner(true);
      console.log("DAL::fetchDataJSON( "+url+" )");
      fetch(url)
      .then(response => response.json())
      .then(json => { fnHandleData(json); })
      .catch(function (error) {
        console.error("dal::fetchDataJSON(" + url + ") - " + error.message);
        var p = document.createElement('p');
        p.appendChild( document.createTextNode('Error: ' + error.message) );
      });
      Spinner(false);
    }
    
     fetchDataTEXT(url, fnHandleData) {
	  Spinner(true);
      console.log("DAL::fetchDataTEXT( "+url+" )");
      fetch(url)
      .then(response => response.text())
      .then(json => { fnHandleData(json); })
      .catch(function (error) {
        console.error("dal::fetchDataTEXT() - " + error.message);
        var p = document.createElement('p');
        p.appendChild( document.createTextNode('Error: ' + error.message) );
      });
      Spinner(false);
    }

    //use a callback when you want to know if the data is updated
    setCallback(fnCB){
      this.callback = fnCB;
    }
  
    init(){
      this.refreshDeviceInformation();
	  this.refreshTime();
      this.refreshHist();
      this.refreshSolar();
	  this.refreshAccu();
	  this.refreshActual();
// 	  this.refreshFields(); //always called in initial call (dashboard)

    } 
	
	refreshEIDPlanner(){
		console.log("DAL::refreshEIDPlanner");
		clearInterval(this.timerREFRESH_EIDPLAN);
      	this.fetchDataJSON( URL_EID_PLAN, this.parseEIDplan.bind(this));
      	this.timerREFRESH_EIDPLAN = setInterval(this.refreshEIDPlanner.bind(this), UPDATE_PLAN);
	}
	
	parseEIDplan(json){
		this.eid_planner = json;
		this.callback?.('eid_planner', json);
	}
	
	refreshHist()
	{
		console.log("DAL::refreshHist");
		clearInterval(this.timerREFRESH_HIST);
		this.fetchDataJSON( URL_HISTORY_MONTHS, this.parseMonths.bind(this));
		this.fetchDataJSON( URL_HISTORY_DAYS, this.parseDays.bind(this));
		this.fetchDataJSON( URL_HISTORY_HOURS, this.parseHours.bind(this));
    	this.timerREFRESH_HIST = setInterval(this.refreshHist.bind(this), UPDATE_HIST);
	}
	parseMonths(json)
	{
		console.log("parseMonths");
		console.log(json);
		//TODO: filter out the corrupt row
		this.months = json;
		this.callback?.('months', json);
	}
	parseDays(json)
	{
		console.log("parseDays");
		console.log(json);
		//TODO: filter out the corrupt row
		this.days = json;
		this.callback?.('days', json);
	}
	parseHours(json)
	{
		console.log("DAL::parseHours");
		console.log(json);
		//TODO: filter out the corrupt row
		this.hours = json;
		this.callback?.('hours', json);
	}


	refreshNetSwitch(){
		console.log("DAL::refreshNetSwitch");
		this.fetchDataJSON( URL_NETSWITCH, this.parseNetSwitch.bind(this));
	}

	parseNetSwitch(json){
		this.netswitch = json;
      	this.callback?.('netswitch', json);
	}
	
	refreshEIDClaim(){
		console.log("DAL::refreshEIDClaim");
      	this.fetchDataJSON( URL_EID_CLAIM, this.parseEIDClaim.bind(this));
	}
	
	parseEIDClaim(json){
		this.eid_claim = json;
      	this.callback?.('eid_claim', json);
	}
	
	refreshInsights(){
		console.log("DAL::refreshInsights");
      	this.fetchDataJSON( URL_INSIGHTS, this.parseInsights.bind(this));
	}
	
	parseInsights(json){
		this.insights = json;
      	this.callback?.('insights', json);
	}
	
	refreshSolar(){
		console.log("DAL::refreshSolar");
		clearInterval(this.timerREFRESH_SOLAR);
      	this.fetchDataJSON( URL_SOLAR, this.parseSolar.bind(this));
      	this.timerREFRESH_SOLAR = setInterval(this.refreshSolar.bind(this), UPDATE_SOLAR);
	}
	
	parseSolar(json){
		this.solar = json;
      	this.callback?.('solar', json);
	}

	refreshAccu(){
		console.log("DAL::refreshAccu");
		clearInterval(this.timerREFRESH_ACCU);
      	this.fetchDataJSON( URL_ACCU, this.parseAccu.bind(this));
      	this.timerREFRESH_ACCU = setInterval(this.refreshAccu.bind(this), UPDATE_ACCU);
	}
	
	parseAccu(json){
		this.accu = json;
      	this.callback?.('accu', json);
	}
	
    //single call; no timer
    refreshDeviceInformation(){
      console.log("DAL::refreshDeviceInformation");
      this.fetchDataJSON( URL_DEVICE_INFO, this.parseDeviceInfo.bind(this));
      this.fetchDataJSON( URL_DEVICE_SETTINGS, this.parseDeviceSettings.bind(this));
    }

    refreshManifest(){
	  clearInterval(this.timerREFRESH_MANIFEST);
	  this.fetchDataJSON( "http://" + ota_url + "version-manifest.json?dummy=" + Date.now() , this.parseVersionManifest.bind(this));
	  this.timerREFRESH_MANIFEST = setInterval(this.refreshManifest.bind(this), 3600 * 12 * 1000); //every 12h
    }

    //store result and call callback if set
    parseDeviceSettings(json){
      this.dev_settings = json;
      
      //because the manifest check uses the ota_url.value from the device settings
      "ota_url" in json ? ota_url = json.ota_url.value: ota_url = "ota.smart-stuff.nl/v5/";
	  this.refreshManifest();      
      this.callback?.('dev_settings', json);
    }
        
    //store result and call callback if set
    parseDeviceInfo(json){
      this.devinfo = json;
      this.callback?.('devinfo', json);
    }
    
    //store result and call callback if set
	parseVersionManifest(json){
	  console.log("DAL::parseVersionManifest");
	  this.version_manifest = json;
      this.callback?.('versionmanifest', json);
	}

	refreshTelegram(){
	  console.log("DAL::refreshTelegram");
      this.fetchDataTEXT( URL_SM_TELEGRAM, this.parseTelegram.bind(this));
	}
    
    parseTelegram(text){
		this.telegram = text;
		this.callback?.('telegram', text);
    }
    
    refreshFields(){
      console.log("DAL::refreshFields");
      this.fetchDataJSON( URL_SM_FIELDS, this.parseFields.bind(this));
    }
    
    parseFields(json){
		this.fields = json;
		this.callback?.('fields', json);
    }

	refreshTime(){
      console.log("DAL::refreshTime");
      clearInterval(this.timerREFRESH_TIME);      
      this.fetchDataJSON( URL_TIME, this.parseTIME.bind(this));
      this.timerREFRESH_TIME = setInterval(this.refreshTime.bind(this), UPDATE_ACTUAL);	
	}

    parseTIME(json){
		this.time = json;
		this.callback?.('time', json);
    }

    // refresh and parse actual data, store and add to history
    //refresh every 10 sec
    refreshActual(){
      console.log("DAL::refreshActual");
//       clearInterval(this.timerREFRESH_ACTUAL);      
      this.fetchDataJSON( URL_SM_ACTUAL, this.parseActual.bind(this));
//       this.timerREFRESH_ACTUAL = setInterval(this.refreshActual.bind(this), UPDATE_ACTUAL);
    }
    
    parseActual(json){
		this.actual = json;
		this.#addActualHistory( json );
		this.callback?.('actual', json);
    }
    
    #addActualHistory(json){
      if( this.actual_history.length >= MAX_SM_ACTUAL) this.actual_history.shift();
      this.actual_history.push(json);
    }
  
    //
    // getters
    //
    getDeviceInfo(){
      return this.devinfo;
    }
    getDeviceSettings(){
    	return this.dev_settings;
    }
	getVersionManifest(){
		return this.version_manifest;
	}
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
// 		console.log("sYYMM: "+sYYMM);
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
    getSolar(){
      return this.solar;
    }
    getAccu(){
      return this.accu;
    }
    
    getFields(){
      return this.fields;
    }    
    
    getActual(){
      return this.actual;
    }    
    //the last (MAX_ACTUAL_HISTORY) actuals
    getActualHistory(){
      return this.actual_history;
    }
  }
  
//callback function for the DAL
function updateFromDAL(source, json)
{
  console.log("updateFromDAL(); source="+source);
//   console.log(json);
  
  switch(source)
  {
    case "time": refreshTime(json); update_reconnected=true; break;
    case "devinfo": parseDeviceInfo(json); break;
    case "dev_settings": ParseDevSettings(); refreshSettings();break;
    case "versionmanifest": parseVersionManifest(json); break;
    case "days": expandData(json);refreshHistData("Days");break;
	case "hours": expandData(json);refreshHistData("Hours");break;
	case "months": expandData(json);refreshHistData("Months");break;
	case "actual": ProcesActual(json);break;
	case "solar": UpdateSolar();break;
	case "accu": UpdateAccu();break;
	case "insights": InsightData(json);break;
	case "fields": SetOnSettings(json); if (activeTab=="bDashTab") refreshDashboard(json);else parseSmFields(json); break;
	case "telegram": document.getElementById("TelData").textContent = json; break;
	case "eid_claim":ProcessEIDClaim(json); break;
	case "eid_planner":ProcessEIDPlanner(json); break;
	case "netswitch": refreshNetSwitch(json);break;
    default:
      console.error("missing handler; source="+source);
      break;
  }
}

function initDAL(callbackFn) {
  objDAL = new dsmr_dal_main();
  objDAL.setCallback(callbackFn);
  objDAL.init();
}