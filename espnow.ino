/* 
***************************************************************************  
**  Copyright (c) 2023 Martijn Hendriks / based on DSMR Api Willem Aandewiel
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

#ifdef ESPNOW 

#ifndef __ESPNOW //only once
#define __ESPNOW

#include <CRC32.h> // Voor CRC32 berekening, zorg dat deze bibliotheek geïnstalleerd is

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

#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
// #define MACSTR "%02X:%02X:%02X:%02X:%02X:%02X"
#define MACSTR2 "%02x%02x%02x%02x%02x%02x"
#define PEER_NAME_CONTAINS "NRGM35"
const int ESP_NOW_CHUNK_SIZE = 232; 
// const int ESP_NOW_CHUNK_SIZE = 1450; //from SDK 3.3.0

// #define RSSI_THRESHOLD -40 // Define an acceptable RSSI threshold

String SolarProdActual();
String SolarProdToday();
uint32_t SolarWpTotal();
uint8_t P2PType = 0;

// --- Globale variabelen voor ACK-mechanisme ---
volatile bool ackReceived = false;
volatile uint16_t lastAckPacketId = 0;
volatile bool lastAckSuccess = false;

// enum _pairStatus { _INACTIVE, _WAITING, _PAIRING, _CONFIRMED, } PairingStatus = _INACTIVE;
enum MessageType { COMMAND, CONFIRMED, NRGACTUALS, NRGTARIFS, NRGSTATIC, UPD_DATA, UPD_VER_RSP, UPD_VER_REQ, UPD_ACK, UPD_GO_UPDATE, } messageType;

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

//all values x 100.000
typedef struct { 
    uint8_t   msgType = NRGTARIFS;
    uint32_t  Efixed;   //per maand in centen
    uint32_t  Et1;      //per kwh in centen
    uint32_t  Et2;      //per kwh in centen
    uint32_t  Et1r;     //terugleververgoeding per kwh in centen
    uint32_t  Et2r;     //terugleververgoeding per kwh in centen
    uint32_t  Gfixed;   //per maand in centen
    uint32_t  G;        //per m3 in centen
    uint32_t  Wfixed;   //per maand in centen
    uint32_t  W;        //per m3 in centen
    uint32_t  Efine;    //terugleverboete per Kwh in centen
} sTariff;

struct { 
    uint8_t   msgType = NRGSTATIC;
    uint16_t  WpSolar;    //watt peak solar system
    char      ssid[32];   //wifi ssid
    char      pw[63];     //wifi password
} Static;

// Structuur voor ESP-NOW data (moet overeenkomen met de Slave)
typedef struct {
  uint8_t type = UPD_DATA;
  uint32_t totalSize;
  uint32_t currentOffset;
  uint16_t chunkSize;
  uint16_t packetId; // Uniek ID voor elk verzonden pakket
  uint8_t data[ESP_NOW_CHUNK_SIZE];
  bool isLastChunk;
  uint32_t crc32;    // CRC/checksum voor data integriteit
} __attribute__((packed)) esp_now_update_data_t;

// Structuur voor ESP-NOW ACK (van Slave naar Master, moet overeenkomen met de Slave)
typedef struct {
  uint8_t type = UPD_ACK;
  uint16_t packetId;      // Het ID van het ontvangen pakket
  bool success;           // True als het pakket succesvol is ontvangen
  uint32_t currentOffset; // De laatst succesvol ontvangen offset
} __attribute__((packed)) esp_now_ack_data_t;

sActualData ActualData;
sTariff TariffData;

void procesACK(const uint8_t *incomingData){
  esp_now_ack_data_t ackData;
  memcpy(&ackData, incomingData, sizeof(ackData));

  // Optioneel: filter op MAC-adres van de verwachte slave (als je meerdere slaves hebt)
  // if (memcmp(info->src_addr, slaveMACAddress, 6) != 0) {
  //   Serial.println("Received ACK from unknown sender, ignoring.");
  //   return;
  // }

  Debugf("Received ACK for packet ID %u, success: %s (Slave Offset: %u)\n", ackData.packetId, ackData.success ? "true" : "false", ackData.currentOffset);
  lastAckPacketId = ackData.packetId;
  lastAckSuccess = ackData.success;
  ackReceived = true; // Markeer dat een ACK is ontvangen
}

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
    case UPD_VER_REQ:
      Debugln("UPD_VER_REQ");
      memcpy(&client_ota_data, incomingData, sizeof(client_ota_data));
      P2PType = UPD_VER_REQ;
      break;
    case UPD_GO_UPDATE:
      Debug("Size UPD data frame: ");Debugln(sizeof(esp_now_update_data_t));
      memcpy(&client_ota_data, incomingData, sizeof(client_ota_data));
      P2PType = UPD_GO_UPDATE;
      break;
    case UPD_ACK:
      if (len != sizeof(esp_now_ack_data_t)) {
        Debugln("Received data with incorrect length, ignoring.");
        return;
      }
      procesACK(incomingData);
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
// #ifdef ETHERNET 
//   if ( strlen(Static.pw) == 0 || strlen(Static.ssid) == 0 ) {
//     if ( readWifiCredentials() ) WiFi.begin(Static.ssid, Static.pw);
//   }
// #else
//   WiFiManager manageWiFi;
//   strcpy(Static.pw, manageWiFi.getWiFiPass().c_str());
//   strcpy(Static.ssid, WiFi.SSID().c_str());
// #endif
  Static.msgType = NRGSTATIC;
  Static.WpSolar = SolarWpTotal();
  esp_err_t result = esp_now_send(NULL, (uint8_t *) &Static, sizeof(Static));
}

void SendTariffData(){
  
  TariffData.msgType= NRGTARIFS;
  TariffData.Efixed = (uint32_t)(settingENBK*100000.0);
  TariffData.Gfixed = (uint32_t)(settingGNBK*100000.0);
  TariffData.Wfixed = (uint32_t)(settingWNBK*100000.0);

  TariffData.Et1    = (uint32_t)(settingEDT1*100000.0);
  TariffData.Et2    = (uint32_t)(settingEDT2*100000.0);
  TariffData.Et1r   = (uint32_t)(settingERT1*100000.0);
  TariffData.Et2r   = (uint32_t)(settingERT2*100000.0);
  TariffData.G      = (uint32_t)(settingGDT*100000.0);
  TariffData.W      = (uint32_t)(settingWDT*100000.0);

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

bool sendUpdateViaESPNOWFromHTTP(const char* url) {
  WiFiClient client;
  HTTPClient http;
  esp_now_update_data_t dataPacket;

  if (!http.begin(client, url)) {
    Debugln("HTTP begin failed");
    dataPacket.totalSize = 0;
    esp_now_send(NULL, (uint8_t *) &dataPacket, sizeof(dataPacket));
    return false;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Debugf("HTTP GET failed, code: %d\n", httpCode);
    http.end();
    dataPacket.totalSize = 0;
    esp_now_send(NULL, (uint8_t *) &dataPacket, sizeof(dataPacket));
    return false;
  }

  uint32_t totalSize = http.getSize(); // Totale grootte van de firmware
  if (totalSize == -1) {
      Debugln("Warning: HTTP server did not provide content length. Update progress will not be accurate.");
      // Je zou hier een maximale updategrootte moeten instellen om overruns te voorkomen.
  }
  
  // De rest van de ESP-NOW structuur en logica blijft vergelijkbaar
  dataPacket.totalSize = totalSize; // Of een ingestelde max als totalSize onbekend is
  // dataPacket.updateType = 0; // default in de initiatie van de struct opgenomen

  uint32_t currentOffset = 0;
  uint16_t currentPacketId = 0;

  // De HTTP-client stream
  WiFiClient *stream = http.getStreamPtr();

  // Loop zolang er data te lezen is of de totale grootte nog niet is bereikt
  while ( (totalSize == -1 && http.connected()) || (currentOffset < totalSize) ) {
    if ( !http.connected() ) {
      Debugln("HTTP stream disconnected unexpectedly.");
      dataPacket.totalSize = 0;
      esp_now_send(NULL, (uint8_t *) &dataPacket, sizeof(dataPacket));
      break;
    }
    size_t bytesAvailable = stream->available();
    if (bytesAvailable == 0) {
      delay(1); // Geef Wi-Fi stack ademruimte
      continue; // Geen data beschikbaar, wacht even
    }

    // Lees de optimale ESP_NOW_CHUNK_SIZE of de resterende bytes
    size_t bytesToRead = min(bytesAvailable, (size_t)ESP_NOW_CHUNK_SIZE);
    
    // Lees data direct in de dataPacket buffer
    size_t bytesRead = stream->readBytes(dataPacket.data, bytesToRead);

    if ( bytesRead == 0 ) {
        // Geen bytes gelezen, mogelijk einde stream of fout
        if (!http.connected() && currentOffset < totalSize) { // Als verbinding verbroken en niet alles gelezen
            Debugln("HTTP stream disconnected unexpectedly.");
            dataPacket.totalSize = 0;
            esp_now_send(NULL, (uint8_t *) &dataPacket, sizeof(dataPacket));
            http.end();
            return false;
        }
        delay(10); // Wacht even voordat je opnieuw probeert te lezen
        continue;
    }

    dataPacket.currentOffset = currentOffset;
    dataPacket.chunkSize = bytesRead;
    dataPacket.isLastChunk = (totalSize != -1 && (currentOffset + bytesRead >= totalSize)); // Einde als totale grootte bekend is
    dataPacket.packetId = currentPacketId;
    dataPacket.crc32 = CRC32::calculate(dataPacket.data, bytesRead);

    int retries = 0;
    const int MAX_RETRIES = 10; 
    const int ACK_TIMEOUT_MS = 1000; 

    bool chunkSentSuccessfully = false;

    while (retries < MAX_RETRIES) {
      ackReceived = false; 
      
      esp_err_t result = esp_now_send(NULL, (uint8_t *) &dataPacket, sizeof(dataPacket));

      if (result != ESP_OK) {
        Debugf("Error %d sending chunk (attempt %d/%d): %s. Retrying...\n", 
                      result, retries + 1, MAX_RETRIES, esp_err_to_name(result)); 
        retries++;
        delay(50);
        continue;
      }

      unsigned long startTime = millis();
      while (!ackReceived && (millis() - startTime < ACK_TIMEOUT_MS)) {
        delay(5);
      }

      if (ackReceived) {
        if (lastAckPacketId == currentPacketId && lastAckSuccess) {
          chunkSentSuccessfully = true;
          break;
        } else {
          Debugf("Received ACK for wrong ID (%u != %u) or NACK (success=%s). Retrying (attempt %d/%d).\n", 
                        lastAckPacketId, currentPacketId, lastAckSuccess ? "true" : "false", retries + 1, MAX_RETRIES);
          retries++;
          delay(50);
        }
      } else {
        Debugf("Timeout waiting for ACK for packet ID %u (attempt %d/%d). Retrying...\n", currentPacketId, retries + 1, MAX_RETRIES);
        retries++;
        delay(50);
      }
    }

    if (!chunkSentSuccessfully) {
      Debugf("Failed to send chunk for offset %u (ID: %u) after %d retries. Aborting update.\n", currentOffset, currentPacketId, MAX_RETRIES);
      // updateFile.close();
      return false;
    }

    currentOffset += bytesRead;
    currentPacketId++;

    // Voortgangsprinter, etc.
  }

  http.end();
  Debugln("Finished sending update via ESP-NOW from HTTP stream.");
  return true;
}

char updateURL[60],updateFile[35];

void handleP2P(){
  if ( !P2PType ) return;
  switch ( P2PType ) {
    case UPD_VER_REQ: {
      client_ota_data.version[0] = '\0';
      ReadManifest(client_ota_data.manifest);
      client_ota_data.type = UPD_VER_RSP;
      esp_err_t result = esp_now_send(NULL, (uint8_t *)&client_ota_data, sizeof(client_ota_data));
      Debugf("[OTA] Version request sent to master: %s -> %s\n",
                client_ota_data.manifest,
                (result == ESP_OK) ? "OK" : "FAILED");
      break;}
    case UPD_GO_UPDATE:
      Debugln("handleP2P -> UPD_GO_UPDATE");
      sprintf(updateFile,client_ota_data.file,client_ota_data.version);
      Debugln(updateFile);
      sprintf(updateURL, "http://%s%s",client_ota_data.update_url,updateFile);
      Debugln(updateURL);
      if ( !sendUpdateViaESPNOWFromHTTP(updateURL) ) Debugln("Error in p2p update proces");
      break;
  }
  P2PType = 0;
}

#endif //_ESPNOW
#else
  void StartESPNOW(){}
  void handleP2P(){}
#endif //ESPNOW
