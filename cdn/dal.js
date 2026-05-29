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
const UPDATE_DASH_HIST  = DEBUG ? 1000 * 30 : 1000 * 3600;
const UPDATE_DEVICE_INFO = DEBUG ? 1000 * 15 : 1000 * 60;
	
if (!DEBUG) {	//production
  	console.log = function () {};
}

const APIHOST = window.location.protocol+'//'+window.location.host;
const APIGW = APIHOST+'/api/';
const API_WS_URL = (window.location.protocol === "https:" ? "wss://" : "ws://") + window.location.hostname + ":81/ws";

// On-demand history payloads for the history and meter-reading views.
const URL_HISTORY_HOURS 	= "/RNGhours.json";
const URL_HISTORY_DAYS 		= "/RNGdays.json";
const URL_HISTORY_MONTHS 	= "/RNGmonths.json";

// Live/global API streams.
const URL_TIME    	 		= "/api/v2/dev/time";
const URL_DASH_LIVE        = APIGW + "v2/dash/live";
const URL_DASH_HIST        = APIGW + "v2/dash/hist";
const URL_SM_ACTUAL    	 	= APIGW + "v2/sm/actual";

// Legacy/special-purpose endpoints that stay request/response based.
const URL_SM_FIELDS			= APIGW + "v2/sm/fields";
const URL_SM_TELEGRAM		= APIGW + "v2/sm/telegram";
const URL_DEVICE_INFO   	= APIGW + "v2/dev/info";
const URL_DEVICE_SETTINGS   = APIGW + "v2/dev/settings";
const URL_INSIGHTS			= APIGW + "v2/stats";
const URL_EID_CLAIM			= "/eid/getclaim";
const URL_NETSWITCH			= "/netswitch.json";
const URL_VERSION_MANIFEST 	= "http://ota.smart-stuff.nl/v5/version-manifest.json?dummy=" + Date.now();
const URL_ESPHOME_MANIFEST  = "http://ota.smart-stuff.nl/esphome/esphome-manifest.json?dummy=" + Date.now();

const MAX_SM_ACTUAL     	= 15*60/5; //store the last 15 minutes (each interval is 10sec)
const MAX_FILECOUNT     	= 30;   //maximum filecount on the device is 30

var ota_url 				= "";  
let objDAL 					= null;

class ApiJsonStream {
	constructor({name, url, interval, parser}) {
		this.name = name;
		this.url = url;
		this.interval = interval;
		this.parser = parser;
		this.timer = 0;
		this.controller = null;
		this.running = false;
		this.inflight = false;
	}

	start({force=false} = {}) {
		if (this.running && !force) return;
		this.stop();
		this.running = true;
		this.#schedule(0);
	}

	stop() {
		this.running = false;
		if (this.timer) clearTimeout(this.timer);
		this.timer = 0;
		if (this.controller) this.controller.abort();
		this.controller = null;
		this.inflight = false;
	}

	refresh() {
		const singleShot = !this.running;
		if (singleShot) this.running = true;
		if (this.timer) clearTimeout(this.timer);
		this.timer = 0;
		this.#fetchOnce(singleShot);
	}

	#schedule(delay) {
		if (!this.running) return;
		if (this.timer) clearTimeout(this.timer);
		this.timer = setTimeout(() => this.#fetchOnce(), delay);
	}

	#fetchOnce(singleShot=false) {
		if (!this.running || this.inflight) return;
		if (typeof PauseAPI !== "undefined" && PauseAPI) {
			if (singleShot) {
				this.running = false;
				return;
			}
			this.#schedule(this.interval);
			return;
		}

		this.inflight = true;
		this.controller = new AbortController();
		console.log("DAL::stream(" + this.name + ") " + this.url);
		fetch(this.url, {cache: "no-store", signal: this.controller.signal})
			.then(response => {
				if (!response.ok) throw new Error("HTTP " + response.status);
				return response.json();
			})
			.then(json => this.parser(json))
			.catch(error => {
				if (error.name !== "AbortError") console.error("DAL::stream(" + this.name + ") - " + error.message);
			})
			.finally(() => {
				this.inflight = false;
				this.controller = null;
				if (singleShot) {
					this.running = false;
					return;
				}
				this.#schedule(this.interval);
			});
	}
}

class ApiWebSocketBridge {
	constructor({url, onMessage, onOpen, onClose}) {
		this.url = url;
		this.onMessage = onMessage;
		this.onOpen = onOpen;
		this.onClose = onClose;
		this.ws = null;
		this.reconnectTimer = 0;
		this.enabled = !!window.WebSocket;
		this.connected = false;
	}

	start() {
		if (!this.enabled || this.ws) return;
		this.#connect();
	}

	#connect() {
		if (!this.enabled) return;
		console.log("DAL::ws connect " + this.url);
		this.ws = new WebSocket(this.url);
		this.ws.onopen = () => {
			this.connected = true;
			this.onOpen?.();
		};
		this.ws.onmessage = event => {
			try {
				this.onMessage?.(JSON.parse(event.data));
			} catch (error) {
				console.error("DAL::ws parse - " + error.message);
			}
		};
		this.ws.onerror = () => {
			this.connected = false;
		};
		this.ws.onclose = () => {
			this.ws = null;
			const wasConnected = this.connected;
			this.connected = false;
			this.onClose?.(wasConnected);
			this.#scheduleReconnect();
		};
	}

	#scheduleReconnect() {
		if (!this.enabled || this.reconnectTimer) return;
		this.reconnectTimer = setTimeout(() => {
			this.reconnectTimer = 0;
			this.#connect();
		}, 10000);
	}
}
    
// The Data Access Layer groups browser<->dongle traffic by role:
// - global infra streams: time, device info/settings
// - tab-specific streams: dash live, dash hist, actual
// - on-demand data: hours/days/months, telegram, fields, insights, claim
class dsmr_dal_main{
	    constructor() {
	      // Cached payloads.
	      this.devinfo=[];
	      this.time=[];      
		  this.dev_settings=[];
		  this.version_manifest=[];
		  this.esphome_manifest=[];
		  this.insights=[];
		  this.dash_live=[];
		  this.dash_hist=[];
		  this.actual=[];
		  this.telegram=[];
	      this.fields=[];
	  	  this.months=[];
		  this.days=[];
		  this.hours=[];
		  this.eid_claim=[];
		  this.netswitch=[];
		  this.actual_history = [];

	      // Polling timers.
	      this.timerREFRESH_TIME = 0;
	      this.timerREFRESH_DASH_LIVE = 0;
	      this.timerREFRESH_DASH_HIST = 0;
	      this.timerREFRESH_ACTUAL = 0;  
		  this.timerREFRESH_DEVICE_INFO = 0;
		  this.timerREFRESH_MANIFEST = 0; 
		  this.esphome_manifest_fetched_at = 0;
	      
	      // Cache timestamps.
		  this.dash_hist_fetched_at = 0;
		  this.device_info_fetched_at = 0;
		  this.callback=null;

		  this.streams = {
			time: new ApiJsonStream({
				name: "time",
				url: URL_TIME,
				interval: UPDATE_ACTUAL,
				parser: this.parseTIME.bind(this)
			}),
			dash_live: new ApiJsonStream({
				name: "dash_live",
				url: URL_DASH_LIVE,
				interval: UPDATE_ACTUAL,
				parser: this.parseDashLive.bind(this)
			}),
			dash_hist: new ApiJsonStream({
				name: "dash_hist",
				url: URL_DASH_HIST,
				interval: UPDATE_DASH_HIST,
				parser: this.parseDashHist.bind(this)
			}),
			actual: new ApiJsonStream({
				name: "actual",
				url: URL_SM_ACTUAL,
				interval: UPDATE_ACTUAL,
				parser: this.parseActual.bind(this)
			})
		  };

		  this.streamDesired = {
			time: false,
			dash_live: false,
			dash_hist: false,
			actual: false
		  };
		  this.ws = new ApiWebSocketBridge({
			url: API_WS_URL,
			onMessage: this.#handleWsMessage.bind(this),
			onOpen: this.#handleWsOpen.bind(this),
			onClose: this.#handleWsClose.bind(this)
		  });
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
		  this.ws.start();
		  this.startTimePolling();
	    } 

		#startLiveSource(source) {
			this.streamDesired[source] = true;
			if (this.ws.connected) {
				this.streams[source].refresh();
				return;
			}
			this.streams[source].start();
		}

		#stopLiveSource(source) {
			this.streamDesired[source] = false;
			this.streams[source].stop();
		}

		#handleWsOpen() {
			console.log("DAL::ws connected");
			Object.values(this.streams).forEach(stream => stream.stop());
		}

		#handleWsClose(wasConnected) {
			if (wasConnected) console.log("DAL::ws disconnected; falling back to fetch streams");
			Object.keys(this.streamDesired).forEach(source => {
				if (this.streamDesired[source]) this.streams[source].start();
			});
		}

		#handleWsMessage(message) {
			const source = message?.source;
			if (!source || !this.streamDesired[source]) return;
			switch (source) {
				case "time": this.parseTIME(message.data); break;
				case "dash_live": this.parseDashLive(message.data); break;
				case "dash_hist": this.parseDashHist(message.data); break;
				case "actual": this.parseActual(message.data); break;
				default: console.log("DAL::ws unknown source " + source);
			}
		}
			
		// Convenience helper for callers that want all history refreshed together.
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
			this.streams.dash_live.refresh();
		}

		startDashLivePolling() {
			this.#startLiveSource("dash_live");
		}

		stopDashLivePolling() {
			this.#stopLiveSource("dash_live");
		}

		parseDashLive(json) {
			this.dash_live = json;
			this.callback?.('dash_live', json);
		}

		refreshDashHist() {
			console.log("DAL::refreshDashHist");
			this.streams.dash_hist.refresh();
		}

		startDashHistPolling() {
			this.#startLiveSource("dash_hist");
		}

		stopDashHistPolling() {
			this.#stopLiveSource("dash_hist");
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
	
	    // Device info/settings stay request/response based and share one cache age.
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

		refreshESPHomeManifest(){
		  this.fetchDataJSON(URL_ESPHOME_MANIFEST, this.parseESPHomeManifest.bind(this));
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

	parseESPHomeManifest(json){
	  console.log("DAL::parseESPHomeManifest");
	  this.esphome_manifest = json;
	  this.esphome_manifest_fetched_at = Date.now();
	  this.callback?.('esphomemanifest', json);
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
	      this.streams.time.refresh();
		}

		startTimePolling(){
			this.#startLiveSource("time");
		}

    parseTIME(json){
		this.time = json;
		this.callback?.('time', json);
    }

    // refresh and parse actual data, store and add to history
    //refresh every 10 sec
    refreshActual(){
      console.log("DAL::refreshActual");
      this.streams.actual.refresh();
    }

		startActualPolling(){
			this.#startLiveSource("actual");
		}

		stopActualPolling(){
			this.#stopLiveSource("actual");
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
	getESPHomeManifest(){
		return this.esphome_manifest;
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
	      return this.dash_live?.solar ?? [];
	    }
	    getAccu(){
	      return this.dash_live?.accu ?? [];
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
	case "esphomemanifest": renderESPHomeMigrationCard(); break;
    case "days": expandData(json);refreshHistData("Days");break;
	case "hours": expandData(json);refreshHistData("Hours");break;
	case "months":
		expandData(json);
		refreshHistData("Months");
		if (activeTab=="bEditMonths") EditMonths();
		break;
	case "dash_live":
		if (activeTab=="bDashTab") {
			refreshDashboard(json);
			UpdateSolar();
			UpdateAccu();
		}
		if (json?.eid) ProcessEIDPlanner(json.eid);
		break;
	case "dash_hist": applyDashHistory(json); break;
	case "actual": ProcesActual(json);break;
	case "insights": InsightData(json);break;
	case "fields": SetOnSettings(json); if (activeTab=="bDashTab") refreshDashboard(json);else parseSmFields(json); break;
	case "telegram": document.getElementById("TelData").textContent = json; break;
	case "eid_claim":ProcessEIDClaim(json); break;
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
