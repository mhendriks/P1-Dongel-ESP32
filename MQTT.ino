/* 
***************************************************************************  
**  Program  : MQTTstuff, part of DSMRloggerAPI
**
**  Copyright (c) 2023 Martijn Hendriks 
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

#ifndef MQTT_DISABLE 

String MQTTclientId;

void handleMQTT(){
  MQTTclient.loop();
  if ( bSendMQTT ) sendMQTTData();
}

String AddPayload(const char* key, const char* value ){
  if ( strlen(value) == 0 ) return "";
  return "\"" + String(key) + "\":\"" + String(value) + "\",";
}

void SendAutoDiscoverHA(const char* dev_name, const char* dev_class, const char* dev_title, const char* dev_unit, const char* dev_payload, const char* state_class, const char* extrapl ){
  char msg_topic[80];
  String msg_payload = "{";
  sprintf(msg_topic,"homeassistant/sensor/%s/%s/config",_DEFAULT_HOSTNAME,dev_name);
//    Debugln(msg_topic);
  msg_payload += AddPayload( "uniq_id"        , dev_name);
  msg_payload += AddPayload( "dev_cla"        , dev_class);
  msg_payload += AddPayload( "name"           , dev_title);
  msg_payload += AddPayload( "stat_t"         , String((String)settingMQTTtopTopic + (String)dev_name).c_str() );
  msg_payload += AddPayload( "unit_of_meas"   , dev_unit);
  msg_payload += AddPayload( "val_tpl"        , dev_payload);
  msg_payload += AddPayload( "stat_cla"       , state_class);
  msg_payload += extrapl;
  msg_payload += "\"dev\":{";
  msg_payload += AddPayload("ids"             , String(_getChipId()).c_str() );
  msg_payload += AddPayload("name"            , settingHostname);
  msg_payload += "\"mdl\":\"" _DEFAULT_HOSTNAME "\",\"mf\":\"Smartstuff\"}}";
//  Debugln(msg_payload);
  if (!MQTTclient.publish(msg_topic, msg_payload.c_str(), true) ) DebugTf("Error publish(%s) [%s] [%d bytes]\r\n", msg_topic, msg_payload, ( strlen(msg_topic) + msg_payload.length() ));
}

void AutoDiscoverHA(){
  if (!EnableHAdiscovery) return;
//mosquitto_pub -h 192.168.2.250 -p 1883 -t "homeassistant/sensor/p1-dongle-pro/power_delivered/config" -m '{"uniq_id":"power_delivered","dev_cla":"power","name":"Power Delivered","stat_t":"Eth-Dongle-Pro/power_delivered","unit_of_meas":"W","val_tpl":"{{ value | round(3) * 1000 }}","stat_cla":"measurement","dev":{"ids":"36956260","name":"Eth-Dongle-Pro","mdl":"P1 Dongle Pro","mf":"Smartstuff"}}'
//#ifndef HEATLINK
  if ( ! bWarmteLink ) { // IF NO HEATLINK  
    SendAutoDiscoverHA("timestamp", "timestamp", "DSMR Last Update", "", "{{ strptime(value[:-1] + '-+0200' if value[12] == 'S' else value[:-1] + '-+0100', '%y%m%d%H%M%S-%z') }}","", "\"icon\": \"mdi:clock\",");
    
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
    
    SendAutoDiscoverHA("current_l1", "current", "Current l1", "A", "{{ value | round(0) }}","measurement","");
    SendAutoDiscoverHA("current_l2", "current", "Current l2", "A", "{{ value | round(0) }}","measurement","");
    SendAutoDiscoverHA("current_l3", "current", "Current l3", "A", "{{ value | round(0) }}","measurement","");
  
    SendAutoDiscoverHA("gas_delivered", "gas", "Gas Delivered", "m³", "{{ value | round(3) }}","total_increasing","");
    
    SendAutoDiscoverHA("water", "water", "Waterverbruik", "m³", "{{ value | round(3) }}","total_increasing","\"icon\": \"mdi:water\",");
//#else 
  } else {
  //= HEATLINK
    SendAutoDiscoverHA("timestamp", "timestamp", "Last Update", "", "{{ strptime(value[:-1] + '-+0200' if value[12] == 'S' else value[:-1] + '-+0100', '%y%m%d%H%M%S-%z') }}","", "\"icon\": \"mdi:clock\",");
    SendAutoDiscoverHA("heat_delivered", "energy", "Heat Delivered", "GJ", "{{ value | round(3) }}","total_increasing","");
//#endif  
  }

}

#include "_mqtt_kb.h"

void MQTTSetBaseInfo(){
#ifdef MQTTKB
  sprintf( settingMQTTtopTopic,"%s/%s/", _DEFAULT_HOSTNAME, macID );
#endif  
}

void MQTTsetServer(){
#ifndef MQTT_DISABLE 
  if ((settingMQTTbrokerPort == 0) || (strlen(settingMQTTbroker) < 4) ) return;
  if ( MQTTclient.connected() ) MQTTclient.disconnect();
  MQTTclient.setBufferSize(MQTT_BUFF_MAX);
  DebugTf("setServer(%s, %d) \r\n", settingMQTTbroker, settingMQTTbrokerPort);
  MQTTclient.setServer(settingMQTTbroker, settingMQTTbrokerPort);
#ifdef SMQTT
  wifiClient.setInsecure();
#endif  
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
  
  DynamicJsonDocument doc(3000); 
  DeserializationError error = deserializeJson(doc, payload);
  if (error) return;
  if ( doc.containsKey("broker") && doc.containsKey("port") && doc.containsKey("user") && doc.containsKey("pass") ){
    //test connection
    MQTTSend( "msg", "MQTT: reconfig check connection", true );
    char MqttID[30+13];
    snprintf(MqttID, sizeof(MqttID), "%s-%s", settingHostname, macID);
    if ( MQTTclient.connected() ) MQTTclient.disconnect();
    MQTTclient.setServer(doc["broker"].as<const char*>(), doc["port"].as<uint16_t>());
    if ( MQTTclient.connect( MqttID, doc["user"].as<const char*>(), doc["pass"].as<const char*>() ) ) {
      //adapt the new settings
      DebugTln("MQTT config check succesfull");
      settingMQTTbrokerPort = doc["port"].as<uint16_t>();
      strcpy(settingMQTTbroker, doc["broker"].as<const char*>());
      strcpy(settingMQTTuser, doc["user"].as<const char*>());
      strcpy(settingMQTTpasswd, doc["pass"].as<const char*>());
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
  char StrPayload[len];

  DebugTf("Message length: %d\n",len );
  for (int i=0;i<len;i++) StrPayload[i] = (char)payload[i];
  payload[len] = '\0';
  DebugT("Message arrived [" + StrTopic + "] ");Debugln(StrPayload);

  if ( StrTopic.indexOf("update") >= 0) {
    bUpdateSketch = true;
    strcpy( UpdateVersion, StrPayload );
    DebugT("Message arrived [" + StrTopic + "] ");Debugln(UpdateVersion);
    UpdateRequested = true;
  }
  if ( StrTopic.indexOf("interval") >= 0) {
    settingMQTTinterval =  String(StrPayload).toInt();
    CHANGE_INTERVAL_MS(publishMQTTtimer, 1000 * settingMQTTinterval - 100);
    writeSettings();
  }
  if ( StrTopic.indexOf("reboot") >= 0) {
    P1Reboot();
  }
  if ( StrTopic.indexOf("reconfig") >= 0) {
    MqttReconfig(StrPayload);
  }
}

//===========================================================================================
void MQTTConnect() {
#ifndef ETHERNET
  if ( DUE( reconnectMQTTtimer) && (WiFi.status() == WL_CONNECTED)) {
#else
  if ( DUE( reconnectMQTTtimer) ) {    
#endif    
    char MqttID[30+13];
    snprintf(MqttID, sizeof(MqttID), "%s-%s", settingHostname, macID);
    snprintf( cMsg, 150, "%sLWT", settingMQTTtopTopic );
    DebugTf("connect %s %s %s %s\n", MqttID, settingMQTTuser, settingMQTTpasswd, cMsg);
    
    // if ( MQTTclient.connect( MqttID, settingMQTTuser, settingMQTTpasswd, cMsg, 1, true, "Offline" ) ) {
    if ( MQTTclient.connect( MqttID, settingMQTTuser, settingMQTTpasswd ) ) {
      LogFile("MQTT: Connection to broker: CONNECTED", true);
      MQTTclient.publish(cMsg,"Online", true); //LWT = online
      StaticInfoSend = false; //resend
      MQTTclient.setCallback(MQTTcallback); //set listner update callback
      sprintf( cMsg,"%supdate", settingMQTTtopTopic );
	    MQTTclient.subscribe(cMsg); //subscribe mqtt update
      sprintf(cMsg,"%sinterval",settingMQTTtopTopic);
      MQTTclient.subscribe(cMsg); //subscribe mqtt interval
      sprintf(cMsg,"%sreboot",settingMQTTtopTopic);
      MQTTclient.subscribe(cMsg); //subscribe mqtt reboot
      sprintf(cMsg,"%sreconfig",settingMQTTtopTopic);
      MQTTclient.subscribe(cMsg); //subscribe mqtt reconfig
#ifndef NO_HA_AUTODISCOVERY
      if ( EnableHAdiscovery ) AutoDiscoverHA();
#endif      
    } else {
      LogFile("MQTT: ... connection FAILED! Will try again in 10 sec", true);
      DebugT("error code: ");Debugln(MQTTclient.state());
    }
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
          // Send normal MQTT message when not sending '/all' topic, except when HA auto discovery is on
          if ( !bActJsonMQTT || EnableHAdiscovery) {
            sprintf(cMsg,"%s%s",settingMQTTtopTopic,Name);
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
  if ( value.length()==0 || !MQTTclient.connected() ) return;
  sprintf(cMsg,"%s%s", settingMQTTtopTopic,item);
  if (!MQTTclient.publish(cMsg, value.c_str(), ret )) {
    DebugTf("Error publish (%s) [%s] [%d bytes]\r\n", cMsg, value.c_str(), (strlen(cMsg) + value.length()));
    StaticInfoSend = false; //probeer het later nog een keer
    return;
  }
  if ( strcmp(item,"all") ==0 ) mqttCount++; //counts the number of succesfull all topics send.
}
//===========================================================================================

void MQTTSend(const char* item, float value){
  char temp[10];
  sprintf(temp,"%.3f",value);
  MQTTSend( item, temp, true );
}

//===========================================================================================
void MQTTSentStaticInfo(){
  if ((settingMQTTinterval == 0) || (strlen(settingMQTTbroker) < 4) ) return;
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
  String jsondata = "{\"grid\":{\"power\":";
  jsondata += String( (DSMRdata.power_delivered.int_val() + DSMRdata.power_returned.int_val()) * (DSMRdata.power_returned?-1.0:1.0),0);
  jsondata += ",\"L1\":{\"power\":";
  jsondata += String((DSMRdata.power_delivered_l1.int_val() + DSMRdata.power_returned_l1.int_val()) * (DSMRdata.power_returned_l1?-1.0:1.0),0);
  jsondata += ",\"voltage\":";
  jsondata += String (DSMRdata.voltage_l1,1);
  jsondata += ",\"current\":";
  jsondata += String (DSMRdata.current_l1,0);
  
  jsondata += "},\"L2\":{\"power\":";
  jsondata += String((DSMRdata.power_delivered_l2.int_val() + DSMRdata.power_returned_l2.int_val()) * (DSMRdata.power_returned_l2?-1.0:1.0),0);
  jsondata += ",\"voltage\":";
  jsondata += String (DSMRdata.voltage_l2,1);
  jsondata += ",\"current\":";
  jsondata += String (DSMRdata.current_l2,0);

  jsondata += "},\"L3\":{\"power\":";
  jsondata += String((DSMRdata.power_delivered_l3.int_val() + DSMRdata.power_returned_l3.int_val()) * (DSMRdata.power_returned_l3?-1.0:1.0),0);
  jsondata += ",\"voltage\":";
  jsondata += String (DSMRdata.voltage_l3,1);
  jsondata += ",\"current\":";
  jsondata += String (DSMRdata.current_l3,0);

  jsondata += "}}}";
  
  Debug("Victron jsondata: ");Debugln(jsondata);

  MQTTSend( "victron/grid", jsondata, false);  
}

//===========================================================================================
void sendMQTTData() {

//TODO: log to file on error or reconnect
#ifndef ETHERNET
  if ( WiFi.status() != WL_CONNECTED ) return;
#endif  
  if ( (settingMQTTinterval == 0) || (strlen(settingMQTTbroker) < 4) ) return;
  if (!MQTTclient.connected()) MQTTConnect();
  if ( MQTTclient.connected() ) {   
    
  DebugTf("Sending data to MQTT server [%s]:[%d]\r\n", settingMQTTbroker, settingMQTTbrokerPort);
  
  if ( !StaticInfoSend )  MQTTSentStaticInfo();
    
  fieldsElements = ACTUALELEMENTS;
  
#ifdef MQTTKB  
  bActJsonMQTT = true;
  EnableHAdiscovery = false;
#endif
  
  if ( bActJsonMQTT ) jsonDoc.clear();

  DSMRdata.applyEach(buildJsonMQTT());
  
  if ( bActJsonMQTT ) {
    String buffer;
    if ( gasDelivered ) {
      jsonDoc["gas"] = gasDelivered;
      jsonDoc["gas_ts"] = gasDeliveredTimestamp;
    }
    serializeJson(jsonDoc,buffer);
    MQTTSend("all", buffer, false);
  } else {
	  if ( DSMRdata.highest_peak_pwr_present ) MQTTSend( "highest_peak_pwr_ts", String(DSMRdata.highest_peak_pwr.timestamp), true);

	  MQTTsendGas();
	  MQTTsendWater();
  }  

#ifdef VICTRON_GRID
  MQTTSendVictronData();
#endif

  bSendMQTT = false; 

  }
}

#else

void MQTTSentStaticInfo(){}
void MQTTSend(const char* item, String value, bool ret){}
void MQTTSend(const char* item, float value){}
void MQTTConnect() {}
void handleMQTT(){}
void SetupMQTT(){}

#endif

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
****************************************************************************
*/
