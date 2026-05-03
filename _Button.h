#ifndef _BUTTON_H
#define _BUTTON_H

void P1Reboot();
void FacReset();
void SwitchLED(byte mode, uint32_t color);
void OverrideLED(bool enabled, uint32_t color);
void HandleLED();

enum btn_states { BTN_PRESSED, BTN_RELEASED };
enum btn_types { BTN_NONE, BTN_LONG_PRESS, BTN_SHORT_PRESS };

volatile btn_states lastState = BTN_RELEASED;
volatile btn_types ButtonPressed = BTN_NONE;
volatile uint32_t lastEdgeMs = 0;
volatile uint32_t Treleased = 0;
volatile bool bButtonReleased = false;
static uint32_t s_buttonIgnoreUntilMs = 0;
static const uint32_t DEBOUNCE_MS = 30;
static const uint32_t LONG_PRESS_MS = 5000;

void IRAM_ATTR isrButton() {
  const uint32_t now = millis();
  if (now - lastEdgeMs < DEBOUNCE_MS) return; // debounce beide flanken
  lastEdgeMs = now;

  btn_states buttonState = (btn_states)digitalRead(button_io);
  if (buttonState != lastState) {
    portENTER_CRITICAL_ISR(&mux);
    lastState = buttonState;
    if (buttonState == BTN_PRESSED) {
      Tpressed = now;
      bButtonPressed = true;
    } else {
      Treleased = now;
      bButtonPressed = false;
      bButtonReleased = true;
    }
    portEXIT_CRITICAL_ISR(&mux);
  }
}

void ResetButton(){
  ButtonPressed = BTN_NONE;
  bButtonPressed = false;
  bButtonReleased = false;
}

//===========================================================================================

void handleButtonPressed(btn_types btn){
  // Ignore spurious edges directly after enabling the ISR during boot.
  if ((int32_t)(millis() - s_buttonIgnoreUntilMs) < 0) return;

  if (btn != BTN_NONE) {
    if ( btn == BTN_SHORT_PRESS ) {
#ifdef ESPNOW
	  if ( allowSkipNetworkByButton && netw_state == NW_NONE && !skipNetwork ) { 
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
  bool released;
  uint32_t pressedAt;
  uint32_t releasedAt;

  portENTER_CRITICAL(&mux);
  released = bButtonReleased;
  pressedAt = Tpressed;
  releasedAt = Treleased;
  if (released) bButtonReleased = false;
  portEXIT_CRITICAL(&mux);

  if (released) {
    uint32_t TimePressed = releasedAt - pressedAt;
    ButtonPressed = (TimePressed > LONG_PRESS_MS) ? BTN_LONG_PRESS : BTN_SHORT_PRESS;
    handleButtonPressed(ButtonPressed);
    ButtonPressed = BTN_NONE;
  }
}

void handleButtonLedFeedback(){
  static uint32_t lastFeedbackColor = LED_BLACK;
  bool pressed;
  uint32_t pressedAt;

  portENTER_CRITICAL(&mux);
  pressed = bButtonPressed;
  pressedAt = Tpressed;
  portEXIT_CRITICAL(&mux);

  if (!pressed || (int32_t)(millis() - s_buttonIgnoreUntilMs) < 0) {
    if (lastFeedbackColor != LED_BLACK) {
      OverrideLED(false, LED_BLACK);
      lastFeedbackColor = LED_BLACK;
    }
    return;
  }

  uint32_t color = ((uint32_t)(millis() - pressedAt) > LONG_PRESS_MS) ? LED_RED : LED_GREEN;
  if (color != lastFeedbackColor) {
    OverrideLED(true, color);
    lastFeedbackColor = color;
  }
}

//Aux processor task
void fAuxProc(void *pvParameters) {
  DebugTln(F("Starting Button handler"));
  if (button_io < 0) {
    DebugTln(F("No button configured"));
    vTaskDelete(NULL);
    return;
  }

  pinMode(button_io, INPUT_PULLUP);

  // Init lastState op basis van huidige level
  lastState = (btn_states)digitalRead(button_io);
  Tpressed  = 0;
  Treleased = 0;
  bButtonPressed = false;
  bButtonReleased = false;
  s_buttonIgnoreUntilMs = millis() + 1500;

  attachInterrupt(digitalPinToInterrupt(button_io), isrButton, CHANGE);
  DebugTln(F("BUTTON setup completed"));

  while (true) {
    handleButtonLedFeedback();
    HandleLED();
    handleButton();
    vTaskDelay(25 / portTICK_PERIOD_MS);
  }
}

void SetupButton() {
  if( xTaskCreatePinnedToCore( fAuxProc, "Aux", 1024*5, NULL, 6, NULL, 0) == pdPASS ) DebugTln(F("Task Aux succesfully created"));
  // gpio_dump_io_configuration(stdout, 1ULL << 0);
}

#endif
