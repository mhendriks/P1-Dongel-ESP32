/* 
***************************************************************************  
**  Copyright (c) 2023 Martijn Hendriks / based on DSMR Api Willem Aandewiel
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

#ifdef ESPNOW

/*
  to activate call StartESPNOW in setup
  function is triggerd from :
  - peer ASKTARIFF and ASKSTATICS
  - when a new p1 telegram is processing (send actual data)
  - changing statics or tariffs (sending new statics or tariff data) (todo)

  pairing done by pressing the dongle button once
  Once paired no other pairring could take place. repair by pressing the button again

  master is pretty chill normally, just waiting or sending p1 data. timeouts are handled by the peer

  todo: rssi check on pairing https://github.com/TenoTrash/ESP32_ESPNOW_RSSI/blob/main/Modulo_Receptor_OLED_SPI_RSSI.ino

*/

#ifndef __ESPNOW //only once
#define __ESPNOW

#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
// #define MACSTR "%02X:%02X:%02X:%02X:%02X:%02X"
#define MACSTR2 "%02x%02x%02x%02x%02x%02x"
#define PEER_NAME_CONTAINS "NRGM35"
// #define RSSI_THRESHOLD -40 // Define an acceptable RSSI threshold

String SolarProdActual();
String SolarProdToday();
uint32_t SolarWpTotal();

// enum _pairStatus { _INACTIVE, _WAITING, _PAIRING, _CONFIRMED, } PairingStatus = _INACTIVE;
enum MessageType { COMMAND, CONFIRMED, NRGACTUALS, NRGTARIFS, NRGSTATIC, } messageType;
enum sAction { CONN_REQUEST, CONN_RESPONSE, CONN_CLEAR, PAIRING, ASK_TARIF, ASK_STATIC }; 

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

esp_now_peer_info_t peerInfo = {};

typedef struct  {
    uint8_t msgType = COMMAND;
    sAction action = CONN_RESPONSE;       // command
    uint8_t channel;
    char host[20];
} __attribute__((packed)) command_t;

command_t Command;
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

// typedef struct struct_pairing {
//     uint8_t msgType;
//     char    ssid[32];    //max 32
//     char    pw[63];      //max 63
//     char    host[30];    //max 30
//     uint8_t ipAddr[4];  //max 4
// } struct_pairing;

typedef struct { 
    uint8_t   msgType = NRGTARIFS;
    uint16_t  Efixed;   //per maand in centen
    uint16_t  Et1;      //per kwh in centen
    uint16_t  Et2;      //per kwh in centen
    uint16_t  Et1r;     //terugleververgoeding per kwh in centen
    uint16_t  Et2r;     //terugleververgoeding per kwh in centen
    uint16_t  Gfixed;   //per maand in centen
    uint16_t  G;        //per m3 in centen
    uint16_t  Wfixed;   //per maand in centen
    uint16_t  W;        //per m3 in centen
    uint16_t  Efine;    //terugleverboete per Kwh in centen
} sTariff;

struct { 
    uint8_t   msgType = NRGSTATIC;
    uint16_t  WpSolar;    //watt peak solar system
    char      ssid[32];   //wifi ssid
    char      pw[63];     //wifi password
} Static;

sActualData ActualData;
sTariff TariffData;

void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  Debug("msgTyp: ");Debugln( incomingData[0] );
  switch ( incomingData[0] ){
    case COMMAND:
      memcpy(&Command, incomingData, sizeof(Command));
      Debugf("COMMAND RECEIVED type [%i], state [%i]\n", Command.action,bPairingmode);
      switch (Command.action) {
        case CONN_REQUEST:
          Debugln("CONN_REQUEST");
          if ( Pref.peers ) { //peer aanwezig
            Debugln("CONN_REQUEST: peer aanwezig");
            Command.action = CONN_RESPONSE;
            Command.channel = WiFi.channel(); //sent channel omdat een channel er naast soms ook werkt ... maar niet stabiel
            esp_now_send(NULL, (uint8_t *) &Command, sizeof(Command)); //ack to slave 
          } else {
            Debugln("CONN_REQUEST: NOT PAIRED");
            Command.action = CONN_CLEAR;
            esp_now_send(broadcastAddress, (uint8_t *) &Command, sizeof(Command)); //ack to slave 
          }
          break;
        case CONN_RESPONSE:
          Debugln("CONN_RESPONSE");
          break;
        case CONN_CLEAR:
          Debugln("CONN_CLEAR");
          break;
        case ASK_TARIF:
          Debugln("ASK_TARIFS");
          SendTariffData();
          break;  
        case ASK_STATIC:
          Debugln("ASK_STATIC");
          sendStatic();
          break;          
        case PAIRING:
          Debugln("PAIRING");
          memcpy(&Command, incomingData, sizeof(Command));
          if ( bPairingmode && Pref.peers == 0 ) { //alleen indien peers = 0
            String hostname = Command.host;
            Debugln("CONN_REQUEST: pair mode");
            if ( hostname.indexOf( PEER_NAME_CONTAINS ) != -1 ) {
              Debugln("CONN_REQUEST: pair mode en juiste hostname");
              memcpy(Pref.mac, info->src_addr,6);
              AddPeer(Pref.mac);
              bPairingmode = false;
              Pref.peers = 1;
              P1StatusWrite();
              Command.action = PAIRING;
              Command.channel = WiFi.channel(); //sent channel omdat een channel er naast soms ook werkt ... maar niet stabiel ivm overlappende channels
              esp_now_send(NULL, (uint8_t *) &Command, sizeof(Command)); //ack to slave 
            }
          } else Debugln("NOT IN PAIRING MODE");
          break;
      }
      break;
  } //switch 
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
#ifdef DEBUG  
  Debug("Last Packet Send Status: ");
  Debugln(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  // printMAC(mac_addr);
  // Debugln();
#endif  
}


void AddPeer( uint8_t * peer_mac ){
    memcpy(peerInfo.peer_addr, peer_mac, 6);
    peerInfo.channel = 0; // Use default channel
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
}

bool bESPNowInit = false;

void StartESPNOW(){
#ifdef ETHERNET
  if ( readWifiCredentials() ) WiFi.begin(Static.ssid, Static.pw);
  else WiFi.mode(WIFI_STA);
#endif  
  if (esp_now_init() != ESP_OK) {
  Debugln("Error initializing ESP-NOW");
  return;
  } 

  // Register send callback function
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  AddPeer(broadcastAddress); //always add broadcast address 
  if ( Pref.peers ) AddPeer(Pref.mac);

  bESPNowInit = true;
}

#ifdef ETHERNET
bool readWifiCredentials(){
  // Static.ssid = '\0';
  // Static.pw = '\0';
  if ( !FSmounted || !LittleFS.exists("/wifi.json") ) return false;
  // Debugln("wifi.json exists");
  
  StaticJsonDocument<200> doc;
  File SettingsFile = LittleFS.open("/wifi.json", "r");
  if (!SettingsFile) return false;
  // Debugln("wifi.json read");
  
  DeserializationError error = deserializeJson(doc, SettingsFile);
  if (error) {
    SettingsFile.close();
    return false;
  }
  SettingsFile.close();
  Debugln("wifi.json read and deserialised");
  if ( !doc.containsKey("ssid") || !doc.containsKey("pw") ) return false;

  strcpy( Static.ssid, doc["ssid"] );
  strcpy( Static.pw, doc["pw"] );

#ifdef DEBUG  
  Debug("ssid: ");Debugln(Static.ssid);
  Debug("pw  : ");Debugln(Static.pw);
#endif
  return true;
}
#endif

void sendStatic() {
#ifdef ETHERNET 
  if ( strlen(Static.pw) == 0 || strlen(Static.ssid) == 0 ) {
    if ( readWifiCredentials() ) WiFi.begin(Static.ssid, Static.pw);
  }
#else
  WiFiManager manageWiFi;
  strcpy(Static.pw, manageWiFi.getWiFiPass().c_str());
  strcpy(Static.ssid, WiFi.SSID().c_str());
#endif
  Static.msgType = NRGSTATIC;
  Static.WpSolar = SolarWpTotal();
  esp_err_t result = esp_now_send(NULL, (uint8_t *) &Static, sizeof(Static));
}

void SendTariffData(){
  
  TariffData.msgType= NRGTARIFS;
  TariffData.Efixed = (uint16_t)(settingENBK*1000.0);
  TariffData.Gfixed = (uint16_t)(settingGNBK*1000.0);
  TariffData.Wfixed = (uint16_t)(settingWNBK*1000.0);

  TariffData.Et1    = (uint16_t)(settingEDT1*1000.0);
  TariffData.Et2    = (uint16_t)(settingEDT2*1000.0);
  TariffData.Et1r   = (uint16_t)(settingERT1*1000.0);
  TariffData.Et2r   = (uint16_t)(settingERT2*1000.0);
  TariffData.G      = (uint16_t)(settingGDT*1000.0);
  TariffData.W      = (uint16_t)(settingWDT*1000.0);

  // Debugf( "TariffData.Efixed : %i\n", TariffData.Efixed );
  // Debugf( "TariffData.Gfixed : %i\n", TariffData.Gfixed );
  // Debugf( "TariffData.Wfixed : %i\n", TariffData.Wfixed );
  // Debugf( "TariffData.Et1    : %i\n", TariffData.Et1 );
  // Debugf( "TariffData.Et2    : %i\n", TariffData.Et2 );
  // Debugf( "TariffData.Et1r   : %i\n", TariffData.Et1r );
  // Debugf( "TariffData.Et2r   : %i\n", TariffData.Et2r );
  // Debugf( "TariffData.W      : %i\n", TariffData.W );
  // Debugf( "TariffData.G      : %i\n", TariffData.G );

  esp_now_send(NULL, (uint8_t *) &TariffData, sizeof(TariffData));

}

void P2PSendActualData(){
  if ( ! Pref.peers || ! bESPNowInit ) return;
  ActualData.epoch  = actT;
  ActualData.P      = DSMRdata.power_delivered.int_val();
  ActualData.Pr     = DSMRdata.power_returned.int_val() ;
  ActualData.e_t1   = DSMRdata.energy_delivered_tariff1.int_val() - dataYesterday.t1;
  ActualData.e_t2   = DSMRdata.energy_delivered_tariff2.int_val() - dataYesterday.t2;
  ActualData.e_t1r  = DSMRdata.energy_returned_tariff1.int_val() - dataYesterday.t1r;
  ActualData.e_t2r  = DSMRdata.energy_returned_tariff2.int_val() - dataYesterday.t2r;
  if ( mbusGas ) ActualData.Gas = gasDelivered * 1000 - dataYesterday.gas;
  else ActualData.Gas = UINT32_MAX; 
  if ( mbusWater ) ActualData.Water  = waterDelivered * 1000 - dataYesterday.water;
  else ActualData.Water = UINT32_MAX;
  ActualData.Esolar = SolarProdToday().toInt();
  if ( !Enphase.Available && !SolarEdge.Available ) ActualData.Psolar = UINT32_MAX;
  else ActualData.Psolar = SolarProdActual().toInt();

  // esp_err_t result = esp_now_send(NULL, (uint8_t *) &ActualData, sizeof(ActualData));
  esp_now_send(NULL, (uint8_t *) &ActualData, sizeof(ActualData));

  // Debug("Senddata Ptot: ");Debugln(ActualData.P+ActualData.Pr);

}

#endif //_ESPNOW
#else
  void StartESPNOW(){}
#endif //ESPNOW
