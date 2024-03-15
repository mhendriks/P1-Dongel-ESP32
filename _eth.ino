#ifdef ETHERNET

#include <WebServer_ESP32_SC_W5500.h>

void startETH(){

// To be called before ETH.begin()
  ESP32_W5500_onEvent();

  ETH.begin( MISO_GPIO, MOSI_GPIO, SCK_GPIO, CS_GPIO, INT_GPIO, SPI_CLOCK_MHZ, ETH_SPI_HOST );
  ETH.setHostname(settingHostname);
  
  // Static IP, leave without this line to get IP via DHCP
  //bool config(IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dns1 = 0, IPAddress dns2 = 0);
  //ETH.config(myIP, myGW, mySN, myDNS);

  ESP32_W5500_waitForConnect();

  PostMacIP(); //post mac en ip 
  
  USBSerial.print(F("ETH IP : ")); //CDC output
  USBSerial.println(IP_Address());
  USBSerial.print(F("ETH MAC: ")); //CDC output
  USBSerial.println(MAC_Address());
  
  SwitchLED( LED_ON, LED_BLUE ); //Ethernet available = RGB LED Blue
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
