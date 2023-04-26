#ifdef BB

 static const char *root_ca PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----

-----END CERTIFICATE-----
)EOF";
    
  void EvergiMQTTSettings(){
    snprintf(StrMac,sizeof(StrMac),"%c%c%c%c%c%c%c%c%c%c%c%c",WiFi.macAddress()[0],WiFi.macAddress()[1],WiFi.macAddress()[3],WiFi.macAddress()[4],WiFi.macAddress()[6],WiFi.macAddress()[7],WiFi.macAddress()[9],WiFi.macAddress()[10],WiFi.macAddress()[12],WiFi.macAddress()[13],WiFi.macAddress()[15],WiFi.macAddress()[16]);
    snprintf(settingMQTTtopTopic,sizeof(settingMQTTtopTopic),"%s/",BB_MQTT_TOPIC);
    strncpy( settingMQTTbroker , BB_HOST  , sizeof(settingMQTTbroker) );
    strncpy( BaseOTAurl        , BB_OTA   ,  sizeof(BaseOTAurl) );
    settingMQTTbrokerPort = BB_PORT;
    EnableHAdiscovery = false;
    settingMQTTinterval = BB_INTERVAL;
    CHANGE_INTERVAL_SEC(publishMQTTtimer, settingMQTTinterval);
    bActJsonMQTT = true;
    wifiClient.setInsecure();
//    wifiClient.setCACert(root_ca);
    DebugTf("[%s] => setServer(%s, %d) \r\n", settingMQTTbroker, settingMQTTbroker, settingMQTTbrokerPort);
    MQTTclient.setServer(BB_HOST, BB_PORT);
    snprintf(MqttID,sizeof(MqttID),"%s_%s",BB_USER,"testDongle");
  }
    
void MQTTSentStaticInfoEvergi(){
  StaticInfoSend = true;
  jsonDoc.clear();
  jsonDoc["id"] = DSMRdata.identification;
  jsonDoc["p1_version"] = DSMRdata.p1_version;
  jsonDoc["sm_id"] = DSMRdata.equipment_id;
  jsonDoc["firmware"] = _VERSION_ONLY;
  jsonDoc["ip_address"] = WiFi.localIP().toString();
  jsonDoc["wifi_rssi"] = String( WiFi.RSSI() );
  jsonDoc["gas_id"] = DSMRdata.mbus1_equipment_id_tc; // empty when nonexisting 
  jsonDoc["reboot"] = P1Status.reboots;
  String buffer;
  serializeJson(jsonDoc,buffer);
  MQTTSend( "static", buffer );
}


//------ TEST
void sendMQTTDataEV_test() {
    if (!MQTTclient.connected()) MQTTConnectEV();
    else {
      DebugTf("Sending data to MQTT server [%s]:[%d]\r\n", settingMQTTbroker, settingMQTTbrokerPort);
      if ( !StaticInfoSend ) MQTTSend("static","{'id':'KFM5KAIFA-METER','p1_version':'50','sm_id':'4530303630313030303035303738373232','firmware':'4.8.1','ip_address':'192.168.2.6','wifi_rssi':'-81','gas_id':'4730303339303031373532383339333137'}");
      MQTTSend("grid","{'timestamp':'230407075201S','energy_delivered_tariff1':3346.52,'energy_delivered_tariff2':4001.951,'energy_returned_tariff1':0,'energy_returned_tariff2':0,'electricity_tariff':'0002','power_delivered':0.291,'power_returned':0,'voltage_l1':232.5,'voltage_l2':232.7,'voltage_l3':230.6,'current_l1':0,'current_l2':0,'current_l3':1,'power_delivered_l1':0.002,'power_delivered_l2':0.062,'power_delivered_l3':0.226,'power_returned_l1':0,'power_returned_l2':0,'power_returned_l3':0}");
    }
}
    
#endif
