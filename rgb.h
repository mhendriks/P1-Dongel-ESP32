#include "esp32-hal-rgb-led.h"
/*
 * STATES
 * Rood           = IP verbinding verbroken
 * groene knipper = successvol lezen p1
 * Blauw          = Ethernet connectie + ip adres
 *
 */

enum rgbcolor { RED, GREEN, BLUE, BLACK, ORANGE };
enum switchmode { LED_ON, LED_OFF, TOGGLE};

class RGBLed {
  public:
    RGBLed(){};
    void begin(int pin, byte brightn = 50 ) {
      _pin = pin;
      brightness = brightn;
    };
    void begin(){};
    void SetBrightness(int val){ brightness = val; }
    void clearRGB_state(){ for ( byte i = 0; i < 3;i++) RGB_state[i]=0; }
    void Switch( rgbcolor color, switchmode mode ){
      //toggle not implemented yet
      if ( mode != TOGGLE ) state = mode;
      else state != state;
      // groen is p1 gelezen ... los van blauw of rood
        if ( color == GREEN ) {
          if ( mode == LED_ON ) RGB_state[GREEN] = brightness;
          else RGB_state[GREEN] = 0;
        } else {
          clearRGB_state();
          if ( mode == LED_ON ){
            switch ( color ){
              case RED:
              case BLUE:
                RGB_state[color] = brightness;
                break;
              case ORANGE:
                RGB_state[RED] = 51;
                RGB_state[GREEN] = 18;
                RGB_state[BLUE] = 1;
                break;
              case BLACK:
    //            clearRGB_state();
                break;
            } //switch
         } // else mode
      } //else
      rgbLedWrite( _pin , RGB_state[RED], RGB_state[GREEN], RGB_state[BLUE] );
    }

  private:
    byte     _pin = 8; //default 8
    byte     brightness = 255; //0 - 255 default 255
    switchmode state = LED_OFF;
    int16_t  RGB_state[3]; //r g b states

} rgb;


void SetupRGB(){
      rgb.begin(RGBLED_PIN, BRIGHTNESS );
      // sign of life = ON during setup or change config
      rgb.Switch( BLUE, LED_ON );
      delay(1500);
      rgb.Switch( BLUE, LED_OFF );
}
