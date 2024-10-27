#ifdef ETHERNET

#include <ETH.h>
#include <SPI.h>
// #include <WebServer_ESP32_SC_W5500.h>

bool netw_connected = false;

#define ETH_TYPE            ETH_PHY_W5500
#define ETH_RST            -1
#define ETH_ADDR            1

#ifdef FIXED_IP
  // Select the IP address according to your local network
  IPAddress myIP(192, 168, 2, 232);
  IPAddress myGW(192, 168, 2, 1);
  IPAddress mySN(255, 255, 255, 0);
  // Google DNS Server IP
  IPAddress myDNS(8, 8, 8, 8);
#endif

void startETH(){
  WiFi.onEvent(NetworkEvent); //general = also needed for the WifiProv
  SPI.begin(SCK_GPIO, MISO_GPIO, MOSI_GPIO);
  ETH.enableIPv6();
  ETH.begin(ETH_TYPE, ETH_ADDR, CS_GPIO, INT_GPIO, ETH_RST, SPI);
  while ( !netw_connected ) {Debug(".");delay(500);};
  Debugln();
  
  #ifdef FIXED_IP
    ETH.config(myIP, myGW, mySN, myDNS);
  #endif
}

void NetworkEvent(arduino_event_id_t event, arduino_event_info_t info) {
  DebugT("Network event: "); Debugln(event);
  switch (event) {
//ETH    
    case ARDUINO_EVENT_ETH_START:
      DebugTln("ETH Started");
      // LED1.Switch( ORANGE, LED_ON );
      ETH.setHostname(_HOSTNAME);
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      DebugTln("ETH Connected");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      DebugTf("ETH Got IP: '%s'\n", esp_netif_get_desc(info.got_ip.esp_netif));
      ETH.printTo(Serial);
      SwitchLED( LED_ON, LED_BLUE ); //Ethernet available = RGB LED Blue
      netw_connected = true;
      break;
    case ARDUINO_EVENT_ETH_LOST_IP:
      DebugTln("!!! ETH Lost IP");
      netw_connected = false;
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      netw_connected = false;
      break;
    case ARDUINO_EVENT_ETH_STOP:
      DebugTln("!!! ETH Stopped");
      netw_connected = false;
      break;
    default:
      break;
  }
}

#endif

String IP_Address(){
#ifdef ETHERNET
  return ETH.localIP().toString();
#else
  return WiFi.localIP().toString();
#endif
}

String MAC_Address(){
#ifdef ETHERNET
  return ETH.macAddress();
#else
  return WiFi.macAddress();
#endif
}
