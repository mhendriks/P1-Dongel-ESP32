#ifndef _BUTTON_H
#define _BUTTON_H

void P1Reboot();
void FacReset();

enum btn_states { BTN_PRESSED, BTN_RELEASED };
enum btn_types { BTN_NONE, BTN_LONG_PRESS, BTN_SHORT_PRESS };

volatile btn_states lastState = BTN_RELEASED;
volatile btn_types ButtonPressed = BTN_NONE;
volatile uint32_t lastEdgeMs = 0;
static const uint32_t DEBOUNCE_MS = 30;

void IRAM_ATTR isrButton() {
  const uint32_t now = millis();
  if (now - lastEdgeMs < DEBOUNCE_MS) return; // debounce beide flanken
  lastEdgeMs = now;

  btn_states buttonState = (btn_states)digitalRead(IO_BUTTON);
  if (buttonState != lastState) {
    portENTER_CRITICAL_ISR(&mux);
    lastState = buttonState;
    if (buttonState == BTN_PRESSED) {
      Tpressed = now;                        // press start
    } else {
      uint32_t TimePressed = now - Tpressed; // press duur
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
	  if ( netw_state == NW_NONE && !skipNetwork ) { 
	  	skipNetwork = true; 
	  	LogFile( "SKIP NETWORK ENABLED",true );
	  	return; 
	  }
      DebugT(F("\n\nSHORT Press : PARING MODE - "));
      if ( Pref.peers ) {
        Debugln(" already PAIRED -> rePAIR"); 
        // return;
      }
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
  btn_types snapshot;
  portENTER_CRITICAL(&mux);
  snapshot = ButtonPressed;
  portEXIT_CRITICAL(&mux);
  if (snapshot != BTN_NONE) handleButtonPressed();
}

//Aux processor task
void fAuxProc(void *pvParameters) {
  DebugTln(F("Starting Button handler"));
  pinMode(IO_BUTTON, INPUT_PULLUP);

  // Init lastState op basis van huidige level
  lastState = (btn_states)digitalRead(IO_BUTTON);
  Tpressed  = 0; // <-- maak Tpressed ook 'volatile uint32_t' en init

  attachInterrupt(digitalPinToInterrupt(IO_BUTTON), isrButton, CHANGE);
  DebugTln(F("BUTTON setup completed"));

  while (true) {
    handleButton();
    vTaskDelay(25 / portTICK_PERIOD_MS);
  }
}

void SetupButton() {
  if( xTaskCreatePinnedToCore( fAuxProc, "Aux", 1024*5, NULL, 6, NULL, 0) == pdPASS ) DebugTln(F("Task Aux succesfully created"));
  // gpio_dump_io_configuration(stdout, 1ULL << 0);
}

#endif
