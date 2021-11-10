/* 
***************************************************************************  
**  Program  : MQTTstuff, part of DSMRloggerAPI
**  Version  : v3.0.0
**
**  Copyright (c) 2021 Martijn Hendriks / based on DSMR Api Willem Aandewiel
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

void MQTTcallback(char* topic, byte* payload, unsigned int length) {
  if (length > 24) return;
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
        LogFile("MQTT Starting");
        DebugTln(F("MQTT State: MQTT Initializing"));
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
            LogFile("MQTT Error : not connected");
            return false;
          }
          
          //MQTTclient.disconnect();
          //DebugTf("disconnect -> MQTT status, rc=%d \r\n", MQTTclient.state());
          DebugTf("[%s] => setServer(%s, %d) \r\n", settingMQTTbroker, MQTTbrokerIPchar, settingMQTTbrokerPort);
          MQTTclient.setServer(MQTTbrokerIPchar, settingMQTTbrokerPort);
          DebugTf("setServer  -> MQTT status, rc=%d \r\n", MQTTclient.state());
          MQTTclientId  = String(settingHostname) + "-" + WiFi.macAddress();
          MQTTclient.setCallback(MQTTcallback); //set listner update callback
          stateMQTT = MQTT_STATE_TRY_TO_CONNECT;
          reconnectAttempts = 0;
          }
    case MQTT_STATE_TRY_TO_CONNECT:
          DebugTln(F("MQTT State: MQTT try to connect"));
          DebugTf("MQTT server is [%s], IP[%s]\r\n", settingMQTTbroker, MQTTbrokerIPchar);
          LogFile("MQTT State: MQTT try to connect");
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
            LogFile("MQTT connected");
            Debugf(" .. connected -> MQTT status, rc=%d\r\n", MQTTclient.state());
            MQTTclient.publish(cMsg,"Online", true);
            sprintf(cMsg,"%supdate",settingMQTTtopTopic);
            MQTTclient.subscribe(cMsg); //subscribe mqtt update
            LogFile("MQTT connected");
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
            DebugTln(F("Next State: MQTT_STATE_ERROR"));
          }   
          break;
          
    case MQTT_STATE_IS_CONNECTED:
          LogFile("MQTT connected");
          MQTTclient.loop();
          return true;

    case MQTT_STATE_ERROR:
          DebugTln(F("MQTT State: MQTT ERROR, wait for 5 sec, before trying again"));
          LogFile("MQTT Error: retry in 5 seconds");
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
    String msg;
    
    template<typename Item>
    void apply(Item &i) {
    String Name = (String)Item::name;
    if (!isInFieldsArray(Name.c_str()) ) {
      if (i.present()) {
        sprintf(cMsg,"%s%s",settingMQTTtopTopic,Name.c_str());
        if (strlen(Item::unit()) > 0) msg = "{\""+Name+"\":[{\"value\":"+value_to_json(i.val())+",\"unit\":\""+Item::unit()+"\"}]}";
        else msg = "{\""+Name+"\":[{\"value\":"+value_to_json(i.val())+"}]}";
        if (Verbose2) DebugTln("mqtt bericht: "+msg);
        if ( !MQTTclient.publish(cMsg, msg.c_str()) ) DebugTf("Error publish(%s) [%s] [%d bytes]\r\n", cMsg, msg.c_str(), (strlen(cMsg) + msg.length()));
      } // if i.present
    } // if isInFieldsArray
  } //apply
  
  template<typename Item>
  Item& value_to_json(Item& i) {
    return i;
  }

  String value_to_json( String i){
    return "\"" + i+ "\"";
  }
  
}; // buildJsonMQTT

//===========================================================================================

void MQTTSend(const char* item, String value){
  String msg = "{\"" + String(item) + "\":[{\"value\":\""+ value + "\"}]}";
  sprintf(cMsg,"%s%s", settingMQTTtopTopic,item);
  if (!MQTTclient.publish(cMsg, (byte*)msg.c_str(),msg.length(),true )) {
    DebugTf("Error publish (%s) [%s] [%d bytes]\r\n", cMsg, msg.c_str(), (strlen(cMsg) + msg.length()));
    StaticInfoSend = false; //probeer het later nog een keer
  }
}

void MQTTSend(const char* item, int32_t value){
  String msg = "{\"" + String(item) + "\":[{\"value\":"+ value + "}]}";
  sprintf(cMsg,"%s%s", settingMQTTtopTopic,item);
  if (!MQTTclient.publish(cMsg, (byte*)msg.c_str(),msg.length(),true )) {
    DebugTf("Error publish (%s) [%s] [%d bytes]\r\n", cMsg, msg.c_str(), (strlen(cMsg) + msg.length()));
    StaticInfoSend = false; //probeer het later nog een keer
  }
}
//===========================================================================================
void MQTTSentStaticP1Info(){
  
  StaticInfoSend = true;
  MQTTSend("identification",DSMRdata.identification);
  MQTTSend("p1_version",DSMRdata.p1_version);
  MQTTSend("equipment_id",DSMRdata.equipment_id);
  MQTTSend("firmware",_VERSION_ONLY);
  MQTTSend("ip_address",WiFi.localIP().toString());
  if (DSMRdata.mbus1_device_type_present){ MQTTSend("gas_device_type", (uint32_t)DSMRdata.mbus1_device_type ); }
  if (DSMRdata.mbus1_equipment_id_tc_present){ MQTTSend("gas_equipment_id",DSMRdata.mbus1_equipment_id_tc); }
  
}

//---------------------------------------------------------------
void MQTTSentStaticDevInfo(){
  
  MQTTSend( "wifi_rssi",WiFi.RSSI() );
  
}

//---------------------------------------------------------------
void MQTTsendGas(){

  if (gasDelivered){
    sprintf(cMsg,"%s%s",settingMQTTtopTopic,"gas_delivered");
    char msg[60];
    sprintf(msg,"{\"gas_delivered\":[{\"value\":%.3f,\"unit\":\"m3\"}]}",gasDelivered);
    if (!MQTTclient.publish(cMsg, msg) ) DebugTf("Error publish(%s) [%s] [%d bytes]\r\n", cMsg, msg, (strlen(cMsg) + strlen(msg)));
  }
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
  if ((telegramCount - telegramErrors) > 2 && !StaticInfoSend){
    MQTTSentStaticP1Info();
    MQTTSentStaticDevInfo();
  }
  fieldsElements = INFOELEMENTS;  
  DSMRdata.applyEach(buildJsonMQTT());
  MQTTsendGas();

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
