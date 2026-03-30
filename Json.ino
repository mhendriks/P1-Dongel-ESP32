/* 
***************************************************************************  
**  Program  : JsonCalls, part of DSMRloggerAPI
**
**  Copyright (c) 2023 Martijn Hendriks
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/
#define ACTUALELEMENTS  22
#define INFOELEMENTS     3
#define FIELDELEMENTS    1

byte fieldsElements = 0;
char Onefield[25];
bool onlyIfPresent = false;

const static PROGMEM char infoArray[][25]   = { "identification","p1_version","equipment_id" }; //waardes dient redelijk statisch zijn niet elke keer versturen
const static PROGMEM char actualArray[][25] = { "timestamp","electricity_tariff","energy_delivered_tariff1","energy_delivered_tariff2","energy_returned_tariff1","energy_returned_tariff2","power_delivered","power_returned","voltage_l1","voltage_l2","voltage_l3","current_l1","current_l2","current_l3","power_delivered_l1","power_delivered_l2","power_delivered_l3","power_returned_l1","power_returned_l2","power_returned_l3","peak_pwr_last_q", "highest_peak_pwr"};

JsonDocument jsonDoc;  // generic doc to return, clear() before use!

static String jsonResponse(std::function<void(JsonDocument& doc)> fill) {
  JsonDocument doc;
  fill(doc);
  String out;
  serializeJson(doc, out);
  return out;
}

static ApiResponse jsonOkResponse(const String& body) {
  return {200, "application/json", body};
}

static ApiResponse jsonNoContentResponse() {
  return {204, "application/json", ""};
}

static ApiResponse jsonNotFoundResponse() {
  JsonDocument doc;
  JsonObject error = doc["error"].to<JsonObject>();
  error["url"] = httpServer.uri();
  error["message"] = "url not valid";
  String body;
  serializeJson(doc, body);
  return {404, "application/json", body};
}

static ApiResponse jsonDocResponse(const JsonDocument& doc) {
  String body;
  serializeJson(doc, body);
  return jsonOkResponse(body);
}

String apiStatsJson() {
  return jsonResponse([&](JsonDocument& doc){
    
    if ( DSMRdata.current_l1_present ) doc["I1piek"]  = P1Stats.I1piek;
    if ( DSMRdata.current_l2_present ) doc["I2piek"]  = P1Stats.I2piek;
    if ( DSMRdata.current_l3_present ) doc["I3piek"]  = P1Stats.I3piek;
    
    if ( DSMRdata.power_delivered_l1_present ) doc["P1max"]   = P1Stats.P1max;
    if ( DSMRdata.power_delivered_l2_present ) doc["P2max"]   = P1Stats.P2max;
    if ( DSMRdata.power_delivered_l3_present ) doc["P3max"]   = P1Stats.P3max;
    
    if ( DSMRdata.power_delivered_l1_present ) doc["P1min"]   = P1Stats.P1min;
    if ( DSMRdata.power_delivered_l2_present ) doc["P2min"]   = P1Stats.P2min;
    if ( DSMRdata.power_delivered_l3_present ) doc["P3min"]   = P1Stats.P3min;

    if ( DSMRdata.voltage_l1_present ) doc["U1piek"]  = P1Stats.U1piek;
    if ( DSMRdata.voltage_l2_present ) doc["U2piek"]  = P1Stats.U2piek;
    if ( DSMRdata.voltage_l3_present ) doc["U3piek"]  = P1Stats.U3piek;

    if ( DSMRdata.voltage_l1_present )doc["U1min"]  = P1Stats.U1min;
    if ( DSMRdata.voltage_l2_present )doc["U2min"]  = P1Stats.U2min;
    if ( DSMRdata.voltage_l3_present )doc["U3min"]  = P1Stats.U3min;

    if ( DSMRdata.voltage_l1_present )doc["TU1over"] = actueleOverspanningSeconden(P1Stats.TU1over, startTijdL1, overspanningActiefL1);
    if ( DSMRdata.voltage_l2_present )doc["TU2over"] = actueleOverspanningSeconden(P1Stats.TU2over, startTijdL2, overspanningActiefL2);
    if ( DSMRdata.voltage_l3_present )doc["TU3over"] = actueleOverspanningSeconden(P1Stats.TU3over, startTijdL3, overspanningActiefL3);
    
    doc["Psluip"]  = P1Stats.Psluip;
    doc["start_time"] = P1Stats.StartTime;

  } );
}

void JsonGas(){
  if (!gasDelivered) return;
  jsonDoc["gas_delivered"]["value"] =  (int)(gasDelivered*1000)/1000.0;
  jsonDoc["gas_delivered"]["unit"]  = "m3";
  jsonDoc["gas_delivered_timestamp"]["value"] = gasDeliveredTimestamp;
}

void JsonPP(){
  if ( DSMRdata.highest_peak_pwr_present ) jsonDoc["highest_peak_pwr_timestamp"]["value"] = DSMRdata.highest_peak_pwr.timestamp;
}

int signalToEnum(const char* signal) {
  if (!signal) return -1;
  if (strcmp(signal, "--") == 0) return 0;
  if (strcmp(signal, "-") == 0)  return 1;
  if (strcmp(signal, "0") == 0)  return 2;
  if (strcmp(signal, "+") == 0)  return 3;
  if (strcmp(signal, "++") == 0) return 4;
  return -1; // ongeldige waarde
}

ApiResponse JsonEIDplanner(){
  
  JsonDocument doc;
  if ( StroomPlanData.size() == 0 ) {
    doc["h_start"] = 99;
    doc.createNestedArray("data");
    return jsonDocResponse(doc);
  }

  JsonArray dataArray = StroomPlanData["data"].as<JsonArray>();
  size_t plannerCount = dataArray.size();
  if (plannerCount == 0) {
    doc["h_start"] = 99;
    doc.createNestedArray("data");
    return jsonDocResponse(doc);
  }

  const char* timestamp = dataArray[0]["timestamp"].is<const char*>() ? dataArray[0]["timestamp"].as<const char*>() : nullptr;
  if (!timestamp || strlen(timestamp) < 13) {
    DebugTln(F("Ongeldige of ontbrekende timestamp"));
    doc["h_start"] = 99;
    doc.createNestedArray("data");
    return jsonDocResponse(doc);
  }

  doc["h_start"] = (timestamp[11] - '0') * 10 + (timestamp[12] - '0');
  JsonArray outputSignals = doc["data"].to<JsonArray>();
  for (int i = 0; i < 14; i++ ){
    const char* signal = nullptr;
    if (i < (int)plannerCount && dataArray[i]["signal"].is<const char*>()) signal = dataArray[i]["signal"].as<const char*>();
    outputSignals.add(signalToEnum(signal));
  }
  return jsonDocResponse(doc);
}

void JsonWater(){

  if ( !WtrMtr && !mbusWater ) return;  
  if ( mbusWater) {
    jsonDoc["water"]["value"] =  (int)(waterDelivered*1000)/1000.0;
    jsonDoc["water_delivered_ts"]["value"] = waterDeliveredTimestamp;
  } else {
    jsonDoc["water"]["value"] =  (float)P1Status.wtr_m3 + (P1Status.wtr_l?P1Status.wtr_l/1000.0:0);
  } 
  jsonDoc["water"]["unit"]  = "m3";
}

String HWrootJson() {

  return jsonResponse([&](JsonDocument& doc){
    String MACID = macID;
    MACID.toLowerCase();
    doc["product_name"] = "P1 Meter";
    doc["product_type"] = "HWE-P1";
    doc["serial"] = MACID;
    doc["firmware_version"] = "6.0304";
    doc["api_version"] = "v1";
  });
 }

String HWapiJson(){

  return jsonResponse([&](JsonDocument& jsonDoc){

    #define F3DEC(...) serialized(String(__VA_ARGS__,3))
    
    if ( WiFi.SSID().length() ) {
      jsonDoc["wifi_ssid"] = WiFi.SSID();
      jsonDoc["wifi_strength"] = abs(WiFi.RSSI());
    }
    else {
      jsonDoc["wifi_ssid"] = "NO-WIFI";
      jsonDoc["wifi_strength"] = 50;
    }

    int smr = 50;  // default fallback

    if ( DSMRdata.p1_version.length() > 0) {
      int parsed = atoi(DSMRdata.p1_version.c_str());

      if (parsed > 0) {
        smr = parsed;
      }
    }

    jsonDoc["smr_version"] = smr;
    jsonDoc["meter_model"] = DSMRdata.identification;
    jsonDoc["unique_id"] = DSMRdata.equipment_id;

    // Energieverbruik en teruglevering
    jsonDoc["active_tariff"] = DSMRdata.electricity_tariff.toInt();
    jsonDoc["total_power_import_kwh"] = F3DEC(DSMRdata.energy_delivered_total + DSMRdata.energy_delivered_tariff1 + DSMRdata.energy_delivered_tariff2);
    jsonDoc["total_power_import_t1_kwh"] = F3DEC(DSMRdata.energy_delivered_tariff1.val());
    jsonDoc["total_power_import_t2_kwh"] = F3DEC(DSMRdata.energy_delivered_tariff2.val());
    jsonDoc["total_power_export_kwh"] = F3DEC(DSMRdata.energy_returned_total + DSMRdata.energy_returned_tariff1 + DSMRdata.energy_returned_tariff2);
    jsonDoc["total_power_export_t1_kwh"] = F3DEC(DSMRdata.energy_returned_tariff1.val());
    jsonDoc["total_power_export_t2_kwh"] = F3DEC(DSMRdata.energy_returned_tariff2.val());

    // Huidige stroomwaarden
    jsonDoc["active_power_w"] = (int32_t)(DSMRdata.power_delivered.int_val() - DSMRdata.power_returned.int_val());
    jsonDoc["active_power_l1_w"] = (int32_t)(DSMRdata.power_delivered_l1.int_val() - DSMRdata.power_returned_l1.int_val());
    jsonDoc["active_power_l2_w"] = (int32_t)(DSMRdata.power_delivered_l2.int_val() - DSMRdata.power_returned_l2.int_val());
    jsonDoc["active_power_l3_w"] = (int32_t)(DSMRdata.power_delivered_l3.int_val() - DSMRdata.power_returned_l3.int_val());
    
    // Spanning en stroom
    jsonDoc["active_voltage_l1_v"] = F3DEC(DSMRdata.voltage_l1.val());
    jsonDoc["active_voltage_l2_v"] = F3DEC(DSMRdata.voltage_l2.val());
    jsonDoc["active_voltage_l3_v"] = F3DEC(DSMRdata.voltage_l3.val());

    float i1 = (DSMRdata.voltage_l1_present&&DSMRdata.voltage_l1.val())?jsonDoc["active_power_l1_w"].as<float>()/DSMRdata.voltage_l1.val():0.0f;
    float i2 = (DSMRdata.voltage_l2_present&&DSMRdata.voltage_l2.val())?jsonDoc["active_power_l2_w"].as<float>()/DSMRdata.voltage_l2.val():0.0f;
    float i3 = (DSMRdata.voltage_l3_present&&DSMRdata.voltage_l3.val())?jsonDoc["active_power_l3_w"].as<float>()/DSMRdata.voltage_l3.val():0.0f;

    jsonDoc["active_current_a"]    = F3DEC( abs(i1) + abs(i2) + abs(i3) );
    jsonDoc["active_current_l1_a"] = F3DEC(i1);
    jsonDoc["active_current_l2_a"] = F3DEC(i2);
    jsonDoc["active_current_l3_a"] = F3DEC(i3);

    jsonDoc["voltage_sag_l1_count"]   = 0;
    jsonDoc["voltage_sag_l2_count"]   = 0;
    jsonDoc["voltage_sag_l3_count"]   = 0;

    jsonDoc["voltage_swell_l1_count"] = 0;
    jsonDoc["voltage_swell_l2_count"] = 0;
    jsonDoc["voltage_swell_l3_count"] = 0;

    jsonDoc["any_power_fail_count"]   = 0;
    jsonDoc["long_power_fail_count"]  = 0;
    
   // Externe apparaten (zoals gasmeter)
    JsonArray external;
    if ( mbusGas || WtrMtr ) external = jsonDoc.createNestedArray("external");
    // Gasverbruik via M-Bus  
    if ( mbusGas ) {
      jsonDoc["total_gas_m3"] = F3DEC(gasDelivered);
      jsonDoc["gas_timestamp"] = strtoull( gasDeliveredTimestamp.substring(0, gasDeliveredTimestamp.length() - 1).c_str(), nullptr, 10);
      
     jsonDoc["gas_unique_id"] = 
        mbusGas == 1 ? DSMRdata.mbus1_equipment_id_tc :
        mbusGas == 2 ? DSMRdata.mbus2_equipment_id_tc :
        mbusGas == 3 ? DSMRdata.mbus3_equipment_id_tc :
        mbusGas == 4 ? DSMRdata.mbus4_equipment_id_tc :
        "";
      JsonObject gasMeter = external.createNestedObject();
      gasMeter["unique_id"] = jsonDoc["gas_unique_id"];
      gasMeter["type"] = "gas_meter";
      gasMeter["timestamp"] =  strtoull( gasDeliveredTimestamp.substring(0, gasDeliveredTimestamp.length() - 1).c_str(), nullptr, 10);
      gasMeter["value"] = F3DEC(gasDelivered);
      gasMeter["unit"] = "m3";
    }

    if ( WtrMtr ) {
      JsonObject waterMeter = external.createNestedObject();

        waterMeter["unique_id"] = 
        mbusWater == 1 ? DSMRdata.mbus1_equipment_id_tc :
        mbusWater == 2 ? DSMRdata.mbus2_equipment_id_tc :
        mbusWater == 3 ? DSMRdata.mbus3_equipment_id_tc :
        mbusWater == 4 ? DSMRdata.mbus4_equipment_id_tc :
        "8369788379824579787689"; //SENSOR-ONLY

      // waterMeter["unique_id"] = mbusGas ? DSMRdata.mbus1_equipment_id_tc;
      waterMeter["type"] = "water_meter";
      String Timestamp = actTimestamp;
      waterMeter["timestamp"] =  strtoull( mbusWater ? waterDeliveredTimestamp.substring(0, waterDeliveredTimestamp.length() - 1).c_str(): Timestamp.substring(0, Timestamp.length() - 1).c_str(), nullptr, 10);
      waterMeter["value"] = mbusWater ? F3DEC(waterDelivered) : F3DEC(P1Status.wtr_m3+P1Status.wtr_l/1000.0) ;
      waterMeter["unit"] = "m3";
    }
  });
}

struct buildJson {
    
    template<typename Item>
    void apply(Item &i) {
     char Name[25];
     strncpy(Name,String(Item::name).c_str(),25);

      if (isInFieldsArray(Name)) {
        
        if (i.present()) {          
          jsonDoc[Name]["value"] = value_to_json(i.val());
          if (String(Item::unit()).length() > 0) jsonDoc[Name]["unit"]  = Item::unit();
        }  else if (!onlyIfPresent) jsonDoc[Name]["value"] = "-";   
    } //infielsarrayname
  }
  
  template<typename Item>
  Item& value_to_json(Item& i) {
    return i;
  }

  double value_to_json(TimestampedFixedValue i) {
    return i.int_val()/1000.0;
  }
  
  double value_to_json(FixedValue i) {
    return i.int_val()/1000.0;
  }

}; // buildjson{} 
 
String deviceTimeJson() 
{
  return jsonResponse([&](JsonDocument& doc){
    doc["timestamp"] = actTimestamp;
    doc["time"] = buildDateTimeString(actTimestamp, sizeof(actTimestamp));
    doc["epoch"] = now();
  });
} // deviceTimeJson()

String network_state () {
  switch ( netw_state ){
    case NW_NONE: return "None";
    case NW_WIFI: return "WiFi";
    case NW_ETH: return "Ethernet";
    case NW_ETH_LINK: return "Ethernet Link (NC)";
    default: return "None";
  }
}

String deviceInfoJson() 
{
  JsonDocument doc;
  doc["fwversion"] = _VERSION_ONLY " ( " __DATE__ " " __TIME__ " )";
  doc["hostname"] = settingHostname;
  doc["ipaddress"] = IP_Address();
  doc["indexfile"] = settingIndexPage;
  doc["macaddress"] = macStr; 
  doc["freeheap"] ["value"] = ESP.getFreeHeap();
  doc["freeheap"]["unit"] = "bytes";
  doc["hardware"]= HWTypeNames[HardwareType];
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);

  doc["chipid"] = _getChipId();
  snprintf(cMsg, sizeof(cMsg), "model %x rev: %x cores: %x", chip_info.model, chip_info.revision, chip_info.cores);
  doc["coreversion"] = cMsg;
  doc["sdkversion"] = String( ESP.getSdkVersion() );
  doc["cpufreq"] ["value"] = ESP.getCpuFreqMHz();
  doc["cpufreq"]["unit"] = "MHz";
  doc["sketchsize"] ["value"] = (uint32_t)(ESP.getSketchSize() / 1024.0);
  doc["sketchsize"]["unit"] = "kB";
  
  doc["freesketchspace"] ["value"] = (uint32_t)(ESP.getFreeSketchSpace() / 1024.0);
  doc["freesketchspace"]["unit"] = "kB";
  doc["flashchipsize"] ["value"] = (uint32_t)(ESP.getFlashChipSize() / 1024 / 1024 );
  doc["flashchipsize"]["unit"] = "MB";
  doc["FSsize"] ["value"] = (uint32_t)(LittleFS.totalBytes() / (1024.0));
  doc["FSsize"]["unit"] = "kB";
  doc["compileoptions"] = ALL_OPTIONS;

  if ( netw_state == NW_WIFI ) {
    doc["ssid"] = WiFi.SSID();
    doc["wifirssi"] = WiFi.RSSI();
  }
  doc["network"] = network_state();
  doc["uptime"] = upTime();

  doc["telegramcount"] = (int)telegramCount;
  doc["telegramerrors"] = (int)telegramErrors;

#ifndef MQTT_DISABLE
  snprintf(cMsg, sizeof(cMsg), "%s:%04d", settingMQTTbroker, settingMQTTbrokerPort);
  doc["mqttbroker"] = cMsg;
  doc["mqttinterval"] = settingMQTTinterval;
  doc["mqttbroker_connected"] = !bMQTTenabled ? "off" : (MQTTclient.connected() ? "yes" : "no");
#endif
  char paired[18];
    sprintf(paired,"%i - %s", Pref.peers, en_connected?"connected":"unconnected");
  doc["paired"] = paired;

  doc["reboots"] = (int)P1Status.reboots;
  doc["lastreset"] = lastReset;  

  String out;
  serializeJson(doc, out);
  return out;
 
} // deviceInfoJson()

//=======================================================================
String deviceSettingsJson() {
  DebugTln(F("sending device settings ...\r"));
  JsonDocument doc;

  // Helper macro to add a setting to the JSON document
#define ADD_SETTING(name, type, min, max, value) \
  doc[name]["value"] = value;                     \
  doc[name]["type"] = type;                       \
  doc[name]["min"] = min;                         \
  doc[name]["max"] = max

  ADD_SETTING("hostname", "s", 0, sizeof(settingHostname) - 1, settingHostname);
  if ( !bWarmteLink ) { // IF NO HEATLINK
  ADD_SETTING("ed_tariff1", "f", 0, 10, settingEDT1);
  ADD_SETTING("ed_tariff2", "f", 0, 10, settingEDT2);
  ADD_SETTING("er_tariff1", "f", 0, 10, settingERT1);
  ADD_SETTING("er_tariff2", "f", 0, 10, settingERT2);
  ADD_SETTING("w_tariff", "f", 0, 10, settingWDT);
  }
  ADD_SETTING("gd_tariff", "f", 0, 10, settingGDT);
  if ( ! bWarmteLink ) { // IF NO HEATLINK
    ADD_SETTING("electr_netw_costs", "f", 0, 100, settingENBK);
    ADD_SETTING("water_netw_costs", "f", 0, 100, settingWNBK);
  }
  ADD_SETTING("gas_netw_costs", "f", 0, 100, settingGNBK);
  ADD_SETTING("IndexPage", "s", 0, sizeof(settingIndexPage) - 1, settingIndexPage);

#ifndef MQTT_DISABLE
if ( !hideMQTTsettings) {
  doc["mqtt_enabled"] = bMQTTenabled;
#ifndef MQTTKB

  doc["mqtt_tls"] = bMQTToverTLS;
  ADD_SETTING("mqtt_broker", "s", 0, sizeof(settingMQTTbroker) - 1, settingMQTTbroker);
  ADD_SETTING("mqtt_broker_port", "i", 1, 9999, settingMQTTbrokerPort);
  ADD_SETTING("mqtt_user", "s", 0, sizeof(settingMQTTuser) - 1, settingMQTTuser);
  ADD_SETTING("mqtt_passwd", "s", 0, sizeof(settingMQTTpasswd) - 1, settingMQTTpasswd);
#endif
  ADD_SETTING("mqtt_toptopic", "s", 0, sizeof(settingMQTTtopTopic) - 1, settingMQTTtopTopic);
  ADD_SETTING("mqtt_interval", "i", 0, 600, settingMQTTinterval);
  doc["act-json-mqtt"] = bActJsonMQTT;
  doc["macid-topic"] = MacIDinToptopic;
  doc["ha_disc_enabl"] = EnableHAdiscovery;
}
#endif

  if (strncmp(BaseOTAurl, "http://", 7) == 0) {
    char ota_url[sizeof(BaseOTAurl)];
    strncpy(ota_url, BaseOTAurl + 7, strlen(BaseOTAurl));
    ADD_SETTING("ota_url", "s", 0, sizeof(ota_url) - 1, ota_url);
  } else {
    ADD_SETTING("ota_url", "s", 0, sizeof(BaseOTAurl) - 1, BaseOTAurl);
  }

  ADD_SETTING("b_auth_user", "s", 0, sizeof(bAuthUser) - 1, bAuthUser);
  ADD_SETTING("b_auth_pw", "s", 0, sizeof(bAuthPW) - 1, bAuthPW);
  ADD_SETTING("overvoltage_threshold", "i", 200, 300, settingOvervoltageThreshold);
  
  //MODBUS TCP settings
  ADD_SETTING("mb_map", "i", 0, 9, SelMap); //RTU+TCP
  ADD_SETTING("mb_id", "i", 1, 255, mb_config.id); //RTU+TCP
  ADD_SETTING("mb_port", "i", 0, 65535, mb_config.port); //TCP
  if ( mb_rx != -1 ){ //check if modbus rtu hardware is available
    ADD_SETTING("mb_baud", "i", 300, 115200, mb_config.baud); //RTU
    ADD_SETTING("mb_parity", "i", 134217744, 134217791, mb_config.parity); //RTU
  }
  doc["mb_monitor"] = bModbusMonitor;
  //booleans
  doc["hist"] = EnableHistory;
  doc["auto_update"] = bAutoUpdate;
  doc["led"] = LEDenabled;
  doc["raw-port"] = bRawPort;
  doc["eid-enabled"] = bEID_enabled;
  doc["eid-planner"] = StroomPlanData.size() > 0 ? true : false;
  doc["nrgm-enabled"] = bNRGMenabled;
  #ifdef NETSWITCH
  doc["netsw-enabled"] = bNETSWenabled;
  #endif

  #ifdef UDP_BCAST
  doc["udp"] = bUDPenabled;
  #endif

  if ( bWarmteLink ) { // IF HEATLINK
    doc["conf"] = "p1-q";
  } else {
    doc["pre40"] = bPre40;
    doc["conf"] = "p1-p";
  }

  doc["water_enabl"] = WtrMtr;
  if (WtrMtr) {
    ADD_SETTING("water_m3", "i", 0, 99999, P1Status.wtr_m3);
    ADD_SETTING("water_l", "i", 0, 999, P1Status.wtr_l);
    ADD_SETTING("water_fact", "f", 0, 10, WtrFactor);
  }

  String out;
  serializeJson(doc, out);
  return out;
}

//====================================================
ApiResponse handleSmApiField(){
  jsonDoc.clear();
  if ( httpServer.pathArg(0) == "gas_delivered" ) JsonGas();
  else if ( httpServer.pathArg(0) == "water") JsonWater();
  else {
    //other fields
    onlyIfPresent = false;
    strCopy(Onefield, 24, httpServer.pathArg(0).c_str());
    fieldsElements = FIELDELEMENTS;
    DSMRdata.applyEach(buildJson());
  }
  return jsonDocResponse(jsonDoc);
}

ApiResponse handleSmApi()
{
  switch ( httpServer.pathArg(0)[0]) {
    
  case 'i': //info
    onlyIfPresent = false;
    fieldsElements = INFOELEMENTS;
    break;
  
  case 'a': //actual
    fieldsElements = ACTUALELEMENTS;
    onlyIfPresent = true;
    break;
  
  case 'f': //fields
    fieldsElements = 0;
    onlyIfPresent = false;
    break;  
    
  case 't': //telegram
    return {200, "text/plain", CapTelegram};
    
  default:
    return jsonNotFoundResponse();
    
  } //switch

    jsonDoc.clear();
    DSMRdata.applyEach(buildJson());
    JsonGas();
    JsonWater();
    JsonPP();
    return jsonDocResponse(jsonDoc);
    
} // handleSmApi()
//====================================================

String smActualJsonDebug() {
  const byte previousFieldsElements = fieldsElements;
  const bool previousOnlyIfPresent = onlyIfPresent;

  jsonDoc.clear();
  fieldsElements = ACTUALELEMENTS;
  onlyIfPresent = true;
  DSMRdata.applyEach(buildJson());
  JsonGas();
  JsonWater();
  JsonPP();

  String out;
  serializeJson(jsonDoc, out);

  jsonDoc.clear();
  fieldsElements = previousFieldsElements;
  onlyIfPresent = previousOnlyIfPresent;
  return out;
}
//====================================================

ApiResponse handleDevApi()
{
   if ( httpServer.pathArg(0) == "info" )
  {
    return jsonOkResponse(deviceInfoJson());
  }
  else if ( httpServer.pathArg(0) == "time")
  {
    return jsonOkResponse(deviceTimeJson());
  }
  else if (httpServer.pathArg(0) == "settings")
  {
    if (httpServer.method() == HTTP_PUT || httpServer.method() == HTTP_POST)
    {
      String jsonIn  = httpServer.arg(0).c_str();
      DebugT("json in :");Debugln(jsonIn);
      char field[25] = "";
      char newValue[101]="";

      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, jsonIn);
      if (!error)
      {
        const char* fieldIn = doc["name"] | "";
        strCopy(field, sizeof(field), fieldIn);

        JsonVariantConst value = doc["value"];
        if (value.is<const char*>()) {
          strCopy(newValue, sizeof(newValue), value.as<const char*>());
        } else if (value.is<bool>()) {
          strCopy(newValue, sizeof(newValue), value.as<bool>() ? "true" : "false");
        } else {
          String valueAsString = value.as<String>();
          strCopy(newValue, sizeof(newValue), valueAsString.c_str());
        }
      }
      else
      {
        // Backward compatible fallback for malformed payloads.
        String wOut[5];
        String wPair[5];
        jsonIn.replace("{", "");
        jsonIn.replace("}", "");
        jsonIn.replace("\"", "");
        int8_t wp = splitString(jsonIn.c_str(), ',',  wPair, 5) ;
        for (int i=0; i<wp; i++)
        {
          int8_t wc = splitString(wPair[i].c_str(), ':',  wOut, 5) ;
          (void)wc;
          if (wOut[0].equalsIgnoreCase("name"))  strCopy(field, sizeof(field), wOut[1].c_str());
          if (wOut[0].equalsIgnoreCase("value")) strCopy(newValue, sizeof(newValue), wOut[1].c_str());
        }
      }

      //DebugTf("--> field[%s] => newValue[%s]\r\n", field, newValue);
      updateSetting(field, newValue);
      return jsonOkResponse(httpServer.arg(0));
    }
    else
    {
      return jsonOkResponse(deviceSettingsJson());
    }
  }
  else return jsonNotFoundResponse();
  
} // handleDevApi()

ApiResponse handleModbusMonitorApi() {
  if (httpServer.method() == HTTP_POST) {
    clearModbusMonitorEntries();
    JsonDocument doc;
    doc["cleared"] = true;
    return jsonDocResponse(doc);
  }

  return jsonOkResponse(modbusMonitorJson());
}

bool isInFieldsArray(const char* lookUp)
{                        
//    DebugTf("Elemts[%2d] | LookUp [%s] | LookUpType[%2d]\r\n", elemts, lookUp, LookUpType);
  if (fieldsElements == 0) return true;  
  for (int i=0; i<fieldsElements; i++)
  {
    //if (Verbose2) DebugTf("[%2d] Looking for [%s] in array[%s]\r\n", i, lookUp, fieldsArray[i]); 
    if (fieldsElements == ACTUALELEMENTS ) { if (strcmp_P(lookUp, actualArray[i]) == 0 ) return true; }
    else if (fieldsElements == INFOELEMENTS ) { if (strcmp_P(lookUp, infoArray[i]) == 0 ) return true; }
    else if (fieldsElements == FIELDELEMENTS ) { if ( (strcmp(lookUp, Onefield) == 0) || (strcmp(lookUp, "timestamp") == 0)  ) return true; }
  }

  return false; 
} // isInFieldsArray()

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
