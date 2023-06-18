#ifdef ETHERNET

#define INT_GPIO            1
#define MISO_GPIO           5
#define MOSI_GPIO           6
#define SCK_GPIO            4
#define CS_GPIO             10

#include <WebServer_ESP32_SC_W5500.h>

// Select the IP address according to your local network
//IPAddress myIP(192, 168, 2, 232);
//IPAddress myGW(192, 168, 2, 1);
//IPAddress mySN(255, 255, 255, 0);

// Google DNS Server IP
IPAddress myDNS(8, 8, 8, 8);

void startETH(){
// To be called before ETH.begin()
  ESP32_W5500_onEvent();

  // start the ethernet connection and the server:
  // Use DHCP dynamic IP and random mac
//  uint16_t index = millis() % NUMBER_OF_MAC;

  //bool begin(int MISO_GPIO, int MOSI_GPIO, int SCLK_GPIO, int CS_GPIO, int INT_GPIO, int SPI_CLOCK_MHZ,
  //           int SPI_HOST, uint8_t *W5500_Mac = W5500_Default_Mac);
  //ETH.begin( MISO_GPIO, MOSI_GPIO, SCK_GPIO, CS_GPIO, INT_GPIO, SPI_CLOCK_MHZ, ETH_SPI_HOST );
  ETH.begin( MISO_GPIO, MOSI_GPIO, SCK_GPIO, CS_GPIO, INT_GPIO, SPI_CLOCK_MHZ, ETH_SPI_HOST );

  // Static IP, leave without this line to get IP via DHCP
  //bool config(IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dns1 = 0, IPAddress dns2 = 0);
  //ETH.config(myIP, myGW, mySN, myDNS);

  ESP32_W5500_waitForConnect();

  PostMacIP(); //post mac en ip 

  USBSerial.print(F("ETH IP : ")); //CDC output
  USBSerial.println(IP_Address());
  USBSerial.print(F("ETH MAC: ")); //CDC output
  USBSerial.println(MAC_Address());
  
  SwitchLED( LED_ON, BLUE ); //Ethernet available = RGB LED Blue
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
