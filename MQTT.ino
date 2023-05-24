  static IPAddress    MQTTbrokerIP;
  static char         MQTTbrokerIPchar[20];
  int8_t              reconnectAttempts = 0;
  String              MQTTclientId;

void SendAutoDiscoverHA(const char* dev_name, const char* dev_class, const char* dev_title, const char* dev_unit, const char* dev_payload, const char* state_class, const char* extrapl ){
  char msg_topic[80];
  char msg_payload[500];
    sprintf(msg_topic,"homeassistant/sensor/p1-dongle-pro/%s/config",dev_name);
//    Debugln(msg_topic);
  if (strlen(dev_class)) sprintf(msg_payload,"{\"uniq_id\":\"%s\",\"dev_cla\": \"%s\",\"name\": \"%s\", \"stat_t\": \"%s%s\", \"unit_of_meas\": \"%s\", \"val_tpl\": \"%s\", \"state_class\":\"%s\"%s }", dev_name,dev_class, dev_title, settingMQTTtopTopic, dev_name, dev_unit, dev_payload,state_class, extrapl);
  else sprintf(msg_payload,"{\"uniq_id\":\"%s\",\"name\": \"%s\", \"stat_t\": \"%s%s\", \"unit_of_meas\": \"%s\", \"val_tpl\": \"%s\", \"state_class\":\"%s\"%s }", dev_name, dev_title, settingMQTTtopTopic, dev_name, dev_unit, dev_payload,state_class, extrapl);
//    Debugln(msg_payload);
  if (!MQTTclient.publish(msg_topic, msg_payload, true) ) DebugTf("Error publish(%s) [%s] [%d bytes]\r\n", msg_topic, msg_payload, (strlen(msg_topic) + strlen(msg_payload)));
}

void AutoDiscoverHA(){
#ifndef EVERGI  
  if (!EnableHAdiscovery) return;
//mosquitto_pub -h 192.168.2.250 -p 1883 -t "homeassistant/sensor/power_delivered/config" -m '{"dev_cla": "gas", "name": "Power Delivered", "stat_t": "DSMR-API/power_delivered", "unit_of_meas": "Wh", "val_tpl": "{{ value_json.power_delivered[0].value | round(3) }}" }'

  SendAutoDiscoverHA("timestamp", "", "DSMR Last Update", "", "{{ strptime(value[0:12], \'%y%m%d%H%M%S\') }}","measurement", ",\"icon\": \"mdi:clock\"");
  
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

  SendAutoDiscoverHA("voltage_l1", "voltage", "Voltage l1", "V", "{{ value | round(0) }}","measurement","");
  SendAutoDiscoverHA("voltage_l2", "voltage", "Voltage l2", "V", "{{ value | round(0) }}","measurement","");
  SendAutoDiscoverHA("voltage_l3", "voltage", "Voltage l3", "V", "{{ value | round(0) }}","measurement","");
  
  SendAutoDiscoverHA("current_l1", "current", "Current l1", "A", "{{ value | round(0) }}","measurement","");
  SendAutoDiscoverHA("current_l2", "current", "Current l2", "A", "{{ value | round(0) }}","measurement","");
  SendAutoDiscoverHA("current_l3", "current", "Current l3", "A", "{{ value | round(0) }}","measurement","");

  SendAutoDiscoverHA("gas_delivered", "gas", "Gas Delivered", "m³", "{{ value | round(2) }}","total_increasing","");
  
  SendAutoDiscoverHA("water", "water", "Waterverbruik", "m³", "{{ value | round(3) }}","total_increasing",",\"icon\": \"mdi:water\"");
#endif
}

//===========================================================================================
static void MQTTcallback(char* topic, byte* payload, unsigned int length) {
  if (length > 24) return;
  sprintf(cMsg,"%supdatefs",settingMQTTtopTopic);
  if (strcmp(topic, cMsg) == 0) bUpdateSketch = false; 
  else bUpdateSketch = true;
  
  for (int i=0;i<length;i++) UpdateVersion[i] = (char)payload[i];
  DebugT("Message arrived [");Debug(topic);Debug("] ");Debugln(UpdateVersion);
  UpdateRequested = true;
}

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
     strncpy(Name,String(Item::name).c_str(),25);
    if ( isInFieldsArray(Name) && i.present() ) {
#ifdef EVERGI      
          jsonDoc[Name] = value_to_json_mqtt(i.val());
#else 
          if ( bActJsonMQTT ) jsonDoc[Name] = value_to_json_mqtt(i.val());
          else {
          sprintf(cMsg,"%s%s",settingMQTTtopTopic,Name);
          MQTTclient.publish( cMsg, String(value_to_json(i.val())).c_str() );
//        if ( !MQTTclient.publish( cMsg, String(value_to_json(i.val())).c_str() ) ) DebugTf("Error publish(%s)\r\n", String(value_to_json(i.val())), strlen(cMsg) );
          }
#endif          
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

void MQTTSend(const char* item, String value){
  if (value.length()==0) return;
  sprintf(cMsg,"%s%s", settingMQTTtopTopic,item);
  if (!MQTTclient.publish(cMsg, value.c_str(),true )) {
    DebugTf("Error publish (%s) [%s] [%d bytes]\r\n", cMsg, value.c_str(), (strlen(cMsg) + value.length()));
    StaticInfoSend = false; //probeer het later nog een keer
  }
}
//===========================================================================================

void MQTTSend(const char* item, float value){
  char temp[10];
  sprintf(temp,"%.3f",value);
  MQTTSend( item, temp );
}

//===========================================================================================
void MQTTSentStaticInfo(){
  if ((settingMQTTinterval == 0) || (strlen(settingMQTTbroker) < 4) ) return;
  StaticInfoSend = true;
  MQTTSend( "identification",DSMRdata.identification );
  MQTTSend( "mac",String(WiFi.macAddress()) );
  MQTTSend( "p1_version",DSMRdata.p1_version );
  MQTTSend( "equipment_id",DSMRdata.equipment_id );
  MQTTSend( "firmware",_VERSION_ONLY );
  MQTTSend( "ip_address",WiFi.localIP().toString());
  MQTTSend( "wifi_rssi",String( WiFi.RSSI() ) );
  if (DSMRdata.mbus1_device_type_present){ MQTTSend("gas_device_type", String(DSMRdata.mbus1_device_type) ); }
  if (DSMRdata.mbus1_equipment_id_tc_present){ MQTTSend("gas_equipment_id",DSMRdata.mbus1_equipment_id_tc); }  
}

//===========================================================================================
void MQTTsendGas(){
  if (!gasDelivered) return;
#ifdef HEATLINK
  MQTTSend( "heat_delivered", gasDelivered );
//  MQTTSend( "heat_delivered_timestamp", gasDeliveredTimestamp ); // 2x: because device timestamp is equal
#else
  MQTTSend( "gas_delivered", gasDelivered );
  MQTTSend( "gas_delivered_timestamp", gasDeliveredTimestamp );
#endif
}

//===========================================================================================
void MQTTConnectEV() {
  //try every 5 sec
  if ( DUE( reconnectMQTTtimer) ){ 
    snprintf( cMsg, 150, "%sLWT", settingMQTTtopTopic );
    if ( MQTTclient.connect( MqttID, EVERGI_USER, EVERGI_TOKEN, cMsg, 1, true, "Offline" ) ) {
//      reconnectAttempts = 0;  
      LogFile("MQTT: Attempting connection... connected", true);
      MQTTclient.publish(cMsg,"Online", true); //LWT = online
      StaticInfoSend = false; //resend
//      MQTTclient.setCallback(MQTTcallback); //set listner update callback
    } else {
      LogFile("MQTT: Attempting connection... connection FAILED", true);
    }
  CHANGE_INTERVAL_SEC(reconnectMQTTtimer, 10);
  }  
}

//===========================================================================================
void sendMQTTDataEV() {

//TODO: log to file on error or reconnect
  
    if (!MQTTclient.connected()) MQTTConnectEV();
    if ( MQTTclient.connected() ) {         
     DebugTf("Sending data to MQTT server [%s]:[%d]\r\n", settingMQTTbroker, settingMQTTbrokerPort);
  
  if ( !StaticInfoSend )  { 
#ifndef EVERGI    
    MQTTSentStaticInfo(); 
#else    
    MQTTSentStaticInfoEvergi(); 
#endif
  }
    
  fieldsElements = ACTUALELEMENTS;
  
  jsonDoc.clear();

  DSMRdata.applyEach(buildJsonMQTT());
  //check if gas and peak is available
  if ( gasDelivered ) {
    jsonDoc["gas"] = gasDelivered;
    jsonDoc["gas_ts"] = gasDeliveredTimestamp;
  }
  if ( DSMRdata.highest_peak_pwr_present ) jsonDoc["highest_peak_pwr_ts"] = DSMRdata.highest_peak_pwr.timestamp;
  String buffer;
  serializeJson(jsonDoc,buffer);
  MQTTSend("grid",buffer);
  } 
}
