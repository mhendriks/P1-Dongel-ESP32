#ifndef _BUTTON_H
#define _BUTTON_H

void P1Reboot();
void FacReset();

enum btn_states { BTN_PRESSED, BTN_RELEASED };
enum btn_types { BTN_NONE, BTN_LONG_PRESS, BTN_SHORT_PRESS };

volatile btn_states lastState = BTN_RELEASED;
volatile btn_types ButtonPressed = BTN_NONE;

void IRAM_ATTR isrButton() {
  btn_states buttonState = (btn_states)digitalRead(IO_BUTTON);
  if (buttonState != lastState) {
    portENTER_CRITICAL_ISR(&mux);
    // Debugln(buttonState ? "Button Released" : "Button Pressed");
    lastState = buttonState;
    if ( buttonState == BTN_PRESSED ) {
      Tpressed = millis();
    }
    else {
      time_t TimePressed = millis() - Tpressed;
      // Debugf("Button time: %d \n", TimePressed);
      ButtonPressed = (TimePressed > 5000) ? BTN_LONG_PRESS : BTN_SHORT_PRESS;
    }
    portEXIT_CRITICAL_ISR(&mux);
  }
}

void ResetButton(){
  ButtonPressed = BTN_NONE;
}

//===========================================================================================

void handleButtonPressed(){
  btn_types btn;
  portENTER_CRITICAL(&mux);
  btn = ButtonPressed;
  ButtonPressed = BTN_NONE;
  portEXIT_CRITICAL(&mux);

  if (btn != BTN_NONE) {
    if ( btn == BTN_SHORT_PRESS ) {
#ifdef ESPNOW      
      DebugT(F("\n\nSHORT Press : PARING MODE - "));
      if ( Pref.peers ) {Debugln(" already PAIRED -> exit"); return;}
      if ( bPairingmode ) bPairingmode = 0;
      else bPairingmode = millis();
      Debugln(bPairingmode ? "ON" : "OFF");
#else 
      P1Reboot();
#endif      
    }
    else if ( btn == BTN_LONG_PRESS ) {
      DebugTln(F("\n\nButton LONG Press = Factory Reset"));
      FacReset();
    }
  }
}

void handleButton(){
  if ( ButtonPressed != BTN_NONE) handleButtonPressed();
}

//Aux processor task
void fAuxProc( void * pvParameters ){
  DebugTln(F("Starting Button handler"));
  pinMode( IO_BUTTON, INPUT_PULLUP );
  //TODO gpio_new_pin_glitch_filter(?);
  attachInterrupt( digitalPinToInterrupt(IO_BUTTON), isrButton, CHANGE );
  DebugTln(F("BUTTON setup completed"));

  while(true) {
    handleButton();
    vTaskDelay(25 / portTICK_PERIOD_MS);
  }
  // write2Log("TASK", "unexpected task exit fAuxProc",true);
  Debugln("unexpected task exit fAuxProc");
  vTaskDelete(NULL);
}

void SetupButton() {
  if( xTaskCreatePinnedToCore( fAuxProc, "Aux", 1024*5, NULL, 2, NULL, 0) == pdPASS ) DebugTln(F("Task Aux succesfully created"));
  // gpio_dump_io_configuration(stdout, 1ULL << 0);
}

#endif
