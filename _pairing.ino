/*
 * Companion (client) device could obtain some configs (ssid/psk/hostname/ip address) from the host/server
 * Steps:
 * 1- client start up and broadcast to all wifi channels
 * 2- Manual action: the server/host starts listning and receives the pairing request
 * 3- host add client to peer list and send back the data
 * 4- client recieves and proces the data 
 * 5- client send ack back when everthing is okay
 * 6- server closed the service
 * 
 * based on; https://randomnerdtutorials.com/esp-now-auto-pairing-esp32-esp8266/
 * docs:https://demo-dijiudu.readthedocs.io/en/latest/api-reference/wifi/esp_now.html
 * 
 */

#ifdef DEV_PAIRING

#ifndef __PAIRING //only once
#define __PAIRING

#include "./../../_secrets/pairing.h"

esp_now_peer_info_t slave;
enum MessageType { PAIRING, DATA, CONFIRMED, } messageType;
enum _pairStatus { _INACTIVE, _WAITING, _PAIRING, _CONFIRMED, } PairingStatus = _INACTIVE;

char PeerHostname[30];

int chan;

struct {
  // uint32_t  reboots;
  uint8_t   mac[6];
  uint8_t   peers;      
} Pref;

typedef struct struct_pairing {
    uint8_t msgType;
    char    ssid[32];    //max 32
    char    pw[63];      //max 63
    char    host[30];    //max 30
    uint8_t ipAddr[4];  //max 4
} struct_pairing;

struct_pairing pairingData, recvdata;

// ---------------------------- esp_ now -------------------------
void printMAC(const uint8_t * mac_addr){
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Debug(macStr);
}

bool addPeer(const uint8_t *peer_addr) {      // add pairing
  memset(&slave, 0, sizeof(slave));
  const esp_now_peer_info_t *peer = &slave;
  memcpy(slave.peer_addr, peer_addr, 6);
  
  slave.channel = chan; // pick a channel
  slave.encrypt = 0; // no encryption
  // check if the peer exists
  bool exists = esp_now_is_peer_exist(slave.peer_addr);
  if (exists) {
    // Slave already paired.
    Debugln("Already Paired");
    return true;
  }
  else {
    esp_err_t addStatus = esp_now_add_peer(peer);
    if (addStatus == ESP_OK) {
      // Pair success
      Debugln("Pair success");
      return true;
    }
    else 
    {
      Debugln("Pair failed");
      return false;
    }
  }
} 

void StopPairing(){
    Debugln("Stop Pairing");
    esp_now_deinit(); //stop service
    WiFi.mode(WIFI_STA); //set wifi to STA  
}

void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
// void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) { 
  // Debugf("%i bytes of data received from : ",len);printMAC(mac_addr);Debugln();
    Debug("msgTyp: ");Debugln( incomingData[0] );
    switch ( (uint8_t)incomingData[0] ){
      case CONFIRMED:
        PairingStatus = _CONFIRMED;
        Debugln("Confirmed");
        StopPairing();
        break;
      case PAIRING:
        PairingStatus = _PAIRING;
        memcpy(&recvdata, incomingData, sizeof(recvdata));
        Debug("pw: ");Debugln(recvdata.pw);
        if ( strcmp(recvdata.pw, CONFIGPW ) == 0 ){
        Debug("msgType: ");Debugln(recvdata.msgType);
        Debug("host: ");Debugln(recvdata.host);
        strncpy(PeerHostname,recvdata.host,sizeof(PeerHostname));
        memcpy(Pref.mac, info->src_addr,6);
        addPeer(Pref.mac);
        esp_err_t result = esp_now_send(Pref.mac, (uint8_t *) &pairingData, sizeof(pairingData));
      }  else Debugln("Incorrect pw");
        break;
      case DATA:
        //komt hier niet
        break;  
    }  
}

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Debug("Last Packet Send Status: ");
  Debug(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success to " : "Delivery Fail to ");
  printMAC(mac_addr);
  Debugln();
}

void HandlePairing() {
  if ( httpServer.arg("cmd") == "stop" ) {
    StopPairing();
    httpServer.send(200, "application/json", "{\"Pairing\":\"Stoped\"}" );
  }
  if ( httpServer.arg("cmd") == "start" ) {
    StartPairing();
    httpServer.send(200, "application/json", "{\"Pairing\":\"Started\"}" );
  }
  if ( httpServer.arg("cmd") == "status" ) {
    char _status[50];
    snprintf(_status, sizeof(_status),"{\"status\":%d,\"client\":\"%s\"}",PairingStatus,PeerHostname);
    httpServer.send(200, "application/json", _status );
  }
}


//=======================================================================


#ifdef ETHERNET
bool readWifiCredentials(){
  if ( !FSmounted || !LittleFS.exists("/wifi.json") ) return false;
  Debugln("wifi.json exists");
  
  JsonDocument doc; 
  File SettingsFile = LittleFS.open("/wifi.json", "r");
  if (!SettingsFile) return false;
  Debugln("wifi.json read");
  
  DeserializationError error = deserializeJson(doc, SettingsFile);
  if (error) {
    SettingsFile.close();
    return false;
  }
  SettingsFile.close();
  Debugln("wifi.json Deserialised");
  if ( !doc["ssid"].is<const char*>() || !doc["pw"].is<const char*>() ) return false;

  strcpy( pairingData.ssid, doc["ssid"] );
  strcpy( pairingData.pw, doc["pw"] );
  Debug("ssid: ");Debugln(pairingData.ssid);
  Debug("pw  : ");Debugln(pairingData.pw);
  
  return true;
 
}
#endif

void StartPairing() {
  
  WiFiManager manageWiFi;
  Debug("Server MAC Address:  ");Debugln(WiFi.macAddress());
  Debug("Server SOFT AP MAC Address:  "); Debugln(WiFi.softAPmacAddress());
  chan = WiFi.channel();
  Debug("Station IP Address: "); Debugln(WiFi.localIP());
  Debug("Wi-Fi Channel: ");  Debugln(WiFi.channel());
  
  //prepair the hostdata
#ifdef ETHERNET  
  for ( int i=0 ; i<4 ; i++ ) pairingData.ipAddr[i] = ETH.localIP()[i]; 
  if ( !readWifiCredentials() ) {
    Debugln("Pairing Aborted - no wifi credentials");
    httpServer.send(200, "application/json", "{\"Pairing\":\"Stoped\"}" );
    return;
  }
  
#else  
  for ( int i=0 ; i<4 ; i++ ) pairingData.ipAddr[i] = WiFi.localIP()[i];
  strcpy( pairingData.ssid, WiFi.SSID().c_str() );
  strcpy( pairingData.pw, manageWiFi.getWiFiPass().c_str() );
#endif
  
  strcpy( pairingData.host, settingHostname );
  pairingData.msgType = DATA;

  // Set the device as a Station and Soft Access Point simultaneously
  WiFi.mode(WIFI_AP_STA);

  if (esp_now_init() != ESP_OK) {
      Debugln("Error initializing ESP-NOW");
      return;
    }
    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);
    
    Debugln("Pairing started");
    PeerHostname[0] = '\0'; //clear
    PairingStatus = _WAITING;
}

#endif //_PAIRING
#endif //DEV_PAIRING
