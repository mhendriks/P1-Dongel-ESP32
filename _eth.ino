#ifdef ETHERNET

#include <ETH.h>
#include <SPI.h>
// #include <WebServer_ESP32_SC_W5500.h>

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
// To be called before ETH.begin()
  // ESP32_W5500_onEvent();
  WiFi.onEvent(NetworkEvent); //general = also needed for the WifiProv
  SPI.begin(SCK_GPIO, MISO_GPIO, MOSI_GPIO);
  ETH.enableIPv6();
  ETH.begin(ETH_TYPE, ETH_ADDR, CS_GPIO, INT_GPIO, ETH_RST, SPI);
  
  // start the ethernet connection and the server:
  // Use DHCP dynamic IP and random mac
//  uint16_t index = millis() % NUMBER_OF_MAC;

  //bool begin(int MISO_GPIO, int MOSI_GPIO, int SCLK_GPIO, int CS_GPIO, int INT_GPIO, int SPI_CLOCK_MHZ,
  //           int SPI_HOST, uint8_t *W5500_Mac = W5500_Default_Mac);
  //ETH.begin( MISO_GPIO, MOSI_GPIO, SCK_GPIO, CS_GPIO, INT_GPIO, SPI_CLOCK_MHZ, ETH_SPI_HOST );
  // ETH.begin( MISO_GPIO, MOSI_GPIO, SCK_GPIO, CS_GPIO, INT_GPIO, SPI_CLOCK_MHZ, ETH_SPI_HOST );

  // Static IP, leave without this line to get IP via DHCP
  //bool config(IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dns1 = 0, IPAddress dns2 = 0);
#ifdef FIXED_IP
  ETH.config(myIP, myGW, mySN, myDNS);
#endif
  // ESP32_W5500_waitForConnect();

//  PostMacIP(); //post mac en ip 

//  USBPrint(F("ETH IP : ")); USBPrintln(IP_Address());
//  USBSerial.print(F("ETH MAC: ")); //CDC output
//  USBSerial.println(macStr);
  
  // SwitchLED( LED_ON, LED_BLUE ); //Ethernet available = RGB LED Blue
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
      // if ( WifiProvBusy ) wifi_prov_mgr_stop_provisioning();
      DebugTf("ETH Got IP: '%s'\n", esp_netif_get_desc(info.got_ip.esp_netif));
      ETH.printTo(Serial);
      // write2Log("NETWORK","Eth got ip", true);
//      ETH.printInfo(Serial); //3.0.0 RC
//      Debug("MAC: ");Debugln(ETH.macAddress());
      // LED1.Switch( BLUE, LED_ON );
        SwitchLED( LED_ON, LED_BLUE ); //Ethernet available = RGB LED Blue
      // netw_connected = true;
      // tLastConnect = 0;
      break;
    case ARDUINO_EVENT_ETH_LOST_IP:
      DebugTln("!!! ETH Lost IP");
      // netw_connected = false;
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
//      DebugTln("!!! ETH Disconnected");
      // write2Log("NETWORK","Eth disconneted", true);
      // if ( Update.isRunning() ) Update.abort();
      // LED1.Switch(RED,LED_ON);
      // netw_connected = false;
      break;
    case ARDUINO_EVENT_ETH_STOP:
      DebugTln("!!! ETH Stopped");
      // netw_connected = false;
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
