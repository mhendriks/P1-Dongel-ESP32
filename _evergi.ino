#ifdef EVERGI
  #define EVERGI_TEST
  #include "/Users/martijn/Documents/Arduino/evergi.h"
    
  void EvergiMQTTSettings(){
    snprintf(settingMQTTtopTopic,45,"%s/smartstuff/%c%c%c%c%c%c%c%c%c%c%c%c/grid",EVERGI_MQTT_PREFIX,WiFi.macAddress()[0],WiFi.macAddress()[1],WiFi.macAddress()[3],WiFi.macAddress()[4],WiFi.macAddress()[6],WiFi.macAddress()[7],WiFi.macAddress()[9],WiFi.macAddress()[10],WiFi.macAddress()[12],WiFi.macAddress()[13],WiFi.macAddress()[15],WiFi.macAddress()[16]);
    strncpy( settingMQTTbroker , EVERGI_HOST  , 101 );
//    strncpy( settingMQTTuser   , EVERGI_USER  ,  40 );
//    strncpy( settingMQTTpasswd , EVERGI_TOKEN ,  30 );
    strncpy( BaseOTAurl        , EVERGI_OTA   ,  35 );
    settingMQTTbrokerPort = EVERGI_PORT;
    EnableHAdiscovery = false;
    settingMQTTinterval = EVERGI_INTERVAL;
    CHANGE_INTERVAL_SEC(publishMQTTtimer, settingMQTTinterval);
    bActJsonMQTT = true;
    wifiClient.setInsecure();
  }
    

    
#endif
