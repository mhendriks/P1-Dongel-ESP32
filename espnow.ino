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

#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
// #define MACSTR "%02X:%02X:%02X:%02X:%02X:%02X"
#define MACSTR2 "%02x%02x%02x%02x%02x%02x"
#define PEER_SSID_CONTAINS "NRGM35"
#define RSSI_THRESHOLD -40 // Define an acceptable RSSI threshold

esp_now_peer_info_t slave;
// enum MessageType { PAIRING, DATA, CONFIRMED, NRGACTUALS, NRGTARIFF } messageType;
enum _pairStatus { _INACTIVE, _WAITING, _PAIRING, _CONFIRMED, } PairingStatus = _INACTIVE;

enum MessageType { PAIRING, COMMAND, DATA, CONFIRMED, NRGACTUALS, NRGTARIFS, NRGSTATIC, } messageType;
enum sAction { CONN_REQUEST, CONN_RESPONSE }; 


// uint8_t mac_peer1[] = {0x30, 0x30, 0xF9, 0xFD, 0x91, 0x19}; //test p1p
// uint8_t mac_peer2[] = {0x30, 0x30, 0xF9, 0xFD, 0x91, 0x18};
// uint8_t mac_peer1[] = {0x4A, 0x27, 0xE2, 0x1F, 0x24, 0x6C}; //nrgm 35 AP
// uint8_t mac_peer1[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; //broadcast
uint8_t mac_peer1[] = {0x48, 0x27, 0xE2, 0x1F, 0x24, 0x6C}; //nrgm 35

esp_now_peer_info_t peerInfo = {};


struct {
    uint8_t msgType = COMMAND;
    sAction action = CONN_RESPONSE;       // command
    uint8_t channel;
} Command;

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
  uint8_t   type = NRGTARIFS; // PEER_TARIFS
  float     FixedEnergy;    // euro
  float     Et1;            // euro
  float     Et2;            // euro
  float     Et1r;           // euro
  float     Et2r;           // euro
  float     FixedGas;       // euro
  float     Gas;            // euro
  float     FixedWater;     // euro
  float     Water;          // euro
  float     FixedHeat;      // euro
  float     Heat;           // euro
} sTariff;

struct_pairing pairingData, recvdata;
sActualData ActualData;
sTariff TariffData;

// ---------------------------- esp_ now -------------------------
void printMAC(const uint8_t * mac_addr){
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Debug(macStr);
}

void StopPairing(){
    Debugln("Stop Pairing");
    esp_now_deinit(); //stop service
    WiFi.mode(WIFI_STA); //set wifi to STA  
}

void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) { 
  Serial.print("msgTyp: ");Serial.println( incomingData[0] );
  switch ( incomingData[0] ){
    case COMMAND:
      Debugln("COMMAND RECEIVED");
      switch (incomingData[1]) {
        case CONN_REQUEST:
          Debugln("CONN_REQUEST");
          Command.action = CONN_RESPONSE;
          Command.channel = WiFi.channel(); //sent channel omdat een channel er naast soms ook werkt ... maar niet stabiel
          esp_now_send(NULL, (uint8_t *) &Command, sizeof(Command)); //ack to slave 
          break;
        case CONN_RESPONSE:
          Debugln("CONN_RESPONSE");
          break;
      }
      break;
  } //switch 
}

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Debug("Last Packet Send Status: ");
  Debug(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success to " : "Delivery Fail to ");
  // printMAC(mac_addr);
  // Debugln();
}

void HandlePairing() {
  // if ( httpServer.arg("cmd") == "stop" ) {
  //   StopPairing();
  //   httpServer.send(200, "application/json", "{\"Pairing\":\"Stoped\"}" );
  // }
  // if ( httpServer.arg("cmd") == "start" ) {
  //   StartPairing();
  //   httpServer.send(200, "application/json", "{\"Pairing\":\"Started\"}" );
  // }
  // if ( httpServer.arg("cmd") == "status" ) {
  //   char _status[50];
  //   snprintf(_status, sizeof(_status),"{\"status\":%d,\"client\":\"%s\"}",PairingStatus,PeerHostname);
  //   httpServer.send(200, "application/json", _status );
  // }
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

void fScanAndPeer( void * pvParameters ){
  // if( xTaskCreate( fBlink, "blink", 2048, NULL, 2, &tBlink ) == pdPASS ) Serial.println(F("Task Blinker succesfully created"));
  
  while (Pref.peers == 0) {
  
  Debugln(F("\nScanning for nearby devices..."));  
  WiFi.scanNetworks(true);
  int16_t numNetworks = 0;
  do {
    numNetworks = WiFi.scanComplete();
    delay(100);
  }  while ( numNetworks < 0 );
  Debug("numNetworks: "); Debugln(numNetworks); 
  // int numNetworks = WiFi.scanNetworks(); // Scan for WiFi networks (including ESP-NOW devices)
  
  for (int i = 0; i < numNetworks; i++) {
    int32_t rssi = WiFi.RSSI(i);
    String peerSSID = WiFi.SSID(i);
    
    if ( peerSSID.indexOf(PEER_SSID_CONTAINS) == -1 ) continue; //only with correct peer name

    Debugf("Device name: %s [%s] [%d] -> ", WiFi.SSID(i).c_str(), WiFi.BSSIDstr(i).c_str(), rssi);    
    if (rssi >= RSSI_THRESHOLD ) { //only closed by peers
      Debugln(F("Device above RSSI threshold, attempting to pair..."));
      Pref.peers++;
      memcpy(Pref.mac, WiFi.BSSID(i), 6);  
      break;
    } else {
      Debugln(F("- Device below RSSI threshold, ignoring."));
    }
  } //for

  WiFi.scanDelete(); // Clean up after the scan
  // if ( !Pref.peers ) delay(3000); //scan again after 3 sec
  }
  Debugf("Peers found : [%d] | mac : " MACSTR "\n",Pref.peers, MAC2STR(Pref.mac));
  P1StatusWrite();
  SetupESPNOW();
  // vTaskSuspend(tBlink);
  vTaskDelete(NULL);
}

TaskHandle_t tScanAndPeer; //  own proces for P1 reading

void SetupESPNOW(){
    if (esp_now_init() != ESP_OK) {
    Debugln("Error initializing ESP-NOW");
    return;
  }

  // Register send callback function
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  //add peer fixed
  memcpy(peerInfo.peer_addr, Pref.mac, 6);
  peerInfo.channel = 0; // Use default channel
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
}

void StartESPNOW(){
  // WiFi.mode(WIFI_STA);

  if ( ! Pref.peers  ) {
    // ScanAddPeers();
    //start scan and peer proces
    if( xTaskCreate( fScanAndPeer, "ScanAndPeer", 1024*4, NULL, 2, &tScanAndPeer ) == pdPASS ) {
      Debugln(F("Task tP1Reader succesfully created"));
    }
  } else {
    Debugln(F("Stored Peer will be used"));
    SetupESPNOW();
  }
}

void SendTariffData(){
  
  //todo implement last send  ... sent every 15 minutes

  TariffData.FixedEnergy  = settingENBK;
  TariffData.FixedGas     = settingGNBK;
  TariffData.FixedWater   = settingWNBK;

  TariffData.Et1          = settingEDT1;
  TariffData.Et2          = settingEDT2;
  TariffData.Et1r         = settingERT1;
  TariffData.Et2r         = settingERT2;

  TariffData.Gas          = settingGDT;
  TariffData.Water        = settingWDT;

  esp_now_send(NULL, (uint8_t *) &TariffData, sizeof(TariffData));

}

String SolarProdActual();
String SolarProdToday();

void SendActualData(){
  if ( !Pref.peers ) return;
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
