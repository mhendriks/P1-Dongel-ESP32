/* 
***************************************************************************  
**  Program  : MQTTstuff, part of DSMRloggerAPI
**  Version  : v4.2.1
**
**  Copyright (c) 2023 Martijn Hendriks / based on DSMR Api Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
**  RvdB  changed MQTT stuff to FSM 
**  AaW   simplified and restructured the FSM 
*/

// Declare some variables within global scope

  static IPAddress    MQTTbrokerIP;
  static char         MQTTbrokerIPchar[20];
  int8_t              reconnectAttempts = 0;
  char                lastMQTTtimestamp[15] = "-";
  char                mqttBuff[100];

  enum states_of_MQTT { MQTT_STATE_INIT, MQTT_STATE_TRY_TO_CONNECT, MQTT_STATE_IS_CONNECTED, MQTT_STATE_ERROR };
  enum states_of_MQTT stateMQTT = MQTT_STATE_INIT;

  String            MQTTclientId;

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

}

//===========================================================================================
void connectMQTT() {
  
  if (Verbose2) DebugTf("MQTTclient.connected(%d), mqttIsConnected[%d], stateMQTT [%d]\r\n", MQTTclient.connected(), mqttIsConnected, stateMQTT);

  if ( (settingMQTTinterval == 0) || (strlen(settingMQTTbroker) < 4) ) {
    mqttIsConnected = false;
    return;
  }

  if (!MQTTclient.connected() || stateMQTT != MQTT_STATE_IS_CONNECTED)
  {
    mqttIsConnected = false;
    stateMQTT = MQTT_STATE_INIT;
  }

  mqttIsConnected = connectMQTT_FSM();
  
  if (Verbose1) DebugTf("connected()[%d], mqttIsConnected[%d], stateMQTT [%d]\r\n", MQTTclient.connected(), mqttIsConnected, stateMQTT);

  CHANGE_INTERVAL_SEC(reconnectMQTTtimer, 5);

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

//===========================================================================================
bool connectMQTT_FSM() 
{  
  switch(stateMQTT) 
  {
    case MQTT_STATE_INIT:  
          {
        LogFile("MQTT Starting",true);
//        DebugTln(F("MQTT State: MQTT Initializing"));
        MQTTbrokerIP = MDNS.queryHost(settingMQTTbroker);
        if (isValidIP(MQTTbrokerIP)) {
          DebugT("queryHost: IP address of server: ");Debugln(MQTTbrokerIP.toString());
        } else {
          DebugTln(F("queryHost MQTT failed"));
//          WiFi.hostByName(settingMQTTbroker, MQTTbrokerIP);  // try to connect directly with ip
            if (MQTTbrokerIP.fromString(settingMQTTbroker)) DebugTln(F("MQTT ip setting = valid ip-address"));
        }
 
          snprintf(MQTTbrokerIPchar, sizeof(MQTTbrokerIPchar), "%d.%d.%d.%d", MQTTbrokerIP[0], MQTTbrokerIP[1], MQTTbrokerIP[2], MQTTbrokerIP[3]);
          if (!isValidIP(MQTTbrokerIP))  
          {
            DebugTf("ERROR: [%s] => is not a valid URL\r\n", settingMQTTbroker);
            settingMQTTinterval = 0;
            stateMQTT = MQTT_STATE_ERROR;
            LogFile("MQTT Error : not connected",false);
            return false;
          }
          
          //MQTTclient.disconnect();
          //DebugTf("disconnect -> MQTT status, rc=%d \r\n", MQTTclient.state());
          DebugTf("[%s] => setServer(%s, %d) \r\n", settingMQTTbroker, MQTTbrokerIPchar, settingMQTTbrokerPort);
          MQTTclient.setServer(MQTTbrokerIPchar, settingMQTTbrokerPort);
          DebugTf("setServer  -> MQTT status, rc=%d \r\n", MQTTclient.state());
          MQTTclientId  = String(settingHostname) + "-" + WiFi.macAddress();
          stateMQTT = MQTT_STATE_TRY_TO_CONNECT;
          reconnectAttempts = 0;
          }
    case MQTT_STATE_TRY_TO_CONNECT:
//          DebugTln(F("MQTT State: MQTT try to connect"));
          LogFile("MQTT State: MQTT try to connect",false);
          DebugTf("MQTT server is [%s], IP[%s]\r\n", settingMQTTbroker, MQTTbrokerIPchar);
          
          DebugTf("Attempting MQTT connection as [%s] .. \r\n", MQTTclientId.c_str());
          reconnectAttempts++;

          //--- If no username, then anonymous connection to broker, otherwise assume username/password.
          sprintf(cMsg,"%sLWT",settingMQTTtopTopic);
          if (String(settingMQTTuser).length() == 0) 
          {
            DebugT(F("without a Username/Password "));
            MQTTclient.connect(MQTTclientId.c_str(),"","",cMsg,1,true,"Offline");
          } 
          else 
          {
            DebugTf("with Username [%s] and password ", settingMQTTuser);
            MQTTclient.connect(MQTTclientId.c_str(), settingMQTTuser, settingMQTTpasswd,cMsg,1,true,"Offline");
          }
          //--- If connection was made succesful, move on to next state...
          if (MQTTclient.connected())
          {
            reconnectAttempts = 0;  
            Debugf(" .. connected -> MQTT status, rc=%d\r\n", MQTTclient.state());

            AutoDiscoverHA();

            //subscribe mqtt update topics
            MQTTclient.publish(cMsg,"Online", true);
            MQTTclient.setCallback(MQTTcallback); //set listner update callback
            sprintf(cMsg,"%supdate",settingMQTTtopTopic);
            MQTTclient.subscribe(cMsg); //subscribe mqtt update
            sprintf(cMsg,"%supdatefs",settingMQTTtopTopic);
            MQTTclient.subscribe(cMsg); //subscribe mqtt update

            LogFile("MQTT connected",false);
            MQTTclient.loop();
            stateMQTT = MQTT_STATE_IS_CONNECTED;
            return true;
          }
          Debugf(" -> MQTT status, rc=%d \r\n", MQTTclient.state());
      
          //--- After 3 attempts... go wait for a while.
          if (reconnectAttempts >= 3)
          {
            DebugTln(F("3 attempts have failed. Retry wait for next reconnect in 10 minutes\r"));
            stateMQTT = MQTT_STATE_ERROR;  // if the re-connect did not work, then return to wait for reconnect
          }   
          break;
          
    case MQTT_STATE_IS_CONNECTED:
          LogFile("MQTT connected",false);
          MQTTclient.loop();
          return true;

    case MQTT_STATE_ERROR:
//          DebugTln(F("MQTT State: MQTT ERROR, wait for 5 sec, before trying again"));
          LogFile("MQTT Error: retry after 5 seconds",true);
          //--- next retry in 10 minutes.
          CHANGE_INTERVAL_SEC(reconnectMQTTtimer, 5);
          break;
    default:
          DebugTln(F("MQTT State: default, this should NEVER happen!"));
          //--- do nothing, this state should not happen
          stateMQTT = MQTT_STATE_INIT;
          CHANGE_INTERVAL_SEC(reconnectMQTTtimer, 5);
          DebugTln(F("Next State: MQTT_STATE_INIT"));
          break;
  }

  return false; 
  
} // connectMQTT_FSM()

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
          if ( bActJsonMQTT ) jsonDoc[Name] = value_to_json_mqtt(i.val());
          else {
          sprintf(cMsg,"%s%s",settingMQTTtopTopic,Name);
          MQTTclient.publish( cMsg, String(value_to_json(i.val())).c_str() );
//        if ( !MQTTclient.publish( cMsg, String(value_to_json(i.val())).c_str() ) ) DebugTf("Error publish(%s)\r\n", String(value_to_json(i.val())), strlen(cMsg) );
          }
    } // if isInFieldsArray && present
  } //apply

  template<typename Item>
  Item& value_to_json(Item& i) {
    return i;
  }

  bool value_to_json(TimestampedFixedValue i) {
    //wordt niet aangeroepen ivm niet uitvragen van dit type
    return i;
  }
  
  String value_to_json(FixedValue i) {
    return String(i,3);
  }

  template<typename Item>
  Item& value_to_json_mqtt(Item& i) {
    return i;
  }

  bool value_to_json_mqtt(TimestampedFixedValue i) {
    //wordt niet aangeroepen ivm niet uitvragen van dit type
    return 0;
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

//---------------------------------------------------------------
void MQTTsendGas(){
  if (!gasDelivered) return;
#ifdef HEATLINK
  MQTTSend( "heat_delivered", gasDelivered );
//  MQTTSend( "heat_delivered_timestamp", gasDeliveredTimestamp ); // double: because device timestamp is equal
#else
  MQTTSend( "gas_delivered", gasDelivered );
  MQTTSend( "gas_delivered_timestamp", gasDeliveredTimestamp );
#endif
}

//---------------------------------------------------------------
void sendMQTTData() 
{
//  String dateTime, topicId, json;

  if ((settingMQTTinterval == 0) || (strlen(settingMQTTbroker) < 4) || bailout() ) return;

  //make proper TopTopic
  if (settingMQTTtopTopic[strlen(settingMQTTtopTopic)-1] != '/') snprintf(settingMQTTtopTopic, sizeof(settingMQTTtopTopic), "%s/",  settingMQTTtopTopic);
  
  if (MQTTclient.connected() && !mqttIsConnected) {
    sprintf(cMsg,"%sLWT",settingMQTTtopTopic);
    MQTTclient.publish(cMsg,"Offline", true); //LWT status update
    MQTTclient.disconnect(); //disconnect when connection is not allowed
  }
  
  if (!MQTTclient.connected() || !mqttIsConnected) DebugTf("MQTTclient.connected(%d), mqttIsConnected[%d], stateMQTT [%d]\r\n", MQTTclient.connected(), mqttIsConnected, stateMQTT);

  if (!MQTTclient.connected())  
  {
    if ( DUE( reconnectMQTTtimer) || mqttIsConnected)
    {
      mqttIsConnected = false;
      StaticInfoSend = false; //zorg voor resend retained info
      connectMQTT();
    }
    else
    {
      DebugTf("trying to reconnect in less than %d seconds\r\n", (TIME_LEFT_SEC(reconnectMQTTtimer) +1) );
    }
    if ( !mqttIsConnected ) 
    {
      DebugTln(F("no connection with a MQTT broker .."));
      return;
    }
  }

  DebugTf("Sending data to MQTT server [%s]:[%d]\r\n", settingMQTTbroker, settingMQTTbrokerPort);
  if (!StaticInfoSend)  MQTTSentStaticInfo();
  fieldsElements = ACTUALELEMENTS;
  
  if ( bActJsonMQTT ) jsonDoc.clear();
  
  DSMRdata.applyEach(buildJsonMQTT());
  
  if ( bActJsonMQTT ) {
    String buffer;
    serializeJson(jsonDoc,buffer);
    MQTTSend("all",buffer);
  }
  
  MQTTsendGas();
  sendMQTTWater();


} // sendMQTTData()

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
