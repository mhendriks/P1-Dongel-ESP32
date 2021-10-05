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

  static IPAddress  MQTTbrokerIP;
  static char       MQTTbrokerIPchar[20];

#ifdef USE_MQTT

  int8_t              reconnectAttempts = 0;
  char                lastMQTTtimestamp[15] = "-";
  char                mqttBuff[100];


  enum states_of_MQTT { MQTT_STATE_INIT, MQTT_STATE_TRY_TO_CONNECT, MQTT_STATE_IS_CONNECTED, MQTT_STATE_ERROR };
  enum states_of_MQTT stateMQTT = MQTT_STATE_INIT;

  String            MQTTclientId;

#endif

//===========================================================================================
void connectMQTT() 
{
#ifdef USE_MQTT
  
  if (Verbose2) DebugTf("MQTTclient.connected(%d), mqttIsConnected[%d], stateMQTT [%d]\r\n"
                                              , MQTTclient.connected()
                                              , mqttIsConnected, stateMQTT);

  if (settingMQTTinterval == 0) {
    mqttIsConnected = false;
    return;
  }

  if (!MQTTclient.connected() || stateMQTT != MQTT_STATE_IS_CONNECTED)
  {
    mqttIsConnected = false;
    stateMQTT = MQTT_STATE_INIT;
  }

  mqttIsConnected = connectMQTT_FSM();
  
  if (Verbose1) DebugTf("connected()[%d], mqttIsConnected[%d], stateMQTT [%d]\r\n"
                                              , MQTTclient.connected()
                                              , mqttIsConnected, stateMQTT);

  CHANGE_INTERVAL_SEC(reconnectMQTTtimer, 5);

#endif
}

//===========================================================================================
bool connectMQTT_FSM() 
{
#ifdef USE_MQTT
  
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
            DebugTln(F("Next State: MQTT_STATE_ERROR"));
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
          stateMQTT = MQTT_STATE_TRY_TO_CONNECT;
          DebugTln(F("Next State: MQTT_STATE_TRY_TO_CONNECT"));
          reconnectAttempts = 0;
          }
    case MQTT_STATE_TRY_TO_CONNECT:
          DebugTln(F("MQTT State: MQTT try to connect"));
          DebugTf("MQTT server is [%s], IP[%s]\r\n", settingMQTTbroker, MQTTbrokerIPchar);
          LogFile("MQTT State: MQTT try to connect");
          DebugTf("Attempting MQTT connection as [%s] .. \r\n", MQTTclientId.c_str());
          reconnectAttempts++;

          //--- If no username, then anonymous connection to broker, otherwise assume username/password.
          if (String(settingMQTTuser).length() == 0) 
          {
            DebugT(F("without a Username/Password "));
            MQTTclient.connect(MQTTclientId.c_str());
          } 
          else 
          {
            DebugTf("with Username [%s] and password ", settingMQTTuser);
            MQTTclient.connect(MQTTclientId.c_str(), settingMQTTuser, settingMQTTpasswd);
          }
          //--- If connection was made succesful, move on to next state...
          if (MQTTclient.connected())
          {
            reconnectAttempts = 0;  
            LogFile("MQTT connected");
            Debugf(" .. connected -> MQTT status, rc=%d\r\n", MQTTclient.state());
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
          DebugTln(F("MQTT State: MQTT ERROR, wait for 10 minutes, before trying again"));
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
#endif

  return false;  

} // connectMQTT_FSM()

//=======================================================================

struct buildJsonMQTT {
#ifdef USE_MQTT

  char topicId[100];
  StaticJsonDocument<125> doc;  

    template<typename Item>
    void apply(Item &i) {
      if (i.present()) 
      {
        String Name = String(Item::name);
        String Unit = Item::unit();
        strcpy(topicId,settingMQTTtopTopic);
        strConcat(topicId, sizeof(topicId), Name.c_str());
        if (Verbose2) DebugTf("topicId[%s]\r\n", topicId);
        doc.clear();
        JsonObject nested = doc[Name].createNestedObject();
        if (Unit.length() > 0) {
          nested["value"] = value_to_json(i.val());
          nested["unit"] = Unit;
        }
        else nested["value"] = value_to_json(i.val());
        serializeJson(doc, mqttBuff);
        if (!MQTTclient.publish(topicId, mqttBuff) ) DebugTf("Error publish(%s) [%s] [%d bytes]\r\n", topicId, mqttBuff, (strlen(topicId) + strlen(mqttBuff)));
      } // if i.present
  } //apply
  
  template<typename Item>
  Item& value_to_json(Item& i) {
    return i;
  }

  String value_to_json(TimestampedFixedValue i) {
    return String(i);
  }
  
  float value_to_json(FixedValue i) {
    return i;
  }
#endif

}; // buildJsonMQTT

//===========================================================================================
void sendMQTTData() 
{
#ifdef USE_MQTT
  String dateTime, topicId, json;

  if ((settingMQTTinterval == 0) || bailout() ) return;

  if (!MQTTclient.connected() || ! mqttIsConnected)
  {
    DebugTf("MQTTclient.connected(%d), mqttIsConnected[%d], stateMQTT [%d]\r\n"
                                              , MQTTclient.connected()
                                              , mqttIsConnected, stateMQTT);
  }
  if (!MQTTclient.connected())  
  {
    if ( DUE( reconnectMQTTtimer) || mqttIsConnected)
    {
      mqttIsConnected = false;
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
  
  DSMRdata.applyEach(buildJsonMQTT());

#endif

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
