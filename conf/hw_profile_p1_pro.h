#warning Using ESP32C3 - P1 Dongle Pro

#define _HOSTNAME           "P1-Dongle-Pro"
#define _DEFAULT_HOSTNAME _HOSTNAME
#define OTAURL              "http://ota.smart-stuff.nl/v5/"

//pinout P1 Dongle Pro v6.x
#define LED                   7
#define DTR_IO                4 // nr = IO pulse = N/A
#define RXP1                 10
#define TXP1                 -1 //disable
#define IO_WATER_SENSOR       5
#define PRT_LED           	  0
#define IO_BUTTON             9 //multifunctional button
#define AUX_BUTTON         	  9

#define RGBLED_PIN            8
#define BRIGHTNESS           50 // Set BRIGHTNESS to about 1/5 (max = 255)
#define TXO1                 -1
#define O1_DTR_IO            -1
#define P1_LED               -1
#define SSL_RAND              1

