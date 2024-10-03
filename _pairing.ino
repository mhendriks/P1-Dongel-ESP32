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
enum MessageType { PAIRING, DATA, CONFIRMED, NRGACTUALS} messageType;
enum _pairStatus { _INACTIVE, _WAITING, _PAIRING, _CONFIRMED, } PairingStatus = _INACTIVE;

char PeerHostname[30];

int chan;

typedef struct {
  uint8_t   type = NRGACTUALS;       // PEER_ACTUALS
  time_t    epoch;      // sec from 1/1/1970
  uint32_t  P;          // W
  uint32_t  Pr;         // W
  uint32_t  e_t1;       // Wh
  uint32_t  e_t2;       // Wh
  uint32_t  e_t1r;      // Wh
  uint32_t  e_t2r;      // Wh
  uint32_t  Gas;        // dm3/Liter
  uint32_t  Water;      // dm3/Liter
  uint32_t  Psolar;     // W  
  uint32_t  Esolar;     // Wh  
} sActualData;

typedef struct struct_pairing {
    uint8_t msgType;
    char    ssid[32];    //max 32
    char    pw[63];      //max 63
    char    host[30];    //max 30
    uint8_t ipAddr[4];  //max 4
} struct_pairing;

typedef struct {
  uint8_t   type;        // PEER_TARIFS
  float     FixedEnergy;    // euro
  float     LowEnergy;      // euro
  float     NormalEnergy;   // euro
  float     FixedGas;       // euro
  float     Gas;            // euro
  float     FixedWater;     // euro
  float     Water;          // euro
  float     FixedHeat;      // euro
  float     Heat;           // euro
} sTarifs;

struct_pairing pairingData, recvdata;
sActualData ActualData;

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

void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) { 
  Debugf("%i bytes of data received from : ",len);printMAC(mac_addr);Debugln();
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
        addPeer(mac_addr);
        esp_err_t result = esp_now_send(mac_addr, (uint8_t *) &pairingData, sizeof(pairingData));
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
  
  StaticJsonDocument<200> doc; 
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
  if ( !doc.containsKey("ssid") || !doc.containsKey("pw") ) return false;

  strcpy( pairingData.ssid, doc["ssid"] );
  strcpy( pairingData.pw, doc["pw"] );
  Debug("ssid: ");Debugln(pairingData.ssid);
  Debug("pw  : ");Debugln(pairingData.pw);
  
  return true;
 
}
#endif

// uint8_t mac_peer1[] = {0x30, 0x30, 0xF9, 0xFD, 0x91, 0x19}; //test p1p
// uint8_t mac_peer2[] = {0x30, 0x30, 0xF9, 0xFD, 0x91, 0x18};
// uint8_t mac_peer1[] = {0x4A, 0x27, 0xE2, 0x1F, 0x24, 0x6C}; //nrgm 35 AP
// uint8_t mac_peer1[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; //broadcast
uint8_t mac_peer1[] = {0x48, 0x27, 0xE2, 0x1F, 0x24, 0x6C}; //nrgm 35

esp_now_peer_info_t peerInfo = {};

void StartESPNOW(){
  // WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
      Debugln("Error initializing ESP-NOW");
      return;
    }
  
  Debug("WifiChannel: ");Debugln(WiFi.channel());
  memcpy(peerInfo.peer_addr, mac_peer1, 6);
  peerInfo.channel = WiFi.channel(); // Use default channel
  peerInfo.encrypt = false;
  Debug("add peer: ");Debugln(esp_now_add_peer(&peerInfo)==ESP_OK?"SUCCES":"FAILED");
    
    // addPeer(mac_peer1);
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
    
  Debugln("Pairing started");
}

void StartPairing() {
  
  WiFiManager manageWiFi;
  Debug("Server MAC Address:  ");Debugln(WiFi.macAddress());
  Debug("Server SOFT AP MAC Address:  "); Debugln(WiFi.softAPmacAddress());
  chan = WiFi.channel();
  Debug("Station IP Address: "); Debugln(WiFi.localIP());
  Debug("Wi-Fi Channel: ");  Debugln(chan);
  
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

String SolarProdActual();
String SolarProdToday();

void SendData2Display(){
  
  ActualData.epoch  = actT;
  ActualData.P      = DSMRdata.power_delivered.int_val();
  ActualData.Pr     = DSMRdata.power_returned.int_val() ;
  ActualData.e_t1   = DSMRdata.energy_delivered_tariff1.int_val() - dataYesterday.t1;
  ActualData.e_t2   = DSMRdata.energy_delivered_tariff2.int_val() - dataYesterday.t2;
  ActualData.e_t1r  = DSMRdata.energy_returned_tariff1.int_val() - dataYesterday.t1r;
  ActualData.e_t2r  = DSMRdata.energy_returned_tariff2.int_val() - dataYesterday.t2r;
  if ( mbusGas ) ActualData.Gas = gasDelivered * 1000 - dataYesterday.gas;
  else ActualData.Gas = 0xFFFFFFFF;
  if ( mbusWater ) ActualData.Water  = waterDelivered * 1000 - dataYesterday.water;
  else ActualData.Water = 0xFFFFFFFF;
  ActualData.Esolar = SolarProdToday().toInt();
  if ( !Enphase.Available && !SolarEdge.Available ) ActualData.Psolar = 0xFFFFFFFF;
  else ActualData.Psolar = SolarProdActual().toInt();

  // esp_err_t result = esp_now_send(NULL, (uint8_t *) &ActualData, sizeof(ActualData));
  esp_now_send(NULL, (uint8_t *) &ActualData, sizeof(ActualData));

  // Debug("Senddata Ptot: ");Debugln(ActualData.P+ActualData.Pr);

}

#endif //_PAIRING
#endif //DEV_PAIRING
