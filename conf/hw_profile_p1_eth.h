#warning Using ESP32C3 - P1 Dongle Pro - ETHERNET

#define PROFILE "[ETH]"
#define _HOSTNAME           "Eth-Dongle-Pro"
#define _DEFAULT_HOSTNAME _HOSTNAME

#ifdef EID
	#define OTAURL "http://ota.smart-stuff.nl/eth-eid/"
#else
	#define OTAURL "http://ota.smart-stuff.nl/eth/"
#endif

//pinout P1 Dongle Pro v6.x
#define LED                   3
#define DTR_IO                0 // nr = IO pulse = N/A
#define RXP1                  7
#define TXP1                 -1 //disable
#define IO_WATER_SENSOR      -1
#define PRT_LED           	 -1
#define IO_BUTTON             9 //multifunctional button
#define AUX_BUTTON         	  9

#define RGBLED_PIN            8
#define BRIGHTNESS           50 // Set BRIGHTNESS to about 1/5 (max = 255)
#define TXO1                 -1
#define O1_DTR_IO            -1
#define P1_LED               -1
#define SSL_RAND              2

//ETH
#define ETH_IRQ            1
#define ETH_SPI_MISO       5
#define ETH_SPI_MOSI       6
#define ETH_SPI_SCK        4
#define ETH_CS            10

#define INT_GPIO            1
#define MISO_GPIO           5
#define MOSI_GPIO           6
#define SCK_GPIO            4
#define CS_GPIO             10


//MODBUS
// #define RTSPIN      		11
// #define RXPIN       		1
// #define TXPIN       		3
// #define BAUDRATE  		 9600