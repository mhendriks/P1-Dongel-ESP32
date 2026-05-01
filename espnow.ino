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
#include "espnow.h" // Voor CRC32 berekening, zorg dat deze bibliotheek geïnstalleerd is

// --- Globale variabelen voor ACK-mechanisme ---
volatile bool ackReceived = false;
volatile uint16_t lastAckPacketId = 0;
volatile bool lastAckSuccess = false;

command_t Command;
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
ActualData_t ActualData;
tariff_t TariffData;
char updateURL[60],updateFile[35];
bool bESPNowInit = false;

/*
  to activate call StartESPNOW in setup
  function is triggerd from :
  - peer ASKTARIFF and ASKSTATICS
  - when new p1 meter data is processing (send actual data)
  - changing statics or tariffs (sending new statics or tariff data) (todo)

  pairing done by pressing the dongle button once
  Once paired no other pairring could take place. repair by pressing the button again

  master is pretty chill normally, just waiting or sending p1 data. timeouts are handled by the peer

  todo: rssi check on pairing https://github.com/TenoTrash/ESP32_ESPNOW_RSSI/blob/main/Modulo_Receptor_OLED_SPI_RSSI.ino

*/
void PSPUpdatePlanner() {
  if ( !bNRGMenabled || !bESPNowInit ) return;
  P2PType = OFFSET_ACTION + ASK_PLANNER; 
}

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
  if (!incomingData || len <= 0) return;
  uint8_t typ = incomingData[0];

  auto need = [&](size_t n) {
    if ((size_t)len < n) { 
      Debugf("ESPNOW: drop typ=%u len=%d need=%u\n", typ, len, (unsigned)n);
      return false;
    }
    return true;
  };

  Debug("msgTyp: ");Debugln( typ );
  switch ( typ ){
    case COMMAND:
      if (!need(sizeof(command_t))) return;
      memcpy(&Command, incomingData, sizeof(command_t));
      Debugf("COMMAND RECEIVED type [%i], state [%i]\n", Command.action,bPairingmode);
      switch (Command.action) {
        case CONN_REQUEST:
          Debugln("CONN_REQUEST");
          if ( Pref.peers ) { //peer aanwezig
            Debugln("CONN_REQUEST: peer aanwezig");
            Command.action = CONN_RESPONSE;
            Command.channel = WiFi.channel(); //sent channel omdat een channel er naast soms ook werkt ... maar niet stabiel
            esp_now_send(NULL, (uint8_t *) &Command, sizeof(Command)); //ack to slave 
            en_connected = true;
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
          // SendTariffData();
          P2PType = OFFSET_ACTION + ASK_TARIF;
          break;  
        case ASK_STATIC:
          Debugln("ASK_STATIC");
          P2PType = OFFSET_ACTION + ASK_STATIC;
          // sendStatic();
          break; 
        case ASK_PLANNER:
          Debugln("ASK_PLANNER");
          P2PType = OFFSET_ACTION + ASK_PLANNER;
          break;                    
        case PAIRING:
          Debugln("PAIRING");
          memcpy(&Command, incomingData, sizeof(Command));
          // if ( bPairingmode && Pref.peers == 0 ) { //alleen indien peers = 0
          if ( bPairingmode ) { //alleen indien peers = 0
            String hostname = Command.host;
            Debugln("CONN_REQUEST: pair mode");
            if ( hostname.indexOf( PEER_NAME_CONTAINS ) != -1 ) {
              Debugln("CONN_REQUEST: pair mode en juiste hostname");
              memcpy(Pref.mac, info->src_addr,6);
              AddPeer(Pref.mac);
              bPairingmode = 0;
              Pref.peers = 1;
              P1StatusWrite();
              Command.action = PAIRING;
              Command.channel = WiFi.channel(); //sent channel omdat een channel er naast soms ook werkt ... maar niet stabiel ivm overlappende channels
              esp_now_send(NULL, (uint8_t *) &Command, sizeof(Command)); //ack to slave 
              en_connected = true;
            }
          } else Debugln("NOT IN PAIRING MODE");
          break;
      } //COMMAND
      break;
    case UPD_VER_REQ:
      if (!need(sizeof(client_ota_data))) return;
      Debugln("UPD_VER_REQ");
      memcpy(&client_ota_data, incomingData, sizeof(client_ota_data));
      P2PType = UPD_VER_REQ;
      break;
    case UPD_GO_UPDATE:
      if (!need(sizeof(client_ota_data))) return;
      Debug("Size UPD data frame: ");Debugln(sizeof(esp_now_update_data_t));
      memcpy(&client_ota_data, incomingData, sizeof(client_ota_data));
      P2PType = UPD_GO_UPDATE;
      break;
    case UPD_ACK:
      if (!need(sizeof(esp_now_ack_data_t))) return;
      procesACK(incomingData);
      break;
    case NRGTARIFS:
      if (!need(sizeof(tariff_t))) return;
      DebugTln("NRGTARIFS");
      memcpy(&TariffData, incomingData, sizeof(TariffData));
      P2PType = NRGTARIFS;
      // ReceiveTariffData();
      break;
  } //switch 
}

// void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {//3.2.x
void OnDataSent(const esp_now_send_info_t *info, esp_now_send_status_t status) { //3.3.x
#ifdef DEBUG  
  Debug("Last Packet Send Status: ");
  Debugln(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  // printMAC(mac_addr);
  // Debugln();
#endif  
  if ( status == ESP_NOW_SEND_SUCCESS ) en_error = 0; else en_error++;
}

void AddPeer( uint8_t * peer_mac ){
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, peer_mac, 6);
  peerInfo.channel = 0; // Use default channel
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
}

void StopESPNOW(){
  if ( !bESPNowInit ) return;

  esp_now_unregister_send_cb();
  esp_now_unregister_recv_cb();
  esp_now_deinit();
  bESPNowInit = false;
  en_connected = false;
  en_error = 0;
  P2PType = 0;
  bPairingmode = 0;
  Debugln("StartESPNOW: deinit OK");
}

void StartESPNOW(){
  if ( !bNRGMenabled ) {
    Debugln("StartESPNOW: skipped, NRG Monitor disabled");
    return;
  }
  if ( bESPNowInit ) {
    Debugln("StartESPNOW: already active");
    return;
  }
  if ( skipNetwork || bEthUsage || netw_state == NW_ETH || netw_state == NW_ETH_LINK ) {
    WiFi.mode(WIFI_STA);
  }
  Debugf("StartESPNOW: peers=%u nrgm=%u\n", Pref.peers, bNRGMenabled ? 1 : 0);
  if (esp_now_init() != ESP_OK) {
    Debugln("Error initializing ESP-NOW");
    bESPNowInit = false;
    return;
  } 

  // Register send callback function
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  AddPeer(broadcastAddress); //always add broadcast address
  Debugln("StartESPNOW: broadcast peer added");
  if ( Pref.peers ) {
    AddPeer(Pref.mac);
    Debugf("StartESPNOW: stored peer added [%02X:%02X:%02X:%02X:%02X:%02X]\n",
           Pref.mac[0], Pref.mac[1], Pref.mac[2], Pref.mac[3], Pref.mac[4], Pref.mac[5]);
  } else {
    Debugln("StartESPNOW: no stored peer");
  }

  bESPNowInit = true;
  Debugln("StartESPNOW: init OK");
}

void SyncESPNOW(){
  if ( bNRGMenabled ) StartESPNOW();
  else {
    StopESPNOW();
#ifdef ETHERNET
    if ( (skipNetwork || bEthUsage || netw_state == NW_ETH || netw_state == NW_ETH_LINK)
         && netw_state != NW_WIFI ) {
      WifiOff();
    }
#endif
  }
}

void sendStroomPlanner(){
  if ( !bNRGMenabled || !bESPNowInit ) {
    Debugln("Stroomplanner send skipped");
    return;
  }
  // check of available data and if EID is enabled
  // if ( StroomPlanData.size() == 0 ) return; //todo: change to let peer know the eid isnt available any more
  
  planner_t PlannerData;
  memset(&PlannerData, 0, sizeof(PlannerData));
  PlannerData.msgType = STROOMPLANNER;

  if ( !bEID_enabled || StroomPlanData.size() == 0 ) {
    DebugTln(F("EID: planner data empty"));
    PlannerData.rec[0].hour = 99; //sent disabled state to display
  } else {
    JsonArray dataArray = StroomPlanData["data"].as<JsonArray>();
    size_t plannerCount = dataArray.size();
    if (plannerCount == 0) return;

    const char* timestamp = dataArray[0]["timestamp"].is<const char*>() ? dataArray[0]["timestamp"].as<const char*>() : nullptr;
    if (!timestamp || strlen(timestamp) < 13) {
      DebugTln(F("EID: Ongeldige of ontbrekende timestamp"));
      return;
    }

    for (int i = 0; i < 10; i++ ){
      PlannerData.rec[i].hour = 255;
      PlannerData.rec[i].type = -1;

      if (i >= (int)plannerCount) continue;
      const char* ts = dataArray[i]["timestamp"].is<const char*>() ? dataArray[i]["timestamp"].as<const char*>() : nullptr;
      if (!ts || strlen(ts) < 13) continue;

      PlannerData.rec[i].hour = (ts[11] - '0') * 10 + (ts[12] - '0');
      const char* signal = dataArray[i]["signal"].is<const char*>() ? dataArray[i]["signal"].as<const char*>() : nullptr;
      PlannerData.rec[i].type = signalToEnum(signal);
      Debugf("! PLANNER - hour: %i - type : %i\n", PlannerData.rec[i].hour, PlannerData.rec[i].type);
    }
  }
  esp_now_send(NULL, (uint8_t *) &PlannerData, sizeof(PlannerData));
  Debugln("Stroomplanner data sent");

}

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
  Static.WpSolar = Enphase.Wp + SolarEdge.Wp;
  // SolarWpTotal();
  esp_err_t result = esp_now_send(NULL, (uint8_t *) &Static, sizeof(Static));
}

void ReceiveTariffData() {

  settingENBK = TariffData.Efixed / 100000.0f;
  settingGNBK = TariffData.Gfixed / 100000.0f;
  settingWNBK = TariffData.Wfixed / 100000.0f;

  settingEDT1 = TariffData.Et1 / 100000.0f;
  settingEDT2 = TariffData.Et2 / 100000.0f;

  settingERT1 = TariffData.Et1r / 100000.0f;
  settingERT2 = TariffData.Et2r / 100000.0f;

  settingGDT  = TariffData.G / 100000.0f;
  settingWDT  = TariffData.W / 100000.0f;

  //update settingsfile
  writeSettings();

#ifdef DEBUG
  Debugf("settingENBK : %.5f\n", settingENBK);
  Debugf("settingGNBK : %.5f\n", settingGNBK);
  Debugf("settingWNBK : %.5f\n", settingWNBK);

  Debugf("settingEDT1 : %.5f\n", settingEDT1);
  Debugf("settingEDT2 : %.5f\n", settingEDT2);
  Debugf("settingERT1 : %.5f\n", settingERT1);
  Debugf("settingERT2 : %.5f\n", settingERT2);

  Debugf("settingGDT  : %.5f\n", settingGDT);
  Debugf("settingWDT  : %.5f\n", settingWDT);
#endif
}

void SendTariffData(){
  if ( ! Pref.peers || ! bESPNowInit ) return;
  TariffData.msgType= NRGTARIFS;
  TariffData.Efixed = (uint32_t)round(settingENBK*100000.0);
  TariffData.Gfixed = (uint32_t)round(settingGNBK*100000.0);
  TariffData.Wfixed = (uint32_t)round(settingWNBK*100000.0);

  TariffData.Et1    = (uint32_t)round(settingEDT1*100000.0);
  TariffData.Et2    = (uint32_t)round(settingEDT2*100000.0);
  TariffData.Et1r   = (uint32_t)round(settingERT1*100000.0);
  TariffData.Et2r   = (uint32_t)round(settingERT2*100000.0);
  TariffData.G      = (uint32_t)round(settingGDT*100000.0);
  TariffData.W      = (uint32_t)round(settingWDT*100000.0);

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
  if ( ! Pref.peers || ! bESPNowInit || !bNRGMenabled || !en_connected ) {
    // Debugln("P2P sending actual blocked");
    return;
  }
  
  ActualData.epoch  = actT;
  ActualData.P      = DSMRdata.power_delivered.int_val();
  ActualData.Pr     = DSMRdata.power_returned.int_val() ;
  ActualData.e_t1   = DSMRdata.energy_delivered_tariff1.int_val() - dataYesterday.t1;
  ActualData.e_t2   = DSMRdata.energy_delivered_tariff2.int_val() - dataYesterday.t2;
  ActualData.e_t1r  = DSMRdata.energy_returned_tariff1.int_val() - dataYesterday.t1r;
  ActualData.e_t2r  = DSMRdata.energy_returned_tariff2.int_val() - dataYesterday.t2r;
  if ( mbusGas ) ActualData.Gas = gasDelivered * 1000 - dataYesterday.gas;
  else ActualData.Gas = UINT32_MAX; 
  if ( WtrMtr ) ActualData.Water  = (waterDelivered * 1000) - dataYesterday.water;
  else ActualData.Water = UINT32_MAX;
  ActualData.Esolar = Enphase.Daily + SolarEdge.Daily;
  
  if ( !Enphase.Available && !SolarEdge.Available ) ActualData.Psolar = UINT32_MAX;
  else ActualData.Psolar = Enphase.Actual + SolarEdge.Actual;
  
  // esp_err_t result = esp_now_send(NULL, (uint8_t *) &ActualData, sizeof(ActualData));
  esp_err_t rs = esp_now_send(NULL, (uint8_t *) &ActualData, sizeof(ActualData));
  if (rs != ESP_OK) Debugf("P2P actual send failed: %d\n", (int)rs);

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
    esp_task_wdt_reset();
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
      esp_task_wdt_reset();
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
        esp_task_wdt_reset();
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

void handleP2P(){
  if ( bPairingmode && (millis() - bPairingmode) > PAIR_TIMEOUT ) { 
    bPairingmode = 0; 
    Debugln("-P- Timeout disable PAIRING mode");
  }
  if ( en_error > 30 ) { en_connected = false; en_error = 0;} //prevent pushing data to a unconnected peer
  if ( !P2PType || bPairingmode ) return;
  switch ( P2PType ) {
    case UPD_VER_REQ: {
      client_ota_data.version[0] = '\0';
      JsonDocument manifest;
      if (ReadManifest(manifest, client_ota_data.manifest)) {
        const char* url = manifest["url"] | "";
        const char* ver = manifest["version"] | "";
        strlcpy(client_ota_data.update_url, url, sizeof(client_ota_data.update_url));
        strlcpy(client_ota_data.version, ver, sizeof(client_ota_data.version));
        Debugln(client_ota_data.update_url);
        Debugln(client_ota_data.version);
      }
      client_ota_data.type = UPD_VER_RSP;
      esp_err_t result = esp_now_send(NULL, (uint8_t *)&client_ota_data, sizeof(client_ota_data));
      Debugf("[OTA] Version request sent to master: %s -> %s\n",
                client_ota_data.manifest,
                (result == ESP_OK) ? "OK" : "FAILED");
      break;}
    case UPD_GO_UPDATE:
      Debugln("handleP2P -> UPD_GO_UPDATE");
      {
      String fileName = client_ota_data.file;
      int fmtPos = fileName.indexOf("%s");
      if (fmtPos >= 0) fileName.replace("%s", client_ota_data.version);
      strlcpy(updateFile, fileName.c_str(), sizeof(updateFile));
      }
      Debugln(updateFile);
      snprintf(updateURL, sizeof(updateURL),"http://%s%s",client_ota_data.update_url,updateFile);
      Debugln(updateURL);
      if ( !sendUpdateViaESPNOWFromHTTP(updateURL) ) Debugln("Error in p2p update proces");
      break;
    case OFFSET_ACTION + ASK_PLANNER:
      sendStroomPlanner();
      break;
    case NRGTARIFS:
      ReceiveTariffData();
      break;
    case OFFSET_ACTION + ASK_STATIC:
      sendStatic();
      break;
    case OFFSET_ACTION + ASK_TARIF:
      SendTariffData();
      break;
  }
  P2PType = 0;
}

#endif //_ESPNOW
#else
  void StartESPNOW(){}
  void StopESPNOW(){}
  void SyncESPNOW(){}
    void handleP2P(){}
#endif //ESPNOW
