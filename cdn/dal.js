/*
***************************************************************************  
**  Copyright (c) 2026 Smartstuff
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
const UPDATE_DASH_HIST  = DEBUG ? 1000 * 30 : 1000 * 3600;
const UPDATE_DEVICE_INFO = DEBUG ? 1000 * 15 : 1000 * 60;
	
if (!DEBUG) {	//production
  	console.log = function () {};
}

const APIHOST = window.location.protocol+'//'+window.location.host;
const APIGW = APIHOST+'/api/';

const URL_HISTORY_HOURS 	= "/RNGhours.json";
const URL_HISTORY_DAYS 		= "/RNGdays.json";
const URL_HISTORY_MONTHS 	= "/RNGmonths.json";
const URL_TIME    	 		= "/api/v2/dev/time";
const URL_DASH_LIVE        = APIGW + "v2/dash/live";
const URL_DASH_HIST        = APIGW + "v2/dash/hist";
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
	      this.dash_live=[];
	      this.dash_hist=[];
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
	      this.timerREFRESH_DASH_LIVE = 0;
	      this.timerREFRESH_DASH_HIST = 0;
	      this.timerREFRESH_ACTUAL = 0;  
		  this.timerREFRESH_DEVICE_INFO = 0;
		  this.timerREFRESH_MANIFEST = 0; 
		  this.timerREFRESH_SOLAR = 0;  	    
		  this.timerREFRESH_ACCU = 0;  
		  this.timerREFRESH_EIDPLAN = 0;  	  
	      this.dash_hist_fetched_at = 0;
	      this.device_info_fetched_at = 0;
	      this.callback=null;
	      }

		#setPollingTimer(timerName, fnRefresh, interval) {
			this.stopPollingTimer(timerName);
			this[timerName] = setInterval(fnRefresh, interval);
		}

		stopPollingTimer(timerName) {
			if (!this[timerName]) return;
			clearInterval(this[timerName]);
			this[timerName] = 0;
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
	      this.ensureDeviceInformation(true);
		  this.startTimePolling();
	    } 
		
		refreshEIDPlanner(){
			console.log("DAL::refreshEIDPlanner");
	      	this.fetchDataJSON( URL_EID_PLAN, this.parseEIDplan.bind(this));
		}

		startEIDPlannerPolling(){
			this.refreshEIDPlanner();
			this.#setPollingTimer("timerREFRESH_EIDPLAN", this.refreshEIDPlanner.bind(this), UPDATE_PLAN);
		}

		stopEIDPlannerPolling(){
			this.stopPollingTimer("timerREFRESH_EIDPLAN");
		}
		
		parseEIDplan(json){
			this.eid_planner = json;
		console.log("DAL::parseEIDplan -> " + json );
		this.callback?.('eid_planner', json);
	}
	
		refreshHist() {
			console.log("DAL::refreshHist");
			this.refreshHistoryHours();
			this.refreshHistoryDays();
			this.refreshHistoryMonths();
		}

		refreshHistoryHours() {
			this.fetchDataJSON(URL_HISTORY_HOURS, this.parseHours.bind(this));
		}

		refreshHistoryDays() {
			this.fetchDataJSON(URL_HISTORY_DAYS, this.parseDays.bind(this));
		}

		refreshHistoryMonths() {
			this.fetchDataJSON(URL_HISTORY_MONTHS, this.parseMonths.bind(this));
		}

		refreshDashLive() {
			console.log("DAL::refreshDashLive");
			this.fetchDataJSON(URL_DASH_LIVE, this.parseDashLive.bind(this));
		}

		startDashLivePolling() {
			this.refreshDashLive();
			this.#setPollingTimer("timerREFRESH_DASH_LIVE", this.refreshDashLive.bind(this), UPDATE_ACTUAL);
		}

		stopDashLivePolling() {
			this.stopPollingTimer("timerREFRESH_DASH_LIVE");
		}

		parseDashLive(json) {
			this.dash_live = json;
			this.solar = json?.solar ?? [];
			this.accu = json?.accu ?? [];
			this.eid_planner = json?.eid ?? [];
			this.callback?.('dash_live', json);
		}

		refreshDashHist() {
			console.log("DAL::refreshDashHist");
			this.fetchDataJSON(URL_DASH_HIST, this.parseDashHist.bind(this));
		}

		startDashHistPolling() {
			this.ensureDashHist(true);
			this.#setPollingTimer("timerREFRESH_DASH_HIST", this.refreshDashHist.bind(this), UPDATE_DASH_HIST);
		}

		stopDashHistPolling() {
			this.stopPollingTimer("timerREFRESH_DASH_HIST");
		}

		ensureDashHist(force=false) {
			const isStale = !this.dash_hist_fetched_at || (Date.now() - this.dash_hist_fetched_at) >= UPDATE_DASH_HIST;
			if (force || isStale || !this.dash_hist?.days?.length) this.refreshDashHist();
		}

		parseDashHist(json) {
			this.dash_hist = json;
			this.dash_hist_fetched_at = Date.now();
			this.callback?.('dash_hist', json);
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
      	this.fetchDataJSON( URL_SOLAR, this.parseSolar.bind(this));
	}

	startSolarPolling(){
		this.refreshSolar();
		this.#setPollingTimer("timerREFRESH_SOLAR", this.refreshSolar.bind(this), UPDATE_SOLAR);
	}

	stopSolarPolling(){
		this.stopPollingTimer("timerREFRESH_SOLAR");
	}
	
	parseSolar(json){
		this.solar = json;
      	this.callback?.('solar', json);
	}

	refreshAccu(){
		console.log("DAL::refreshAccu");
      	this.fetchDataJSON( URL_ACCU, this.parseAccu.bind(this));
	}

	startAccuPolling(){
		this.refreshAccu();
		this.#setPollingTimer("timerREFRESH_ACCU", this.refreshAccu.bind(this), UPDATE_ACCU);
	}

	stopAccuPolling(){
		this.stopPollingTimer("timerREFRESH_ACCU");
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

		ensureDeviceInformation(force=false){
			const isStale = !this.device_info_fetched_at || (Date.now() - this.device_info_fetched_at) >= UPDATE_DEVICE_INFO;
			if (force || isStale || !Object.keys(this.devinfo || {}).length || !Object.keys(this.dev_settings || {}).length) {
				this.refreshDeviceInformation();
			}
		}

		startDeviceInformationPolling(){
			this.ensureDeviceInformation(true);
			this.#setPollingTimer("timerREFRESH_DEVICE_INFO", this.refreshDeviceInformation.bind(this), UPDATE_DEVICE_INFO);
		}

		stopDeviceInformationPolling(){
			this.stopPollingTimer("timerREFRESH_DEVICE_INFO");
		}

	    refreshManifest(){
		  this.fetchDataJSON( "http://" + ota_url + "version-manifest.json?dummy=" + Date.now() , this.parseVersionManifest.bind(this));
	    }

		startManifestPolling(){
			this.refreshManifest();
			this.#setPollingTimer("timerREFRESH_MANIFEST", this.refreshManifest.bind(this), 3600 * 12 * 1000);
		}

    //store result and call callback if set
    parseDeviceSettings(json){
      this.dev_settings = json;
      
	      //because the manifest check uses the ota_url.value from the device settings
	      "ota_url" in json ? ota_url = json.ota_url.value: ota_url = "ota.smart-stuff.nl/v5/";
		  this.startManifestPolling();      
	      this.callback?.('dev_settings', json);
	    }
        
    //store result and call callback if set
	    parseDeviceInfo(json){
	      this.devinfo = json;
	      this.device_info_fetched_at = Date.now();
	      this.callback?.('devinfo', json);
	    }
    
    //store result and call callback if set
	parseVersionManifest(json){
	  console.log("DAL::parseVersionManifest");
	  this.version_manifest = json;
      this.callback?.('versionmanifest', json);
	}

	refreshTelegram(){
	  console.log("DAL::refreshRawSmartMeterData");
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
	      this.fetchDataJSON( URL_TIME, this.parseTIME.bind(this));
		}

		startTimePolling(){
			this.refreshTime();
			this.#setPollingTimer("timerREFRESH_TIME", this.refreshTime.bind(this), UPDATE_ACTUAL);
		}

    parseTIME(json){
		this.time = json;
		this.callback?.('time', json);
    }

    // refresh and parse actual data, store and add to history
    //refresh every 10 sec
    refreshActual(){
      console.log("DAL::refreshActual");
      this.fetchDataJSON( URL_SM_ACTUAL, this.parseActual.bind(this));
    }

		startActualPolling(){
			this.refreshActual();
			this.#setPollingTimer("timerREFRESH_ACTUAL", this.refreshActual.bind(this), UPDATE_ACTUAL);
		}

		stopActualPolling(){
			this.stopPollingTimer("timerREFRESH_ACTUAL");
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
	getDashHist()
	{
		return this.dash_hist;
	}
	getDashLive()
	{
		return this.dash_live;
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
    case "dev_settings":
		ParseDevSettings();
		if (!(activeTab == "bEditSettings" && hasDirtySettings())) refreshSettings();
		break;
    case "versionmanifest": parseVersionManifest(json); break;
    case "days": expandData(json);refreshHistData("Days");break;
	case "hours": expandData(json);refreshHistData("Hours");break;
	case "months":
		expandData(json);
		refreshHistData("Months");
		if (activeTab=="bEditMonths") EditMonths();
		break;
	case "dash_live":
		if (activeTab=="bDashTab") {
			refreshDashboard(dashLiveToFields(json));
			UpdateSolar();
			UpdateAccu();
		}
		if (json?.eid) ProcessEIDPlanner(json.eid);
		break;
	case "dash_hist": applyDashHistory(json); break;
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
