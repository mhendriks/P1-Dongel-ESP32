#ifdef EVERGI

 static const char *root_ca PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIG1TCCBL2gAwIBAgIQbFWr29AHksedBwzYEZ7WvzANBgkqhkiG9w0BAQwFADCB
iDELMAkGA1UEBhMCVVMxEzARBgNVBAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0pl
cnNleSBDaXR5MR4wHAYDVQQKExVUaGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNV
BAMTJVVTRVJUcnVzdCBSU0EgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMjAw
MTMwMDAwMDAwWhcNMzAwMTI5MjM1OTU5WjBLMQswCQYDVQQGEwJBVDEQMA4GA1UE
ChMHWmVyb1NTTDEqMCgGA1UEAxMhWmVyb1NTTCBSU0EgRG9tYWluIFNlY3VyZSBT
aXRlIENBMIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEAhmlzfqO1Mdgj
4W3dpBPTVBX1AuvcAyG1fl0dUnw/MeueCWzRWTheZ35LVo91kLI3DDVaZKW+TBAs
JBjEbYmMwcWSTWYCg5334SF0+ctDAsFxsX+rTDh9kSrG/4mp6OShubLaEIUJiZo4
t873TuSd0Wj5DWt3DtpAG8T35l/v+xrN8ub8PSSoX5Vkgw+jWf4KQtNvUFLDq8mF
WhUnPL6jHAADXpvs4lTNYwOtx9yQtbpxwSt7QJY1+ICrmRJB6BuKRt/jfDJF9Jsc
RQVlHIxQdKAJl7oaVnXgDkqtk2qddd3kCDXd74gv813G91z7CjsGyJ93oJIlNS3U
gFbD6V54JMgZ3rSmotYbz98oZxX7MKbtCm1aJ/q+hTv2YK1yMxrnfcieKmOYBbFD
hnW5O6RMA703dBK92j6XRN2EttLkQuujZgy+jXRKtaWMIlkNkWJmOiHmErQngHvt
iNkIcjJumq1ddFX4iaTI40a6zgvIBtxFeDs2RfcaH73er7ctNUUqgQT5rFgJhMmF
x76rQgB5OZUkodb5k2ex7P+Gu4J86bS15094UuYcV09hVeknmTh5Ex9CBKipLS2W
2wKBakf+aVYnNCU6S0nASqt2xrZpGC1v7v6DhuepyyJtn3qSV2PoBiU5Sql+aARp
wUibQMGm44gjyNDqDlVp+ShLQlUH9x8CAwEAAaOCAXUwggFxMB8GA1UdIwQYMBaA
FFN5v1qqK0rPVIDh2JvAnfKyA2bLMB0GA1UdDgQWBBTI2XhootkZaNU9ct5fCj7c
tYaGpjAOBgNVHQ8BAf8EBAMCAYYwEgYDVR0TAQH/BAgwBgEB/wIBADAdBgNVHSUE
FjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwIgYDVR0gBBswGTANBgsrBgEEAbIxAQIC
TjAIBgZngQwBAgEwUAYDVR0fBEkwRzBFoEOgQYY/aHR0cDovL2NybC51c2VydHJ1
c3QuY29tL1VTRVJUcnVzdFJTQUNlcnRpZmljYXRpb25BdXRob3JpdHkuY3JsMHYG
CCsGAQUFBwEBBGowaDA/BggrBgEFBQcwAoYzaHR0cDovL2NydC51c2VydHJ1c3Qu
Y29tL1VTRVJUcnVzdFJTQUFkZFRydXN0Q0EuY3J0MCUGCCsGAQUFBzABhhlodHRw
Oi8vb2NzcC51c2VydHJ1c3QuY29tMA0GCSqGSIb3DQEBDAUAA4ICAQAVDwoIzQDV
ercT0eYqZjBNJ8VNWwVFlQOtZERqn5iWnEVaLZZdzxlbvz2Fx0ExUNuUEgYkIVM4
YocKkCQ7hO5noicoq/DrEYH5IuNcuW1I8JJZ9DLuB1fYvIHlZ2JG46iNbVKA3ygA
Ez86RvDQlt2C494qqPVItRjrz9YlJEGT0DrttyApq0YLFDzf+Z1pkMhh7c+7fXeJ
qmIhfJpduKc8HEQkYQQShen426S3H0JrIAbKcBCiyYFuOhfyvuwVCFDfFvrjADjd
4jX1uQXd161IyFRbm89s2Oj5oU1wDYz5sx+hoCuh6lSs+/uPuWomIq3y1GDFNafW
+LsHBU16lQo5Q2yh25laQsKRgyPmMpHJ98edm6y2sHUabASmRHxvGiuwwE25aDU0
2SAeepyImJ2CzB80YG7WxlynHqNhpE7xfC7PzQlLgmfEHdU+tHFeQazRQnrFkW2W
kqRGIq7cKRnyypvjPMkjeiV9lRdAM9fSJvsB3svUuu1coIG1xxI1yegoGM4r5QP4
RGIVvYaiI76C0djoSbQ/dkIUUXQuB8AL5jyH34g3BZaaXyvpmnV4ilppMXVAnAYG
ON51WhJ6W0xNdNJwzYASZYH+tmCWI+N60Gv2NNMGHwMZ7e9bXgzUCZH5FaBFDGR5
S9VWqHB73Q+OyIVvIbKYcSc2w/aSuFKGSA==
-----END CERTIFICATE-----
)EOF";
    
  void EvergiMQTTSettings(){
    snprintf(StrMac,sizeof(StrMac),"%c%c%c%c%c%c%c%c%c%c%c%c",WiFi.macAddress()[0],WiFi.macAddress()[1],WiFi.macAddress()[3],WiFi.macAddress()[4],WiFi.macAddress()[6],WiFi.macAddress()[7],WiFi.macAddress()[9],WiFi.macAddress()[10],WiFi.macAddress()[12],WiFi.macAddress()[13],WiFi.macAddress()[15],WiFi.macAddress()[16]);
    snprintf(settingMQTTtopTopic,sizeof(settingMQTTtopTopic),"%s/smartstuff/%s/",EVERGI_MQTT_PREFIX,StrMac);
    strncpy( settingMQTTbroker , EVERGI_HOST  , sizeof(settingMQTTbroker) );
    strncpy( BaseOTAurl        , EVERGI_OTA   ,  sizeof(BaseOTAurl) );
    settingMQTTbrokerPort = EVERGI_PORT;
    EnableHAdiscovery = false;
    settingMQTTinterval = EVERGI_INTERVAL;
    CHANGE_INTERVAL_SEC(publishMQTTtimer, settingMQTTinterval);
    bActJsonMQTT = true;
//    wifiClient.setInsecure();
    wifiClient.setCACert(root_ca);
    DebugTf("[%s] => setServer(%s, %d) \r\n", settingMQTTbroker, settingMQTTbroker, settingMQTTbrokerPort);
    MQTTclient.setServer(EVERGI_HOST, EVERGI_PORT);
    snprintf(MqttID,sizeof(MqttID),"P1-Dongle-Pro-%s",StrMac);
    // Debug("EVERGI_TOKEN: ");Debugln(EVERGI_TOKEN);
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