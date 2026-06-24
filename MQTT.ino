/* 
***************************************************************************  
**  Copyright (c) 2023 Martijn Hendriks 
***************************************************************************      
*/

#ifndef MQTT_DISABLE 

String MQTTclientId;
static volatile bool haUpdateStatePending = true;
static bool haUpdateInProgress = false;
static uint8_t haUpdateProgress = 0;

static bool MQTTPublishHAUpdateState() {
  if (!MQTTclient.connected()) return false;

  char stateTopic[sizeof(MQTopTopic) + 20];
  snprintf(stateTopic, sizeof(stateTopic), "%supdate/state", MQTopTopic);

  JsonDocument doc;
  doc["installed_version"] = _VERSION_ONLY;
  doc["latest_version"] = LatestFirmwareVersion();
  doc["title"] = "DSMR-API Firmware";
  doc["in_progress"] = haUpdateInProgress;
  if (haUpdateInProgress) doc["update_percentage"] = haUpdateProgress;
  else doc["update_percentage"] = nullptr;

  String payload;
  serializeJson(doc, payload);
  if (MQTTclient.publish(stateTopic, payload.c_str(), true)) return true;

  DebugTf("Error publish HA update state (%s), MQTT state %d\r\n",
          stateTopic, MQTTclient.state());
  return false;
}

void CreateMacIDTopic() {
  snprintf( MQTopTopic, sizeof(MQTopTopic), "%s%s%s", settingMQTTtopTopic, MacIDinToptopic?macID:"",MacIDinToptopic?"/":"" );
#ifdef DEBUG
  DebugT("MQTopTopic: ");Debugln(MQTopTopic);
#endif
}

void fMqtt( void * pvParameters ){
#ifndef MQTT_DISABLE    
  DebugTln(F("Start MQTT Thread"));
  // MQTTSetBaseInfo();
  MQTTsetServer();
  esp_task_wdt_add(nullptr);
  while(true) {
    PrintHWMark(1);
    handleMQTT();
    esp_task_wdt_reset();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
#endif
  LogFile("Mqtt: unexpected task exit", true);
  vTaskDelete(NULL);
}

void StartMqttTask(){
  if ( skipNetwork ) return;
  if( xTaskCreate( fMqtt, "mqtt", 1024*6, NULL, 6, NULL) == pdPASS ) DebugTln(F("Task MQTT succesfully created"));
}

void handleMQTT(){
  MQTTclient.loop();
  if (haUpdateStatePending) haUpdateStatePending = !MQTTPublishHAUpdateState();
  if ( bSendMQTT ) sendMQTTData();
  MQTTConnect();
}

void MQTTSetHAUpdateState(bool inProgress, uint8_t progress) {
  haUpdateInProgress = inProgress;
  haUpdateProgress = progress;
  haUpdateStatePending = true;
}

static void addIfNotEmpty(JsonDocument& doc, const char* key, const char* value) {
  if (value && value[0]) doc[key] = value;
}

static void addIfNotEmpty(JsonObject doc, const char* key, const char* value) {
  if (value && value[0]) doc[key] = value;
}

static void addHAExtraPayload(JsonDocument& doc, const char* extrapl) {
  if (!extrapl || !extrapl[0]) return;

  String extra = "{";
  extra += extrapl;
  extra += "\"_\":0}";

  JsonDocument extraDoc;
  if (deserializeJson(extraDoc, extra)) return;

  for (JsonPair kv : extraDoc.as<JsonObject>()) {
    if (strcmp(kv.key().c_str(), "_") != 0) doc[kv.key()] = kv.value();
  }
}

void SendAutoDiscoverHA(const char* dev_name, const char* dev_class, const char* dev_title, const char* dev_unit, const char* dev_payload, const char* state_class, const char* extrapl ){
  char msg_topic[120];
  char unique_id[50];
  char state_topic[sizeof(MQTopTopic) + 25];
  char chipId[24];
  const char* macSuffix = strlen(macID) > 8 ? macID + strlen(macID) - 8 : macID;
  if (HAUniqueIds) {
    snprintf(msg_topic, sizeof(msg_topic), "homeassistant/sensor/%s-%s/%s/config", settingHostname, macSuffix, dev_name);
    snprintf(unique_id, sizeof(unique_id), "%s_%s", dev_name, macSuffix);
  } else {
    snprintf(msg_topic, sizeof(msg_topic), "homeassistant/sensor/%s/%s/config", settingHostname, dev_name);
    strlcpy(unique_id, dev_name, sizeof(unique_id));
  }
//    Debugln(msg_topic);
  snprintf(state_topic, sizeof(state_topic), "%s%s", MQTopTopic, dev_name);
  snprintf(chipId, sizeof(chipId), "%llu", (unsigned long long)_getChipId());

  JsonDocument doc;
  addIfNotEmpty(doc, "uniq_id", unique_id);
  addIfNotEmpty(doc, "dev_cla", dev_class);
  addIfNotEmpty(doc, "name", dev_title);
  addIfNotEmpty(doc, "stat_t", state_topic);
  addIfNotEmpty(doc, "unit_of_meas", dev_unit);
  addIfNotEmpty(doc, "val_tpl", dev_payload);
  addIfNotEmpty(doc, "stat_cla", state_class);
  addHAExtraPayload(doc, extrapl);

  JsonObject dev = doc["dev"].to<JsonObject>();
  addIfNotEmpty(dev, "ids", chipId);
  addIfNotEmpty(dev, "name", settingHostname);
  dev["mdl"] = settingHostname;
  dev["mf"] = "Smartstuff";

  String msg_payload;
  serializeJson(doc, msg_payload);
//  Debugln(msg_payload);
  if (!MQTTclient.publish(msg_topic, msg_payload.c_str(), true)) {
    DebugTf("Error publish(%s) [%s] [%u bytes]\r\n",
            msg_topic, msg_payload.c_str(),
            (unsigned)(strlen(msg_topic) + msg_payload.length()));
  }
}

static void SendAutoDiscoverHAUpdate() {
  char configTopic[120];
  char uniqueId[50];
  char stateTopic[sizeof(MQTopTopic) + 20];
  char commandTopic[sizeof(MQTopTopic) + 20];
  char chipId[24];
  const char* macSuffix = strlen(macID) > 8 ? macID + strlen(macID) - 8 : macID;

  if (HAUniqueIds) {
    snprintf(configTopic, sizeof(configTopic), "homeassistant/update/%s-%s/firmware/config", settingHostname, macSuffix);
    snprintf(uniqueId, sizeof(uniqueId), "firmware_update_%s", macSuffix);
  } else {
    snprintf(configTopic, sizeof(configTopic), "homeassistant/update/%s/firmware/config", settingHostname);
    strlcpy(uniqueId, "firmware_update", sizeof(uniqueId));
  }
  snprintf(stateTopic, sizeof(stateTopic), "%supdate/state", MQTopTopic);
  snprintf(commandTopic, sizeof(commandTopic), "%supdate/install", MQTopTopic);
  snprintf(chipId, sizeof(chipId), "%llu", (unsigned long long)_getChipId());

  JsonDocument doc;
  doc["uniq_id"] = uniqueId;
  doc["dev_cla"] = "firmware";
  doc["name"] = "DSMR Firmware";
  doc["stat_t"] = stateTopic;
  doc["cmd_t"] = commandTopic;
  doc["pl_inst"] = "install";

  JsonObject dev = doc["dev"].to<JsonObject>();
  dev["ids"] = chipId;
  dev["name"] = settingHostname;
  dev["mdl"] = settingHostname;
  dev["mf"] = "Smartstuff";

  String payload;
  serializeJson(doc, payload);
  MQTTclient.publish(configTopic, payload.c_str(), true);
}

void AutoDiscoverHA(){
  if (!EnableHAdiscovery) return;
//mosquitto_pub -h 192.168.2.250 -p 1883 -t "homeassistant/sensor/p1-dongle-pro/power_delivered/config" -m '{"uniq_id":"power_delivered","dev_cla":"power","name":"Power Delivered","stat_t":"Eth-Dongle-Pro/power_delivered","unit_of_meas":"W","val_tpl":"{{ value | round(3) * 1000 }}","stat_cla":"measurement","dev":{"ids":"36956260","name":"Eth-Dongle-Pro","mdl":"P1 Dongle Pro","mf":"Smartstuff"}}'
//#ifndef HEATLINK
  if ( ! bWarmteLink ) { // IF NO HEATLINK  
    SendAutoDiscoverHA("timestamp", "timestamp", "DSMR Last Update", "", "{{ strptime(value[:-1] + '-+0200' if value[12] == 'S' else value[:-1] + '-+0100', '%y%m%d%H%M%S-%z') }}","", "\"icon\": \"mdi:clock\",");
    SendAutoDiscoverHA("uptime", "duration", "Uptime", "s", "","","");

    SendAutoDiscoverHA("power_delivered", "power", "Power Delivered", "W", "{{ value | round(3) * 1000 }}","measurement","");
    SendAutoDiscoverHA("power_returned" , "power", "Power Returned" , "W", "{{ value | round(3) * 1000 }}","measurement","");  
    
    SendAutoDiscoverHA("energy_delivered_tariff1", "energy", "Energy Delivered T1", "kWh", "{{ value | round(3) }}","total_increasing","");
    SendAutoDiscoverHA("energy_delivered_tariff2", "energy", "Energy Delivered T2", "kWh", "{{ value | round(3) }}","total_increasing","");
    SendAutoDiscoverHA("energy_returned_tariff1", "energy", "Energy Returned T1", "kWh", "{{ value | round(3) }}","total_increasing","");
    SendAutoDiscoverHA("energy_returned_tariff2", "energy", "Energy Returned T2", "kWh", "{{ value | round(3) }}","total_increasing","");
    
    SendAutoDiscoverHA("power_delivered_l1", "power", "Power Delivered l1", "W", "{{ value | round(3) * 1000 }}","measurement","");
    SendAutoDiscoverHA("power_delivered_l2", "power", "Power Delivered l2", "W", "{{ value | round(3) * 1000 }}","measurement","");
    SendAutoDiscoverHA("power_delivered_l3", "power", "Power Delivered l3", "W", "{{ value | round(3) * 1000 }}","measurement","");
  
    SendAutoDiscoverHA("power_returned_l1", "power", "Power Returned l1", "W", "{{ value | round(3) * 1000 }}","measurement","");
    SendAutoDiscoverHA("power_returned_l2", "power", "Power Returned l2", "W", "{{ value | round(3) * 1000 }}","measurement","");
    SendAutoDiscoverHA("power_returned_l3", "power", "Power Returned l3", "W", "{{ value | round(3) * 1000 }}","measurement","");
  
    SendAutoDiscoverHA("voltage_l1", "voltage", "Voltage l1", "V", "{{ value | round(1) }}","measurement","");
    SendAutoDiscoverHA("voltage_l2", "voltage", "Voltage l2", "V", "{{ value | round(1) }}","measurement","");
    SendAutoDiscoverHA("voltage_l3", "voltage", "Voltage l3", "V", "{{ value | round(1) }}","measurement","");
    
    SendAutoDiscoverHA("current_l1", "current", "Current l1", "A", "{{ value | round(3) }}","measurement","");
    SendAutoDiscoverHA("current_l2", "current", "Current l2", "A", "{{ value | round(3) }}","measurement","");
    SendAutoDiscoverHA("current_l3", "current", "Current l3", "A", "{{ value | round(3) }}","measurement","");
  
    SendAutoDiscoverHA("gas_delivered", "gas", "Gas Delivered", "m³", "{{ value | round(3) }}","total_increasing","");
    SendAutoDiscoverHA("peak_pwr_last_q", "power", "Peak last Quarter", "W", "{{ value | round(3) * 1000 }}","measurement","");
    SendAutoDiscoverHA("highest_peak_pwr", "power", "Highest Peak this month", "W", "{{ value | round(3) * 1000 }}","measurement","");
    
    SendAutoDiscoverHA("water", "water", "Waterverbruik", "m³", "{{ value | round(3) }}","total_increasing","\"icon\": \"mdi:water\",");
//#else 
  } else {
  //= HEATLINK
    SendAutoDiscoverHA("timestamp", "timestamp", "Last Update", "", "{{ strptime(value[:-1] + '-+0200' if value[12] == 'S' else value[:-1] + '-+0100', '%y%m%d%H%M%S-%z') }}","", "\"icon\": \"mdi:clock\",");
    SendAutoDiscoverHA("heat_delivered", "energy", "Heat Delivered", "GJ", "{{ value | round(3) }}","total_increasing","");
//#endif  
  }

  SendAutoDiscoverHAUpdate();
}

void MQTTDisconnect(){
  if ( skipNetwork) return;
  if (MQTTclient.connected()) {
    sprintf(cMsg,"%sLWT",MQTopTopic);
    MQTTclient.publish(cMsg,"Offline", true); //LWT status update
  }
  if ( MQTTclient.connected() ) MQTTclient.disconnect();
}

void MQTTsetServer(){
#ifndef MQTT_DISABLE 
  MQTTDisconnect(); //close active connection
  if (!bMQTTenabled) return;
  if ((settingMQTTbrokerPort == 0) || (strlen(settingMQTTbroker) < 4) ) return;
  // MQTTDisconnect();
  if (bMQTToverTLS) {
    wifiClientTLS.setInsecure();
    MQTTclient.setClient(wifiClientTLS);
  }
  else {
    MQTTclient.setClient(wifiClient);
  }
  MQTTclient.setBufferSize(MQTT_BUFF_MAX);
  MQTTclient.setKeepAlive(60);
  DebugVerboseTf("setServer(%s, %lu) \r\n", settingMQTTbroker, (unsigned long)settingMQTTbrokerPort);
  MQTTclient.setServer(settingMQTTbroker, settingMQTTbrokerPort);

//  CHANGE_INTERVAL_SEC(reconnectMQTTtimer, 1);
  MQTTConnect(); //try to connect

#endif
}

void MqttReconfig(String payload){

/*
{
    "broker": "url",
    "port": 1892,
    "user": "name",
    "pass": "word"
}
*/
  // 1) deserialise -> json
  // 2) test new connection
  // 3) when okay copy new config and restart mqtt process
  
  JsonDocument doc; 
  DeserializationError error = deserializeJson(doc, payload);
  if (error) return;
  if ( doc["broker"].is<const char*>() && doc["port"].is<int>() && doc["user"].is<const char*>() && doc["pass"].is<const char*>() ){
    //test connection
    MQTTSend( "msg", "MQTT: reconfig check connection", true );
    char MqttID[sizeof(settingHostname) + sizeof(macID) + 1];
    snprintf(MqttID, sizeof(MqttID), "%s-%s", settingHostname, macID);
    MQTTDisconnect();
    MQTTclient.setServer(doc["broker"].as<const char*>(), doc["port"].as<uint16_t>());
    if ( MQTTclient.connect( MqttID, doc["user"].as<const char*>(), doc["pass"].as<const char*>() ) ) {
      //adapt the new settings
      DebugTln("MQTT config check succesfull");
      settingMQTTbrokerPort = doc["port"].as<uint16_t>();
      strlcpy(settingMQTTbroker, doc["broker"].as<const char*>(), sizeof(settingMQTTbroker));
      strlcpy(settingMQTTuser, doc["user"].as<const char*>(), sizeof(settingMQTTuser));
      strlcpy(settingMQTTpasswd, doc["pass"].as<const char*>(), sizeof(settingMQTTpasswd));
      writeSettings(); //save the settings
      MQTTsetServer();
    } else {
      //fallback to old config
      DebugTln("MQTT config check FAILED");
      MQTTsetServer();
      MQTTSend( "msg", "MQTT: connection check FAILED", true );
    } 
  } else {
    DebugTln("MQTT config failure: missing elements");
    MQTTSend( "msg", "MQTT: config failure due to missing json keys", true );
  }
} // end MqttConfig

//===========================================================================================

static void MQTTcallback(char* topic, byte* payload, unsigned int len) {
  String StrTopic = topic;
  constexpr size_t MQTT_PAYLOAD_MAX = 1024;
  char StrPayload[MQTT_PAYLOAD_MAX + 1];

  DebugTraceTf("Message length: %d\n",len );
  size_t copyLen = len;
  if (copyLen > MQTT_PAYLOAD_MAX) {
    DebugVerboseTf("MQTT payload truncated from %u to %u bytes\n", (unsigned)len, (unsigned)MQTT_PAYLOAD_MAX);
    copyLen = MQTT_PAYLOAD_MAX;
  }
  memcpy(StrPayload, payload, copyLen);
  StrPayload[copyLen] = '\0';
  DebugTraceT(F("Message arrived [")); DebugTrace(StrTopic); DebugTrace(F("] ")); DebugTraceLn(StrPayload);

  char updateTopic[sizeof(MQTopTopic) + 20];
  char installTopic[sizeof(MQTopTopic) + 20];
  snprintf(updateTopic, sizeof(updateTopic), "%supdate", MQTopTopic);
  snprintf(installTopic, sizeof(installTopic), "%supdate/install", MQTopTopic);

  if (strcmp(topic, installTopic) == 0 && strcmp(StrPayload, "install") == 0) {
    DebugVerboseT(F("HA MQTT install command received"));
    QueueRemoteUpdate(LatestFirmwareVersion(), true);
  } else if (strcmp(topic, updateTopic) == 0) {
    DebugVerboseT(F("MQTT update command received"));
    QueueRemoteUpdate(StrPayload, true);
  }
  if ( StrTopic.indexOf("interval") >= 0) {
    settingMQTTinterval =  String(StrPayload).toInt();
    CHANGE_INTERVAL_MS(publishMQTTtimer, 1000 * settingMQTTinterval - 100);
    writeSettings();
  }
  if ( StrTopic.indexOf("reboot") >= 0) {
    LogFile("reboot: mqtt command", false);
    P1Reboot();
  }
  if ( StrTopic.indexOf("reconfig") >= 0) {
    MqttReconfig(StrPayload);
  }
  if ( StrTopic.indexOf("toptopic") >= 0) {
      strlcpy(settingMQTTtopTopic, StrPayload, sizeof(settingMQTTtopTopic));
      size_t topLen = strlen(settingMQTTtopTopic);
      if (topLen > 0 && settingMQTTtopTopic[topLen - 1] != '/') strlcat(settingMQTTtopTopic, "/", sizeof(settingMQTTtopTopic));
      CreateMacIDTopic();
      if ( MQTTclient.connected() ) MQTTclient.disconnect();
      writeSettings();
  }
  if ( StrTopic.indexOf("ota-url") >= 0) {
      strlcpy(BaseOTAurl, StrPayload, sizeof(BaseOTAurl));
      size_t otaLen = strlen(BaseOTAurl);
      if (otaLen > 0 && BaseOTAurl[otaLen - 1] != '/') strlcat(BaseOTAurl, "/", sizeof(BaseOTAurl));
      writeSettings();
  }

}

//===========================================================================================
void MQTTConnect() {
// #ifndef ETHERNET
//   if ( DUE( reconnectMQTTtimer) && (WiFi.status() == WL_CONNECTED)) {
// #else
  if (!bMQTTenabled) return;
  if ( MQTTclient.connected() || !strlen(settingMQTTbroker) || settingMQTTbrokerPort == 0 ) return; //interval 0 will connect to the broker
  if ( DUE( reconnectMQTTtimer) && ( netw_state == NW_ETH || netw_state == NW_WIFI) ) {    
// #endif    
    LogFile("MQTT: RECONNECT to broker...", true);
    char MqttID[sizeof(settingHostname) + sizeof(macID) + 1];
    snprintf(MqttID, sizeof(MqttID), "%s-%s", settingHostname, macID);
    snprintf( cMsg, 150, "%sLWT", MQTopTopic );
    DebugVerboseTf("connect %s user=%s passwd=%s lwt=%s\n",
                   MqttID,
                   strlen(settingMQTTuser) ? "<set>" : "<empty>",
                   strlen(settingMQTTpasswd) ? "<set>" : "<empty>",
                   cMsg);
    
    // if ( MQTTclient.connect( MqttID, settingMQTTuser, settingMQTTpasswd, cMsg, 1, true, "Offline" ) ) {
    mqttConnectActive = true;
    if ( MQTTclient.connect( MqttID, settingMQTTuser, settingMQTTpasswd ) ) {
      LogFile("MQTT: CONNECTED to broker", true);
      MQTTclient.publish(cMsg,"Online", true); //LWT = online
      StaticInfoSend = false; //resend
      MQTTclient.setCallback(MQTTcallback); //set listner update callback
      
      const char* topics[] = {
          "update",
          "update/install",
          "interval",
          "reboot",
          "reconfig",
          "toptopic",
          "ota-url"
        };

      // subscribe on all topics
      for (auto& t : topics) {
        snprintf(cMsg, sizeof(cMsg), "%s%s", MQTopTopic, t);
        MQTTclient.subscribe(cMsg);
      }

      haUpdateStatePending = !MQTTPublishHAUpdateState();
      if ( EnableHAdiscovery ) AutoDiscoverHA();
    } else {
      LogFile("MQTT: ... connection FAILED! Will try again in 10 sec", true);
      DebugT("error code: ");Debugln(MQTTclient.state());
    }
    mqttConnectActive = false;
  } //due
} //mqttconnect

//=======================================================================

struct buildJsonMQTT {
/* twee types
 *  {energy_delivered_tariff1":[{"value":11741.29,"unit":"kWh"}]}"
 *  {equipment_id":[{"value":"4530303435303033383833343439383137"}]}"
 *  
 *  msg = "{\""+Name+"\":[{\"value\":"+value_to_json(i.val())+",\"unit\":"+Unit+"}]}";
 *  msg = "\"{\""+Name+"\":[{\"value\":"+value_to_json(i.val())+"}]}\""
 *  
 */   
    template<typename Item>
    void apply(Item &i) {
     char Name[25];
     strncpy(Name,String(Item::name).c_str(),sizeof(Name));
    if ( isInFieldsArray(Name) && i.present() ) {
          // add value to '/all' topic
          if ( bActJsonMQTT ) jsonDoc[Name] = value_to_json_mqtt(i.val());
          else if ( MQTTclient.connected() && (!bActJsonMQTT || EnableHAdiscovery) ) {
            sprintf(cMsg,"%s%s",MQTopTopic,Name);
            MQTTclient.publish( cMsg, String(value_to_json(i.val())).c_str() );
          }
    } // if isInFieldsArray && present
  } //apply

  template<typename Item>
  Item& value_to_json(Item& i) {
    return i;
  }

  String value_to_json(TimestampedFixedValue i) {
    return String(i,3);
  }
  
  String value_to_json(FixedValue i) {
    return String(i,3);
  }

  template<typename Item>
  Item& value_to_json_mqtt(Item& i) {
    return i;
  }

  String value_to_json_mqtt(TimestampedFixedValue i) {
    return String(i,3);
  }

  double value_to_json_mqtt(FixedValue i) {
    return i.int_val()/1000.0;
  }

}; // buildJsonMQTT

//===========================================================================================

void MQTTSend(const char* item, String value, bool ret){
  if ( !bMQTTenabled || value.length()==0 || !MQTTclient.connected() ) return;
  sprintf(cMsg,"%s%s", MQTopTopic,item);
  if (!MQTTclient.publish(cMsg, value.c_str(), ret )) {
    DebugTf("Error publish (%s) [%s] [%d bytes]\r\n", cMsg, value.c_str(), (strlen(cMsg) + value.length()));
    StaticInfoSend = false; //probeer het later nog een keer
    return;
  }
  if ( strcmp(item,"all") ==0 ) mqttCount++; //counts the number of succesfull all topics send.
}
//===========================================================================================

void MQTTSend(const char* item, float value){
  // prevent invalid negatives for total_increasing sensors
  if (strstr(item, "gas_delivered") || strstr(item, "water")) if (value < 0) value = 0.0;

  char temp[10];
  sprintf(temp,"%.3f",value);
  MQTTSend(item, temp, true);
}

//===========================================================================================
void MQTTSentStaticInfo(){
  if ( skipNetwork ) return;
  if (!bMQTTenabled || (settingMQTTinterval == 0) || (strlen(settingMQTTbroker) < 4) ) return;
  StaticInfoSend = true;
  MQTTSend( "identification",DSMRdata.identification, true );
  MQTTSend( "mac",macStr, true );
  MQTTSend( "p1_version",DSMRdata.p1_version, true );
  MQTTSend( "equipment_id",DSMRdata.equipment_id, true );
  MQTTSend( "firmware",_VERSION_ONLY, true );
  MQTTSend( "ip_address",IP_Address(), true);
  MQTTSend( "wifi_rssi",String( WiFi.RSSI() ), true );

  if (DSMRdata.mbus1_equipment_id_tc_present){ MQTTSend("gas_equipment_id",DSMRdata.mbus1_equipment_id_tc, true); }  
}

//===========================================================================================
void MQTTsendGas(){
  if (!gasDelivered) return;
//#ifdef HEATLINK
  if ( bWarmteLink ) { // IF HEATLINK
  MQTTSend( "heat_delivered", gasDelivered );
//  MQTTSend( "heat_delivered_ts", gasDeliveredTimestamp ); // double: because device timestamp is equal
//#else
  } else {
  MQTTSend( "gas_delivered", gasDelivered );
  MQTTSend( "gas_delivered_timestamp", gasDeliveredTimestamp, true );
//#endif
  }
}

void MQTTSendVictronData(){
  auto gridPower = [](auto delivered, auto returned) -> int32_t {
    return (int32_t)((delivered.int_val() + returned.int_val()) * (returned ? -1.0 : 1.0));
  };
  auto fixedValue = [](auto value, uint8_t decimals) -> float {
    float scaled = value.int_val() / 1000.0f;
    if (decimals == 0) return roundf(scaled);
    if (decimals == 1) return roundf(scaled * 10.0f) / 10.0f;
    return scaled;
  };

  JsonDocument doc;
  JsonObject grid = doc["grid"].to<JsonObject>();
  grid["power"] = gridPower(DSMRdata.power_delivered, DSMRdata.power_returned);

  JsonObject l1 = grid["L1"].to<JsonObject>();
  l1["power"] = gridPower(DSMRdata.power_delivered_l1, DSMRdata.power_returned_l1);
  l1["voltage"] = fixedValue(DSMRdata.voltage_l1, 1);
  l1["current"] = fixedValue(DSMRdata.current_l1, 0);

  JsonObject l2 = grid["L2"].to<JsonObject>();
  l2["power"] = gridPower(DSMRdata.power_delivered_l2, DSMRdata.power_returned_l2);
  l2["voltage"] = fixedValue(DSMRdata.voltage_l2, 1);
  l2["current"] = fixedValue(DSMRdata.current_l2, 0);

  JsonObject l3 = grid["L3"].to<JsonObject>();
  l3["power"] = gridPower(DSMRdata.power_delivered_l3, DSMRdata.power_returned_l3);
  l3["voltage"] = fixedValue(DSMRdata.voltage_l3, 1);
  l3["current"] = fixedValue(DSMRdata.current_l3, 0);

  String jsondata;
  serializeJson(doc, jsondata);
  
  DebugTrace(F("Victron jsondata: ")); DebugTraceLn(jsondata);

  MQTTSend( "victron/grid", jsondata, false);  
}

//===========================================================================================
void sendMQTTData() {
  
  bSendMQTT = false; 

//TODO: log to file on error or reconnect
#ifndef ETHERNET
  if ( WiFi.status() != WL_CONNECTED ) return;
#endif  
  if ( !bMQTTenabled || (settingMQTTinterval == 0) || (strlen(settingMQTTbroker) < 4) ) return;
  MQTTConnect();
  if ( MQTTclient.connected() ) {   
  mqttPublishActive = true;
    
  DebugVerboseTf("Sending data to MQTT server [%s]:[%lu]\r\n",
                 settingMQTTbroker, (unsigned long)settingMQTTbrokerPort);
  
  if ( !StaticInfoSend )  MQTTSentStaticInfo();
    
  fieldsElements = ACTUALELEMENTS;
  
  if ( bActJsonMQTT ) jsonDoc.clear();

  DSMRdata.applyEach(buildJsonMQTT());

  if ( bActJsonMQTT ) {
    String buffer;
      if ( WtrMtr ) {
        jsonDoc["water"]    = waterDelivered;
        jsonDoc["water_ts"] = waterDeliveredTimestamp;
      }
      if ( mbusGas ) {
        jsonDoc["gas"]      = gasDelivered;
        jsonDoc["gas_ts"]   = gasDeliveredTimestamp;
      }
    serializeJson(jsonDoc,buffer);
    MQTTSend("all", buffer, false);
  } else {
    MQTTsendGas();
    MQTTsendWater();  
  }
  if ( DSMRdata.highest_peak_pwr_present ) MQTTSend( "highest_peak_pwr_ts", String(DSMRdata.highest_peak_pwr.timestamp), true);
  MQTTSend( "uptime",String(uptime()), false);

#ifdef VICTRON_GRID
  MQTTSendVictronData();
#endif

  mqttPublishActive = false;
  }
}

#else //MQTT disabled

void MQTTSentStaticInfo(){}
void MQTTSend(const char* item, String value, bool ret){}
void MQTTSend(const char* item, float value){}
void MQTTConnect() {}
void handleMQTT(){}
void SetupMQTT(){}
void MQTTSetHAUpdateState(bool inProgress, uint8_t progress){}
void MQTTDisconnect(){}

#endif
