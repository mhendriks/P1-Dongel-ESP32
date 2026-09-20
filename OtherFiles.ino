/*
***************************************************************************  
**  Program  : settings_status_files, part of DSMRloggerAPI
**
**  Copyright (c) 2023 Martijn Hendriks
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

bool GetFile(String filename, String path ){
  HTTPClient http;
  if(wifiClient.connect(HOST_DATA_FILES, 443)) {
    Debugln(path + filename);
      http.begin(path + filename);
      int httpResponseCode = http.GET();
//      Serial.print(F("HTTP Response code: "));Serial.println(httpResponseCode);
      if (httpResponseCode == 200 ){
        String payload = http.getString();
  //      Serial.println(payload);
        File file = LittleFS.open(filename, "w"); // open for reading and writing
        if (!file) DebugTln(F("open file FAILED!!!\r\n"));
        else {
          file.print(payload);
          file.close();
          http.end();
          wifiClient.stop(); //end client connection to server
          return true;
        }
        file.close();
      }
      http.end(); 
      wifiClient.stop(); //end client connection to server  
  } else {
    DebugTln(F("connection to server failed"));
  }
  return false;
}

//refactor settingsfile
/*
  1) check is settingsfile exists
  2) ifnot of parsefout maak nieuwe file op basis van template
  3) ifexists read contents
  twee opties: 
  a) alles eerst valideren <---- deze wel het prettigst maar tijdens gebruik aanpassingen worden wellicht niet opgemerkt 
  b) valideren op moment dat het nodig is -> niet aanwezig dan default waarde opnemen in model

 */

template <typename TSource>
void writeToJsonFile(const TSource &doc, File &_file) 
{
  if (!serializeJson(doc, _file) ||!FSmounted) {
      DebugTln(F("write(): Failed to write to json file"));
      return;  
  } 
  else {
    DebugTln(F("write(): json file writen"));
    //verbose write output
    if (Verbose1) 
    {
      DebugTln(F("Save to json file:"));
      serializeJson(doc, TelnetStream); //print settingsfile to telnet output
#ifdef DEBUG      
      serializeJson(doc, USBSerial); //print settingsfile to serial output    
#endif      
    } // Verbose1  
  }
    
  _file.flush();
  _file.close(); 
}

//=======================================================================
void writeSettingsDirect() {
  if ( !FSmounted ) return;

  DebugTln(F("Writing to [" SETTINGS_FILE "] ..."));

  File SettingsFile = LittleFS.open(SETTINGS_FILE, FILE_WRITE); // open for writing
  
  if (!SettingsFile) 
  {
    DebugTln("open(" SETTINGS_FILE ", 'w') FAILED!!! --> Bailout\r\n");
    return;
  }
  
  if (strlen(settingIndexPage) < 7) strCopy(settingIndexPage, (sizeof(settingIndexPage) -1), _DEFAULT_HOMEPAGE);
  if (settingMQTTbrokerPort < 1)    settingMQTTbrokerPort = 1883;
    
  DebugTln(F("Start writing setting data to json settings file"));
  
  JsonDocument docw; 
  docw["Hostname"] = settingHostname;
  docw["EnergyDeliveredT1"] = settingEDT1;
  docw["EnergyDeliveredT2"] = settingEDT2;
  docw["EnergyReturnedT1"] = settingERT1;
  docw["EnergyReturnedT2"] = settingERT2;
  docw["GASDeliveredT"] = settingGDT;
  docw["WaterDelivered"] = settingWDT;
  docw["EnergyVasteKosten"] = settingENBK;
  docw["GasVasteKosten"] = settingGNBK;
  docw["WaterVasteKosten"] = settingWNBK;
  docw["OverVoltageThreshold"] = settingOvervoltageThreshold;
  docw["CTFactor"] = settingCTFactor;
  docw["VTFactor"] = settingVTFactor;
  docw["Fuse"] = settingFuse;
  docw["Phases"] = settingPhases;
  // docw["SmHasFaseInfo"] = settingSmHasFaseInfo;
  docw["IndexPage"] = settingIndexPage;
  yield();
  docw["MQTTbroker"] = settingMQTTbroker;
  docw["MQTTbrokerPort"] = settingMQTTbrokerPort;
  docw["MQTTUser"] = settingMQTTuser;
  docw["MQTTpasswd"] = settingMQTTpasswd;
  docw["MQTTinterval"] = settingMQTTinterval;
  docw["MQTTtopTopic"] = settingMQTTtopTopic;
  docw["mqtt-enabled"] = bMQTTenabled;
  docw["mqtt_tls"] = bMQTToverTLS;
  
  docw["LED"] = LEDenabled;
  docw["ota"] = BaseOTAurl;
  docw["enableHistory"] = EnableHistory;
  docw["watermeter"] = WtrMtr;
  docw["waterfactor"] = WtrFactor;
  docw["HAdiscovery"] = EnableHAdiscovery;
  docw["basic-auth"]["user"] = bAuthUser;
  docw["basic-auth"]["pass"] = bAuthPW;
  docw["auto-update"] = bAutoUpdate;
  docw["pre40"] = bPre40;
  docw["act-json-mqtt"] = bActJsonMQTT;
  docw["raw-port"] = bRawPort;
  docw["led-prt"] = bLED_PRT;
  docw["mb_map"] = SelMap;
  docw["mb_id"] = mb_config.id;
  docw["mb_port"] = mb_config.port;
  docw["mb_baud"] = mb_config.baud;
  docw["mb_parity"] = mb_config.parity - 134217700;
  docw["mb_monitor"] = bModbusMonitor;
  docw["battery_driver"] = batteryConnectorDriver;
  docw["battery_modbus_enabled"] = modbusBatteryConfig.enabled;
  docw["battery_modbus_ip"] = modbusBatteryConfig.ip;
  docw["battery_modbus_port"] = modbusBatteryConfig.port;
  docw["battery_modbus_unit_id"] = modbusBatteryConfig.id;
  docw["battery_modbus_poll_seconds"] = modbusBatteryConfig.pollIntervalSeconds;
  docw["battery_solaredge_site_id"] = solarEdgeBatteryConfig.siteId;
  docw["battery_solaredge_api_key"] = solarEdgeBatteryConfig.apiKey;
  docw["battery_solaredge_poll_seconds"] = solarEdgeBatteryConfig.pollIntervalSeconds;
  docw["battery_power_register"] = modbusBatteryConfig.activePower.registerAddress;
  docw["battery_power_type"] = modbusBatteryConfig.activePower.valueType;
  docw["battery_power_scale"] = modbusBatteryConfig.activePower.scale;
  docw["battery_power_word_swap"] = modbusBatteryConfig.activePower.wordSwap;
  docw["battery_soc_register"] = modbusBatteryConfig.stateOfCharge.registerAddress;
  docw["battery_soc_type"] = modbusBatteryConfig.stateOfCharge.valueType;
  docw["battery_soc_scale"] = modbusBatteryConfig.stateOfCharge.scale;
  docw["battery_soc_word_swap"] = modbusBatteryConfig.stateOfCharge.wordSwap;
  docw["battery_state_register"] = modbusBatteryConfig.operatingState.registerAddress;
  docw["battery_state_type"] = modbusBatteryConfig.operatingState.valueType;
  docw["battery_state_scale"] = modbusBatteryConfig.operatingState.scale;
  docw["battery_state_word_swap"] = modbusBatteryConfig.operatingState.wordSwap;
  docw["battery_available_capacity_register"] = modbusBatteryConfig.availableCapacity.registerAddress;
  docw["battery_available_capacity_type"] = modbusBatteryConfig.availableCapacity.valueType;
  docw["battery_available_capacity_scale"] = modbusBatteryConfig.availableCapacity.scale;
  docw["battery_available_capacity_word_swap"] = modbusBatteryConfig.availableCapacity.wordSwap;
  docw["battery_charge_limit_register"] = modbusBatteryConfig.chargeLimit.registerAddress;
  docw["battery_charge_limit_type"] = modbusBatteryConfig.chargeLimit.valueType;
  docw["battery_charge_limit_scale"] = modbusBatteryConfig.chargeLimit.scale;
  docw["battery_charge_limit_word_swap"] = modbusBatteryConfig.chargeLimit.wordSwap;
  docw["battery_discharge_limit_register"] = modbusBatteryConfig.dischargeLimit.registerAddress;
  docw["battery_discharge_limit_type"] = modbusBatteryConfig.dischargeLimit.valueType;
  docw["battery_discharge_limit_scale"] = modbusBatteryConfig.dischargeLimit.scale;
  docw["battery_discharge_limit_word_swap"] = modbusBatteryConfig.dischargeLimit.wordSwap;
  docw["battery_state_idle_code"] = modbusBatteryConfig.idleStateCode;
  docw["battery_state_charging_code"] = modbusBatteryConfig.chargingStateCode;
  docw["battery_state_discharging_code"] = modbusBatteryConfig.dischargingStateCode;
  docw["pv_driver"] = pvConnectorDriver;
  docw["pv_wp"] = pvWattPeak;
  docw["pv_sunspec_ip"] = sunSpecPvConfig.ip;
  docw["pv_sunspec_port"] = sunSpecPvConfig.port;
  docw["pv_sunspec_unit_id"] = sunSpecPvConfig.id;
  docw["pv_sunspec_poll_seconds"] = sunSpecPvConfig.pollIntervalSeconds;
  docw["pv_modbus_power_register"] = sunSpecPvConfig.activePower.registerAddress;
  docw["pv_modbus_power_type"] = sunSpecPvConfig.activePower.valueType;
  docw["pv_modbus_power_scale"] = sunSpecPvConfig.activePower.scale;
  docw["pv_modbus_power_word_swap"] = sunSpecPvConfig.activePower.wordSwap;
  docw["pv_modbus_power_sf_register"] = sunSpecPvConfig.activePowerScaleRegister;
  docw["pv_modbus_energy_register"] = sunSpecPvConfig.dailyEnergy.registerAddress;
  docw["pv_modbus_energy_type"] = sunSpecPvConfig.dailyEnergy.valueType;
  docw["pv_modbus_energy_scale"] = sunSpecPvConfig.dailyEnergy.scale;
  docw["pv_modbus_energy_word_swap"] = sunSpecPvConfig.dailyEnergy.wordSwap;
  docw["pv_modbus_energy_sf_register"] = sunSpecPvConfig.dailyEnergyScaleRegister;
  docw["pv_modbus_use_register_scale_factors"] = sunSpecPvConfig.useRegisterScaleFactors;
  docw["pv_enphase_url"] = enphasePvConfig.url;
  docw["pv_enphase_token"] = enphasePvConfig.token;
  docw["pv_enphase_poll_seconds"] = enphasePvConfig.pollIntervalSeconds;
  docw["pv_sma_url"] = smaPvConfig.url;
  docw["pv_sma_token"] = smaPvConfig.token;
  docw["pv_sma_poll_seconds"] = smaPvConfig.pollIntervalSeconds;
  docw["pv_omniksol_url"] = omniksolPvConfig.url;
  docw["pv_omniksol_token"] = omniksolPvConfig.token;
  docw["pv_omniksol_poll_seconds"] = omniksolPvConfig.pollIntervalSeconds;
  docw["mqtt-hide"] = hideMQTTsettings;
  docw["remove-index"] = RemoveIndexAfterUpdate;
  docw["macid-topic"] = MacIDinToptopic;
  docw["ha_unique_ids"] = HAUniqueIds;
  docw["skip-network"] = skipNetwork;
  docw["try_calc_i"] = try_calc_i;
  docw["mimic"] = mimicsEnabled() ? mimicType : (uint8_t)MIMIC_NONE;


  docw["eid-enabled"] = bEID_enabled;
  if ( bEID_enabled ) EID_RESTART_IDLE_TIMER();

  #ifdef UDP_BCAST
  docw["udp"] = bUDPenabled;
  #endif
  docw["nrgm-enabled"] = bNRGMenabled;
  #ifdef NETSWITCH
  docw["netsw-enabled"] = bNETSWenabled;
  #endif

#ifdef VIRTUAL_P1
  if ( strlen(virtual_p1_ip) ) docw["virtual_p1_ip"] = virtual_p1_ip;
#endif  

  writeToJsonFile(docw, SettingsFile);
  
} // writeSettingsDirect()

//=======================================================================
void writeSettings() {
  if (WorkerEnqueueSimple(WORKER_JOB_SETTINGS_WRITE, WORKER_PRIO_NORMAL)) return;
  writeSettingsDirect();
} // writeSettings()

//=======================================================================
void readSettings(bool show) 
{
  File SettingsFile;
  if (!FSmounted) return;

  DebugTf(" %s ..\r\n", SETTINGS_FILE);
 
   if (!LittleFS.exists(SETTINGS_FILE)) 
  {
    DebugTln(F(" .. DSMRsettings.json file not found! --> created file!"));
    writeSettings();
    return;
  }
  
  for (int T = 0; T < 2; T++) 
  {
    SettingsFile = LittleFS.open(SETTINGS_FILE, "r");
    if (!SettingsFile) 
    {
      if (T == 0) DebugTf(" .. something went wrong opening [%s]\r\n", SETTINGS_FILE);
      else        DebugT(T);
      delay(500);
    }
  } // try T times ..
  
  DebugTln(F("Reading settings:.."));
  
  JsonDocument doc; 
  DeserializationError error = deserializeJson(doc, SettingsFile);
  if (error) {
    Debugln();
    LogFile("read():Failed to read DSMRsettings.json file",true);
    SettingsFile.close();
    writeSettings();
    return;
  }
  bool settingsBackfillNeeded = false;
  
  //strcpy(LittleFSTimestamp, doc["Timestamp"]);
  strlcpy(settingHostname, doc["Hostname"] | activeDefaultHostname, sizeof(settingHostname));
  strlcpy(settingIndexPage, doc["IndexPage"] | _DEFAULT_HOMEPAGE, sizeof(settingIndexPage));
  settingEDT1 = doc["EnergyDeliveredT1"];
  settingEDT2 = doc["EnergyDeliveredT2"];
  settingERT1 = doc["EnergyReturnedT1"];
  settingERT2 = doc["EnergyReturnedT2"];
  settingGDT = doc["GASDeliveredT"];
  if (doc["WaterDelivered"].is<float>()) settingWDT = doc["WaterDelivered"];
  settingENBK = doc["EnergyVasteKosten"];
  settingGNBK = doc["GasVasteKosten"];
  if (doc["WaterVasteKosten"].is<float>()) settingWNBK = doc["WaterVasteKosten"];
  if (doc["OverVoltageThreshold"].is<int>()) {
    settingOvervoltageThreshold = constrain(doc["OverVoltageThreshold"].as<int>(), 200, 300);
  }
  if (doc["CTFactor"].is<int>()) {
    settingCTFactor = constrain(doc["CTFactor"].as<int>(), (int)METER_FACTOR_MIN, (int)METER_FACTOR_MAX);
  }
  if (doc["VTFactor"].is<int>()) {
    settingVTFactor = constrain(doc["VTFactor"].as<int>(), (int)METER_FACTOR_MIN, (int)METER_FACTOR_MAX);
  }
  // HTTP POST credentials deliberately do not live in DSMRsettings.json.
  // A legacy MEENT installation is recognised solely by its saved WebID and
  // migrated once to the private NVS connector store.
  if (doc["MeentWebId"].is<const char*>()) {
    const char* legacyApiKey = doc["MeentApiKey"].is<const char*>() ? doc["MeentApiKey"].as<const char*>() : "";
    const uint16_t legacyInterval = doc["MeentInterval"].is<int>()
        ? constrain(doc["MeentInterval"].as<int>(), 1, 3600) : 300;
    settingsBackfillNeeded = ImportLegacyMeentConfiguration(doc["MeentWebId"].as<const char*>(), legacyApiKey, legacyInterval)
        || settingsBackfillNeeded;
  }
  // settingSmHasFaseInfo = doc["SmHasFaseInfo"];
  
  if (doc["mqtt-hide"].is<bool>()) hideMQTTsettings = doc["mqtt-hide"];
  if (doc["remove-index"].is<bool>()) RemoveIndexAfterUpdate = doc["remove-index"];
  if (doc["macid-topic"].is<bool>()) MacIDinToptopic = doc["macid-topic"];
  if (doc["ha_unique_ids"].is<bool>()) HAUniqueIds = doc["ha_unique_ids"];
  
//  settingTelegramInterval = doc["TelegramInterval"];
//  CHANGE_INTERVAL_SEC(nextTelegram, settingTelegramInterval);
// 
  //sprintf(settingMQTTbroker, "%s:%d", MQTTbroker, MQTTbrokerPort);
  strlcpy(settingMQTTbroker, doc["MQTTbroker"] | "", sizeof(settingMQTTbroker));
  settingMQTTbrokerPort = doc["MQTTbrokerPort"];
  strlcpy(settingMQTTuser, doc["MQTTUser"] | "", sizeof(settingMQTTuser));
  strlcpy(settingMQTTpasswd, doc["MQTTpasswd"] | "", sizeof(settingMQTTpasswd));
  settingMQTTinterval = doc["MQTTinterval"];
  if (doc["MQTTtopTopic"].is<const char*>()) strlcpy(settingMQTTtopTopic, doc["MQTTtopTopic"], sizeof(settingMQTTtopTopic));
  else if (!settingMQTTtopTopic[0]) snprintf(settingMQTTtopTopic, sizeof(settingMQTTtopTopic), "%s/", activeDefaultHostname);
  if (settingMQTTtopTopic[0] && settingMQTTtopTopic[strlen(settingMQTTtopTopic)-1] != '/') strlcat(settingMQTTtopTopic, "/", sizeof(settingMQTTtopTopic));
  CreateMacIDTopic();
  if (doc["mqtt-enabled"].is<bool>()) bMQTTenabled = doc["mqtt-enabled"];
  if (doc["mqtt_tls"].is<bool>()) bMQTToverTLS = doc["mqtt_tls"];
  
  CHANGE_INTERVAL_MS(publishMQTTtimer, 1000 * settingMQTTinterval - 100);
  LEDenabled = doc["LED"];
  if (doc["ota"].is<const char*>()) strlcpy(BaseOTAurl, doc["ota"].as<const char*>(), sizeof(BaseOTAurl));
  if (doc["enableHistory"].is<bool>()) EnableHistory = doc["enableHistory"];
  if (doc["watermeter"].is<bool>() ) WtrMtr = doc["watermeter"];
  if (doc["waterfactor"].is<float>()) WtrFactor = doc["waterfactor"];
  settingsBackfillNeeded = settingsBackfillNeeded || !doc["Fuse"].is<int>() || !doc["Phases"].is<int>() ||
                            !doc["CTFactor"].is<int>() || !doc["VTFactor"].is<int>();
  if (doc["Fuse"].is<int>()) {
    uint8_t newFuse = doc["Fuse"];
    settingFuse = (newFuse == 16 || newFuse == 25 || newFuse == 35) ? newFuse : 25;
  }
  if (doc["Phases"].is<int>()) settingPhases = constrain(doc["Phases"].as<int>(), 0, 3);

  if (doc["HAdiscovery"].is<bool>()) EnableHAdiscovery = doc["HAdiscovery"];
  if (doc["auto-update"].is<bool>()) bAutoUpdate = doc["auto-update"];
  if (doc["pre40"].is<bool>()) bPre40 = doc["pre40"];
  if (doc["raw-port"].is<bool>()) bRawPort = doc["raw-port"];
  if (doc["led-prt"].is<bool>()) bLED_PRT = doc["led-prt"];
  if (doc["try_calc_i"].is<bool>()) try_calc_i = doc["try_calc_i"];

  if (doc["eid-enabled"].is<bool>()) bEID_enabled = doc["eid-enabled"];
#ifdef VIRTUAL_P1
  if (doc["virtual_p1_ip"].is<const char*>()) strlcpy(virtual_p1_ip, doc["virtual_p1_ip"].as<const char*>(), sizeof(virtual_p1_ip));  
#endif  

  if (doc["act-json-mqtt"].is<bool>()) bActJsonMQTT = doc["act-json-mqtt"];

  const char* temp = doc["basic-auth"]["user"];
  if (temp) strlcpy(bAuthUser, temp, sizeof(bAuthUser));
  
  temp = doc["basic-auth"]["pass"];
  if (temp) strlcpy(bAuthPW, temp, sizeof(bAuthPW));
  if (doc["mb_map"].is<int>()) setModbusMapping(doc["mb_map"]);
  if (doc["mb_id"].is<int>()) mb_config.id = doc["mb_id"];
  if (doc["mb_port"].is<int>()) mb_config.port = doc["mb_port"];
  if (doc["mb_baud"].is<int>()) mb_config.baud = doc["mb_baud"];
  if (doc["mb_parity"].is<int>()) mb_config.parity = 134217700 + doc["mb_parity"].as<int>();
  if (doc["mb_monitor"].is<bool>()) bModbusMonitor = doc["mb_monitor"];
  if (doc["battery_driver"].is<int>()) batteryConnectorDriver = (BatteryConnectorDriver)constrain(doc["battery_driver"].as<int>(), BATTERY_DRIVER_NONE, BATTERY_DRIVER_SOLAREDGE_HTTP);
  // Version 5.10 migrates the old Victron-only keys into the generic battery
  // connector.  New keys take precedence when both are present.
  if (doc["victron_accu_enabled"].is<bool>()) modbusBatteryConfig.enabled = doc["victron_accu_enabled"];
  if (doc["victron_accu_ip"].is<const char*>()) strlcpy(modbusBatteryConfig.ip, doc["victron_accu_ip"].as<const char*>(), sizeof(modbusBatteryConfig.ip));
  if (doc["victron_accu_id"].is<int>()) modbusBatteryConfig.id = constrain(doc["victron_accu_id"].as<int>(), 1, 247);
  if (doc["battery_modbus_enabled"].is<bool>()) modbusBatteryConfig.enabled = doc["battery_modbus_enabled"];
  if (!doc["battery_driver"].is<int>() && modbusBatteryConfig.enabled) batteryConnectorDriver = BATTERY_DRIVER_MODBUS_TCP;
  if (doc["battery_modbus_ip"].is<const char*>()) strlcpy(modbusBatteryConfig.ip, doc["battery_modbus_ip"].as<const char*>(), sizeof(modbusBatteryConfig.ip));
  if (doc["battery_modbus_port"].is<int>()) modbusBatteryConfig.port = constrain(doc["battery_modbus_port"].as<int>(), 1, 65535);
  if (doc["battery_modbus_unit_id"].is<int>()) modbusBatteryConfig.id = constrain(doc["battery_modbus_unit_id"].as<int>(), 1, 247);
  if (doc["battery_modbus_poll_seconds"].is<int>()) modbusBatteryConfig.pollIntervalSeconds = constrain(doc["battery_modbus_poll_seconds"].as<int>(), 1, 3600);
  if (doc["battery_solaredge_site_id"].is<uint32_t>()) solarEdgeBatteryConfig.siteId = doc["battery_solaredge_site_id"];
  if (doc["battery_solaredge_api_key"].is<const char*>()) strlcpy(solarEdgeBatteryConfig.apiKey, doc["battery_solaredge_api_key"].as<const char*>(), sizeof(solarEdgeBatteryConfig.apiKey));
  if (doc["battery_solaredge_poll_seconds"].is<int>()) solarEdgeBatteryConfig.pollIntervalSeconds = constrain(doc["battery_solaredge_poll_seconds"].as<int>(), 300, 3600);
#define LOAD_BATTERY_FIELD(prefix, target) \
  if (doc[prefix "_register"].is<int>()) target.registerAddress = constrain(doc[prefix "_register"].as<int>(), 0, 65535); \
  if (doc[prefix "_type"].is<int>()) target.valueType = constrain(doc[prefix "_type"].as<int>(), MODBUS_BATTERY_U16, MODBUS_BATTERY_F32); \
  if (doc[prefix "_scale"].is<float>()) target.scale = doc[prefix "_scale"].as<float>(); \
  if (doc[prefix "_word_swap"].is<bool>()) target.wordSwap = doc[prefix "_word_swap"];
  LOAD_BATTERY_FIELD("battery_power", modbusBatteryConfig.activePower)
  LOAD_BATTERY_FIELD("battery_soc", modbusBatteryConfig.stateOfCharge)
  LOAD_BATTERY_FIELD("battery_state", modbusBatteryConfig.operatingState)
  LOAD_BATTERY_FIELD("battery_available_capacity", modbusBatteryConfig.availableCapacity)
  LOAD_BATTERY_FIELD("battery_charge_limit", modbusBatteryConfig.chargeLimit)
  LOAD_BATTERY_FIELD("battery_discharge_limit", modbusBatteryConfig.dischargeLimit)
#undef LOAD_BATTERY_FIELD
  if (doc["battery_state_idle_code"].is<int>()) modbusBatteryConfig.idleStateCode = doc["battery_state_idle_code"];
  if (doc["battery_state_charging_code"].is<int>()) modbusBatteryConfig.chargingStateCode = doc["battery_state_charging_code"];
  if (doc["battery_state_discharging_code"].is<int>()) modbusBatteryConfig.dischargingStateCode = doc["battery_state_discharging_code"];
  if (doc["pv_driver"].is<int>()) pvConnectorDriver = (PvConnectorDriver)constrain(doc["pv_driver"].as<int>(), PV_DRIVER_NONE, PV_DRIVER_OMNIKSOL_HTTP);
  if (doc["pv_wp"].is<uint32_t>()) pvWattPeak = doc["pv_wp"];
  if (doc["pv_solaredge_site_id"].is<uint32_t>()) solarEdgePvConfig.siteId = doc["pv_solaredge_site_id"];
  if (doc["pv_solaredge_api_key"].is<const char*>()) strlcpy(solarEdgePvConfig.apiKey, doc["pv_solaredge_api_key"].as<const char*>(), sizeof(solarEdgePvConfig.apiKey));
  if (doc["pv_solaredge_poll_seconds"].is<int>()) solarEdgePvConfig.pollIntervalSeconds = constrain(doc["pv_solaredge_poll_seconds"].as<int>(), 300, 3600);
  // PV connector releases before the shared connection stored these values
  // under pv_solaredge_*. Adopt them only when no shared credentials exist.
  if (!solarEdgeBatteryConfig.siteId && solarEdgePvConfig.siteId) {
    solarEdgeBatteryConfig.siteId = solarEdgePvConfig.siteId;
    strlcpy(solarEdgeBatteryConfig.apiKey, solarEdgePvConfig.apiKey, sizeof(solarEdgeBatteryConfig.apiKey));
    solarEdgeBatteryConfig.pollIntervalSeconds = solarEdgePvConfig.pollIntervalSeconds;
  }
  if (doc["pv_solaredge_wp"].is<uint32_t>()) solarEdgePvConfig.wattPeak = doc["pv_solaredge_wp"];
  if (doc["pv_sunspec_ip"].is<const char*>()) strlcpy(sunSpecPvConfig.ip, doc["pv_sunspec_ip"].as<const char*>(), sizeof(sunSpecPvConfig.ip));
  if (doc["pv_sunspec_port"].is<int>()) sunSpecPvConfig.port = constrain(doc["pv_sunspec_port"].as<int>(), 1, 65535);
  if (doc["pv_sunspec_unit_id"].is<int>()) sunSpecPvConfig.id = constrain(doc["pv_sunspec_unit_id"].as<int>(), 1, 247);
  if (doc["pv_sunspec_poll_seconds"].is<int>()) sunSpecPvConfig.pollIntervalSeconds = constrain(doc["pv_sunspec_poll_seconds"].as<int>(), 1, 3600);
  if (doc["pv_sunspec_wp"].is<uint32_t>()) sunSpecPvConfig.wattPeak = doc["pv_sunspec_wp"];
  if (doc["pv_modbus_power_register"].is<int>()) sunSpecPvConfig.activePower.registerAddress = constrain(doc["pv_modbus_power_register"].as<int>(), 0, 65535);
  if (doc["pv_modbus_power_type"].is<int>()) sunSpecPvConfig.activePower.valueType = constrain(doc["pv_modbus_power_type"].as<int>(), MODBUS_BATTERY_U16, MODBUS_BATTERY_F32);
  if (doc["pv_modbus_power_scale"].is<float>()) sunSpecPvConfig.activePower.scale = doc["pv_modbus_power_scale"];
  if (doc["pv_modbus_power_word_swap"].is<bool>()) sunSpecPvConfig.activePower.wordSwap = doc["pv_modbus_power_word_swap"];
  if (doc["pv_modbus_power_sf_register"].is<int>()) sunSpecPvConfig.activePowerScaleRegister = constrain(doc["pv_modbus_power_sf_register"].as<int>(), 0, 65535);
  if (doc["pv_modbus_energy_register"].is<int>()) sunSpecPvConfig.dailyEnergy.registerAddress = constrain(doc["pv_modbus_energy_register"].as<int>(), 0, 65535);
  if (doc["pv_modbus_energy_type"].is<int>()) sunSpecPvConfig.dailyEnergy.valueType = constrain(doc["pv_modbus_energy_type"].as<int>(), MODBUS_BATTERY_U16, MODBUS_BATTERY_F32);
  if (doc["pv_modbus_energy_scale"].is<float>()) sunSpecPvConfig.dailyEnergy.scale = doc["pv_modbus_energy_scale"];
  if (doc["pv_modbus_energy_word_swap"].is<bool>()) sunSpecPvConfig.dailyEnergy.wordSwap = doc["pv_modbus_energy_word_swap"];
  if (doc["pv_modbus_energy_sf_register"].is<int>()) sunSpecPvConfig.dailyEnergyScaleRegister = constrain(doc["pv_modbus_energy_sf_register"].as<int>(), 0, 65535);
  if (doc["pv_modbus_use_register_scale_factors"].is<bool>()) sunSpecPvConfig.useRegisterScaleFactors = doc["pv_modbus_use_register_scale_factors"];
#define LOAD_PV_HTTP(prefix, target) \
  if (doc[prefix "_url"].is<const char*>()) strlcpy(target.url, doc[prefix "_url"].as<const char*>(), sizeof(target.url)); \
  if (doc[prefix "_token"].is<const char*>()) strlcpy(target.token, doc[prefix "_token"].as<const char*>(), sizeof(target.token)); \
  if (doc[prefix "_poll_seconds"].is<int>()) target.pollIntervalSeconds = constrain(doc[prefix "_poll_seconds"].as<int>(), 5, 3600); \
  if (doc[prefix "_wp"].is<uint32_t>()) target.wattPeak = doc[prefix "_wp"];
  LOAD_PV_HTTP("pv_enphase", enphasePvConfig)
  LOAD_PV_HTTP("pv_sma", smaPvConfig)
  LOAD_PV_HTTP("pv_omniksol", omniksolPvConfig)
#undef LOAD_PV_HTTP
  if (doc["skip-network"].is<bool>()) skipNetwork = doc["skip-network"];
  if (doc["mimic"].is<int>()) {
    int newMimic = constrain(doc["mimic"].as<int>(), (int)MIMIC_NONE, (int)MIMIC_SHELLY_PRO_3EM);
    mimicType = mimicsEnabled() ? newMimic : MIMIC_NONE;
  } else {
    mimicType = MIMIC_NONE;
  }

  #ifdef UDP_BCAST
  if (doc["udp"].is<bool>()) bUDPenabled = doc["udp"];
  #endif
  if (doc["nrgm-enabled"].is<bool>()) bNRGMenabled = doc["nrgm-enabled"];
  else {
    // legacy migration from <=5.2.9
    bNRGMenabled = (Pref.peers > 0);
    writeSettings();
  }

  #ifdef NETSWITCH
  if (doc["netsw-enabled"].is<bool>()) bNETSWenabled = doc["netsw-enabled"];
  #endif

  SettingsFile.close();
  //end json
  if (settingsBackfillNeeded) writeSettingsDirect();

    mdns_hostname_set(settingHostname);
    mdns_instance_name_set(activeDefaultHostname);

//  Debug(F(".. done\r"));


  if (strlen(settingIndexPage) < 7) strCopy(settingIndexPage, (sizeof(settingIndexPage) -1), _DEFAULT_HOMEPAGE);
  
  if (settingMQTTbrokerPort    < 1) settingMQTTbrokerPort   = 1883;
  settingHttpPostInterval = constrain(settingHttpPostInterval, 1, 3600);

  if (!show) return;

} // readSettings()


//=======================================================================
void updateSetting(const char *field, const char *newValue)
{
  bool mqtt_reconnect = false;
  bool reboot_required = false;
  DebugTf("-> field[%s], newValue[%s]\r\n", field, newValue);

  if (!FSmounted) return;

  if (!stricmp(field, "Hostname")) {
    strCopy(settingHostname, 29, newValue); 
    if (strlen(settingHostname) < 1) strCopy(settingHostname, 29, activeDefaultHostname); 
    char *dotPntr = strchr(settingHostname, '.') ;
    if (dotPntr != NULL)
    {
      byte dotPos = (dotPntr-settingHostname);
      if (dotPos > 0)  settingHostname[dotPos] = '\0';
    }
//    DebugTf("Need reboot before new %s.local will be available!\r\n\n", settingHostname);
  }
  if (!stricmp(field, "ed_tariff1"))        settingEDT1         = String(newValue).toFloat();  
  if (!stricmp(field, "ed_tariff2"))        settingEDT2         = String(newValue).toFloat();  
  if (!stricmp(field, "er_tariff1"))        settingERT1         = String(newValue).toFloat();  
  if (!stricmp(field, "er_tariff2"))        settingERT2         = String(newValue).toFloat();  
  if (!stricmp(field, "electr_netw_costs")) settingENBK         = String(newValue).toFloat();

  if (!stricmp(field, "gd_tariff"))         settingGDT          = String(newValue).toFloat();  
  if (!stricmp(field, "gas_netw_costs"))    settingGNBK         = String(newValue).toFloat();
  if (!stricmp(field, "overvoltage_threshold")) {
    uint16_t newThreshold = constrain(String(newValue).toInt(), 200, 300);
    if (settingOvervoltageThreshold != newThreshold) {
      settingOvervoltageThreshold = newThreshold;
      ResetOvervoltageStats();
    }
  }
  if (!stricmp(field, "ct_factor")) {
    settingCTFactor = constrain(String(newValue).toInt(), (int)METER_FACTOR_MIN, (int)METER_FACTOR_MAX);
  }
  if (!stricmp(field, "vt_factor")) {
    settingVTFactor = constrain(String(newValue).toInt(), (int)METER_FACTOR_MIN, (int)METER_FACTOR_MAX);
  }
  if (!stricmp(field, "fuse")) {
    uint8_t newFuse = String(newValue).toInt();
    settingFuse = (newFuse == 16 || newFuse == 25 || newFuse == 35) ? newFuse : 25;
  }
  if (!stricmp(field, "phases")) {
    settingPhases = constrain(String(newValue).toInt(), 0, 3);
  }
#ifdef VIRTUAL_P1
  if (!stricmp(field, "virtual_p1_ip")) {
    bool changed = strncmp(virtual_p1_ip, newValue, sizeof(virtual_p1_ip)) != 0;
    strCopy(virtual_p1_ip, sizeof(virtual_p1_ip), newValue);
    if (changed) virtualP1ConfigChanged();
  }
#endif

  if (!stricmp(field, "w_tariff"))          settingWDT          = String(newValue).toFloat();  
  if (!stricmp(field, "water_netw_costs"))  settingWNBK         = String(newValue).toFloat(); 

  if (!stricmp(field, "water_m3")){
    P1Status.wtr_m3         = String(newValue).toInt();
    CHANGE_INTERVAL_MS(StatusTimer, 100);
  }
  if (!stricmp(field, "water_l")) {
    P1Status.wtr_l         = String(newValue).toInt();
    CHANGE_INTERVAL_MS(StatusTimer, 100);
  }

  // if (!stricmp(field, "sm_has_fase_info")) 
  // {
  //   settingSmHasFaseInfo = String(newValue).toInt(); 
  //   if (settingSmHasFaseInfo != 0)  settingSmHasFaseInfo = 1;
  //   else                            settingSmHasFaseInfo = 0;  
  // }

  if (!stricmp(field, "IndexPage"))        strCopy(settingIndexPage, (sizeof(settingIndexPage) -1), newValue);  

#ifndef MQTT_DISABLE 
  if (!stricmp(field, "mqtt_enabled") || !stricmp(field, "mqtt-enabled")) {
    bool newMQTTenabled = (stricmp(newValue, "true") == 0 ? true : false);
    if (bMQTTenabled != newMQTTenabled) {
      bMQTTenabled = newMQTTenabled;
      if (!bMQTTenabled) {
        bSendMQTT = false;
        MQTTDisconnect();
      } else {
        MQTTsetServer();
      }
    }
  }

  if (!stricmp(field, "mqtt_broker"))  {
    DebugT("settingMQTTbroker! to : ");
    memset(settingMQTTbroker, '\0', sizeof(settingMQTTbroker));
    strCopy(settingMQTTbroker, 100, newValue);
    Debugf("[%s]\r\n", settingMQTTbroker);
    mqtt_reconnect = true;
  }

  if (!stricmp(field, "mqtt_broker_port")) {
    settingMQTTbrokerPort = String(newValue).toInt();  
    mqtt_reconnect = true;
  }
  if (!stricmp(field, "mqtt_user")) {
    strCopy(settingMQTTuser    ,35, newValue);  
    mqtt_reconnect = true;
  }
  if (!stricmp(field, "mqtt_passwd")) {
    strCopy(settingMQTTpasswd  ,sizeof(settingMQTTpasswd), newValue);  
    mqtt_reconnect = true;
  }
  
  if (!stricmp(field, "mqtt_tls")) {
    bMQTToverTLS = (stricmp(newValue, "true") == 0?true:false); 
    mqtt_reconnect = true;
  }

  if ( mqtt_reconnect ) MQTTsetServer();
  
  if (!stricmp(field, "mqtt_interval")) {
    settingMQTTinterval   = String(newValue).toInt();  
    CHANGE_INTERVAL_MS(publishMQTTtimer, 1000 * settingMQTTinterval - 100);
    // if ( settingMQTTinterval == 0 )  MQTTDisconnect();
  }
  if (!stricmp(field, "mqtt_toptopic")) {
    strCopy(settingMQTTtopTopic, sizeof(settingMQTTtopTopic), newValue);  
  }
  if (settingMQTTtopTopic[0] && settingMQTTtopTopic[strlen(settingMQTTtopTopic)-1] != '/') strlcat(settingMQTTtopTopic, "/", sizeof(settingMQTTtopTopic));
  CreateMacIDTopic();
#endif
  
  if (!stricmp(field, "b_auth_user")) strCopy(bAuthUser,25, newValue);  
  if (!stricmp(field, "b_auth_pw")) strCopy(bAuthPW,25, newValue); 
  if (!stricmp(field, "water_fact")) WtrFactor = String(newValue).toFloat(); 
  
  if (!stricmp(field, "ota_url")) {
    const char* cleanUrl = newValue;
    if (strncasecmp(cleanUrl, "http://", 7) == 0) cleanUrl += 7;
    else if (strncasecmp(cleanUrl, "https://", 8) == 0) cleanUrl += 8;

    char ota_url[sizeof(BaseOTAurl)];
    snprintf(ota_url, sizeof(ota_url), "http://%s", cleanUrl);
    strlcpy(BaseOTAurl, ota_url, sizeof(BaseOTAurl));
  }
  
  //booleans
  if (!stricmp(field, "led")) LEDenabled = (stricmp(newValue, "true") == 0?true:false); 
  if (!stricmp(field, "hist")) EnableHistory = (stricmp(newValue, "true") == 0?true:false); 
  if (!stricmp(field, "auto_update") || !stricmp(field, "auto-update")) bAutoUpdate = (stricmp(newValue, "true") == 0?true:false);
  if (!stricmp(field, "water_enabl")) WtrMtr = (stricmp(newValue, "true") == 0?true:false);  
  if (!stricmp(field, "ha_disc_enabl")) EnableHAdiscovery = (stricmp(newValue, "true") == 0?true:false);  
  if (!stricmp(field, "ha_unique_ids")) {
    HAUniqueIds = (stricmp(newValue, "true") == 0?true:false);
#ifndef MQTT_DISABLE
    if (bMQTTenabled && EnableHAdiscovery && MQTTclient.connected()) AutoDiscoverHA();
#endif
  }
  if (!stricmp(field, "pre40")) {
    bPre40 = (stricmp(newValue, "true") == 0?true:false);    
    SetupP1In();
  }
  if (!stricmp(field, "raw-port")) bRawPort = (stricmp(newValue, "true") == 0?true:false);  
  if (!stricmp(field, "try_calc_i")) try_calc_i = (stricmp(newValue, "true") == 0?true:false);
  if (!stricmp(field, "act-json-mqtt")) bActJsonMQTT = (stricmp(newValue, "true") == 0?true:false);  
  if (!stricmp(field, "eid-enabled")) bEID_enabled = (stricmp(newValue, "true") == 0?true:false);  
  
  #ifdef UDP_BCAST
  if (!stricmp(field, "udp")) bUDPenabled = (stricmp(newValue, "true") == 0?true:false);  
  #endif
  if (!stricmp(field, "nrgm-enabled")) {
    bool newNrgmEnabled = (stricmp(newValue, "true") == 0?true:false);
    if ( bNRGMenabled != newNrgmEnabled ) {
      bNRGMenabled = newNrgmEnabled;
      SyncESPNOW();
    }
  }
  #ifdef NETSWITCH
  if (!stricmp(field, "netsw-enabled")) bNETSWenabled = (stricmp(newValue, "true") == 0?true:false);
  #endif

  if (!stricmp(field, "mb_map")) setModbusMapping(String(newValue).toInt());  
  if (!stricmp(field, "mb_id")) {
    uint8_t oldModbusId = mb_config.id;
    uint8_t newModbusId = constrain(String(newValue).toInt(), 1, 255);
    if (oldModbusId != newModbusId) {
      mb_config.id = newModbusId;
      updateModbusServerId(oldModbusId, newModbusId);
    }
  }
  if (!stricmp(field, "mb_port")) mb_config.port = String(newValue).toInt();  
  if (!stricmp(field, "mb_baud")) mb_config.baud = String(newValue).toInt();  
  if (!stricmp(field, "mb_parity")) mb_config.parity = String(newValue).toInt();  
  if (!stricmp(field, "mb_monitor")) bModbusMonitor = (stricmp(newValue, "true") == 0 ? true : false);
  bool batteryConfigChanged = false;
  if (!stricmp(field, "battery_driver")) {
    batteryConnectorDriver = (BatteryConnectorDriver)constrain(String(newValue).toInt(), BATTERY_DRIVER_NONE, BATTERY_DRIVER_SOLAREDGE_HTTP);
    modbusBatteryConfig.enabled = batteryConnectorDriver == BATTERY_DRIVER_MODBUS_TCP;
    batteryConfigChanged = true;
  }
  if (!stricmp(field, "battery_modbus_enabled") || !stricmp(field, "victron_accu_enabled")) {
    modbusBatteryConfig.enabled = (stricmp(newValue, "true") == 0);
    batteryConfigChanged = true;
  }
  if (!stricmp(field, "battery_modbus_ip") || !stricmp(field, "victron_accu_ip")) {
    strCopy(modbusBatteryConfig.ip, sizeof(modbusBatteryConfig.ip), newValue);
    batteryConfigChanged = true;
  }
  if (!stricmp(field, "battery_modbus_port")) { modbusBatteryConfig.port = constrain(String(newValue).toInt(), 1, 65535); batteryConfigChanged = true; }
  if (!stricmp(field, "battery_modbus_poll_seconds")) { modbusBatteryConfig.pollIntervalSeconds = constrain(String(newValue).toInt(), 1, 3600); batteryConfigChanged = true; }
  if (!stricmp(field, "battery_solaredge_site_id")) { solarEdgeBatteryConfig.siteId = String(newValue).toInt(); batteryConfigChanged = true; }
  if (!stricmp(field, "battery_solaredge_api_key") && strlen(newValue) && strcmp(newValue, "********")) { strCopy(solarEdgeBatteryConfig.apiKey, sizeof(solarEdgeBatteryConfig.apiKey), newValue); batteryConfigChanged = true; }
  if (!stricmp(field, "battery_solaredge_poll_seconds")) { solarEdgeBatteryConfig.pollIntervalSeconds = constrain(String(newValue).toInt(), 300, 3600); batteryConfigChanged = true; }
  #define SET_BATTERY_FIELD(prefix, target) \
    if (!stricmp(field, prefix "_register")) { target.registerAddress = constrain(String(newValue).toInt(), 0, 65535); batteryConfigChanged = true; } \
    if (!stricmp(field, prefix "_type")) { target.valueType = constrain(String(newValue).toInt(), MODBUS_BATTERY_U16, MODBUS_BATTERY_F32); batteryConfigChanged = true; } \
    if (!stricmp(field, prefix "_scale")) { target.scale = String(newValue).toFloat(); batteryConfigChanged = true; } \
    if (!stricmp(field, prefix "_word_swap")) { target.wordSwap = !stricmp(newValue, "true"); batteryConfigChanged = true; }
  SET_BATTERY_FIELD("battery_power", modbusBatteryConfig.activePower)
  SET_BATTERY_FIELD("battery_soc", modbusBatteryConfig.stateOfCharge)
  SET_BATTERY_FIELD("battery_state", modbusBatteryConfig.operatingState)
  SET_BATTERY_FIELD("battery_available_capacity", modbusBatteryConfig.availableCapacity)
  SET_BATTERY_FIELD("battery_charge_limit", modbusBatteryConfig.chargeLimit)
  SET_BATTERY_FIELD("battery_discharge_limit", modbusBatteryConfig.dischargeLimit)
  #undef SET_BATTERY_FIELD
  if (!stricmp(field, "battery_state_idle_code")) { modbusBatteryConfig.idleStateCode = String(newValue).toInt(); batteryConfigChanged = true; }
  if (!stricmp(field, "battery_state_charging_code")) { modbusBatteryConfig.chargingStateCode = String(newValue).toInt(); batteryConfigChanged = true; }
  if (!stricmp(field, "battery_state_discharging_code")) { modbusBatteryConfig.dischargingStateCode = String(newValue).toInt(); batteryConfigChanged = true; }
  if (!stricmp(field, "battery_modbus_unit_id") || !stricmp(field, "victron_accu_id")) {
    modbusBatteryConfig.id = constrain(String(newValue).toInt(), 1, 247);
    batteryConfigChanged = true;
  }
#ifdef MBUS
  if (batteryConfigChanged) modbusBatteryConfigChanged();
#endif
  if (batteryConfigChanged) solarEdgeBatteryConfigChanged();
  bool pvConfigChanged = false;
  if (!stricmp(field, "pv_driver")) {
    pvConnectorDriver = (PvConnectorDriver)constrain(String(newValue).toInt(), PV_DRIVER_NONE, PV_DRIVER_OMNIKSOL_HTTP);
    pvConfigChanged = true;
  }
  if (!stricmp(field, "pv_wp")) { pvWattPeak = constrain(String(newValue).toInt(), 0, 1000000); pvConfigChanged = true; }
  if (!stricmp(field, "pv_solaredge_site_id")) { solarEdgeBatteryConfig.siteId = String(newValue).toInt(); pvConfigChanged = true; }
  if (!stricmp(field, "pv_solaredge_api_key") && strlen(newValue) && strcmp(newValue, "********")) { strCopy(solarEdgeBatteryConfig.apiKey, sizeof(solarEdgeBatteryConfig.apiKey), newValue); pvConfigChanged = true; }
  if (!stricmp(field, "pv_solaredge_poll_seconds")) { solarEdgeBatteryConfig.pollIntervalSeconds = constrain(String(newValue).toInt(), 300, 3600); pvConfigChanged = true; }
  if (!stricmp(field, "pv_solaredge_wp")) { solarEdgePvConfig.wattPeak = constrain(String(newValue).toInt(), 0, 1000000); pvConfigChanged = true; }
  if (!stricmp(field, "pv_sunspec_ip")) { strCopy(sunSpecPvConfig.ip, sizeof(sunSpecPvConfig.ip), newValue); pvConfigChanged = true; }
  if (!stricmp(field, "pv_sunspec_port")) { sunSpecPvConfig.port = constrain(String(newValue).toInt(), 1, 65535); pvConfigChanged = true; }
  if (!stricmp(field, "pv_sunspec_unit_id")) { sunSpecPvConfig.id = constrain(String(newValue).toInt(), 1, 247); pvConfigChanged = true; }
  if (!stricmp(field, "pv_sunspec_poll_seconds")) { sunSpecPvConfig.pollIntervalSeconds = constrain(String(newValue).toInt(), 1, 3600); pvConfigChanged = true; }
  if (!stricmp(field, "pv_sunspec_wp")) { sunSpecPvConfig.wattPeak = constrain(String(newValue).toInt(), 0, 1000000); pvConfigChanged = true; }
  if (!stricmp(field, "pv_modbus_power_register")) { sunSpecPvConfig.activePower.registerAddress = constrain(String(newValue).toInt(), 0, 65535); pvConfigChanged = true; }
  if (!stricmp(field, "pv_modbus_power_type")) { sunSpecPvConfig.activePower.valueType = constrain(String(newValue).toInt(), MODBUS_BATTERY_U16, MODBUS_BATTERY_F32); pvConfigChanged = true; }
  if (!stricmp(field, "pv_modbus_power_scale")) { sunSpecPvConfig.activePower.scale = String(newValue).toFloat(); pvConfigChanged = true; }
  if (!stricmp(field, "pv_modbus_power_word_swap")) { sunSpecPvConfig.activePower.wordSwap = !stricmp(newValue, "true"); pvConfigChanged = true; }
  if (!stricmp(field, "pv_modbus_power_sf_register")) { sunSpecPvConfig.activePowerScaleRegister = constrain(String(newValue).toInt(), 0, 65535); pvConfigChanged = true; }
  if (!stricmp(field, "pv_modbus_energy_register")) { sunSpecPvConfig.dailyEnergy.registerAddress = constrain(String(newValue).toInt(), 0, 65535); pvConfigChanged = true; }
  if (!stricmp(field, "pv_modbus_energy_type")) { sunSpecPvConfig.dailyEnergy.valueType = constrain(String(newValue).toInt(), MODBUS_BATTERY_U16, MODBUS_BATTERY_F32); pvConfigChanged = true; }
  if (!stricmp(field, "pv_modbus_energy_scale")) { sunSpecPvConfig.dailyEnergy.scale = String(newValue).toFloat(); pvConfigChanged = true; }
  if (!stricmp(field, "pv_modbus_energy_word_swap")) { sunSpecPvConfig.dailyEnergy.wordSwap = !stricmp(newValue, "true"); pvConfigChanged = true; }
  if (!stricmp(field, "pv_modbus_energy_sf_register")) { sunSpecPvConfig.dailyEnergyScaleRegister = constrain(String(newValue).toInt(), 0, 65535); pvConfigChanged = true; }
  if (!stricmp(field, "pv_modbus_use_register_scale_factors")) { sunSpecPvConfig.useRegisterScaleFactors = !stricmp(newValue, "true"); pvConfigChanged = true; }
#define SET_PV_HTTP(prefix, target) \
  if (!stricmp(field, prefix "_url")) { strCopy(target.url, sizeof(target.url), newValue); pvConfigChanged = true; } \
  if (!stricmp(field, prefix "_token") && strlen(newValue) && strcmp(newValue, "********")) { strCopy(target.token, sizeof(target.token), newValue); pvConfigChanged = true; } \
  if (!stricmp(field, prefix "_poll_seconds")) { target.pollIntervalSeconds = constrain(String(newValue).toInt(), 5, 3600); pvConfigChanged = true; } \
  if (!stricmp(field, prefix "_wp")) { target.wattPeak = constrain(String(newValue).toInt(), 0, 1000000); pvConfigChanged = true; }
  SET_PV_HTTP("pv_enphase", enphasePvConfig)
  SET_PV_HTTP("pv_sma", smaPvConfig)
  SET_PV_HTTP("pv_omniksol", omniksolPvConfig)
#undef SET_PV_HTTP
  if (pvConfigChanged) {
    pvConnectorConfigChanged();
#ifdef MBUS
    sunSpecPvConfigChanged();
#endif
  }
  if (!stricmp(field, "mimic")) {
    int newMimic = constrain(String(newValue).toInt(), (int)MIMIC_NONE, (int)MIMIC_SHELLY_PRO_3EM);
    reboot_required = (mimicType != newMimic);
    mimicType = mimicsEnabled() ? newMimic : MIMIC_NONE;
  }

  SendTariffData(); // P2PType = NRGTARIFS;
  if (reboot_required) {
    writeSettingsDirect();
    LogFile("reboot: mimic changed", true);
    delay(200);
    P1Reboot();
  } else {
    writeSettings();
  }
  
} // updateSetting()

//bugfix file size append sdk 3.3.1
size_t fileSizeOf(const char* path) {
  struct stat st;
  if (stat(path, &st) == 0) return (size_t)st.st_size;
  return 0; // not found or error
}

//=======================================================================
void LogFileWriteDirect(const char* payload, bool toDebug) {
  if (toDebug) DebugTln(payload);
#if DIRECT_AP_CONNECT && !DIRECT_AP_ENABLE_LOCAL_LOGS
  return;
#endif
  if (!FSmounted) return;
  size_t size = fileSizeOf("/littlefs/P1.log");

  //log rotate
  if (size > 12000){ 
    LittleFS.remove("/P1_old.log");     //remove .log if existing 
    LittleFS.remove("/P1_log.old");     //remove .old if existing 
    //rename file
    DebugTln(F("RebootLog: log rotation"));
    LittleFS.rename("/P1.log", "/P1_old.log");
  }

  //appending
  File LogFile = LittleFS.open("/P1.log", "a"); // open for appending  
  if (!LogFile) {
    DebugTln(F("open P1.log FAILED!!!--> Bailout\r\n"));
    LogFile.close(); 
    return;
  }
    
    String log_payload = "{\"up\":";
    log_payload += String(uptime());
    log_payload += ",\"time\":\"";
    log_payload += buildDateTimeString(actTimestamp, sizeof(actTimestamp));
    log_payload += "\",\"";
    
    if ( strlen(payload)==0 ) {
       log_payload += PROFILE " [" _VERSION_ONLY "] REBOOT reason: ";
       log_payload += String(lastReset);
       log_payload += " | reboots: ";
       log_payload += String(P1Status.reboots);
    } else log_payload += payload;

    log_payload += "\"}";

    LogFile.println(log_payload.c_str());
    //closing the file
    LogFile.close(); 
}

//=======================================================================
void LogFile(const char* payload, bool toDebug) {
  if (WorkerEnqueueLog(payload, toDebug)) return;
  LogFileWriteDirect(payload, toDebug);
}
/***************************************************************************
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
***************************************************************************/
