#ifdef AUX_BUTTON

void IRAM_ATTR iButton_pressed() {
  pressed++;
  Tpressed = millis();
  DebugTln(F("AUX_BUTTON pressed"));
}

void handleButtonPressed(){
  DebugTf("Button %d times pressed\n", pressed );
  switch(pressed){
  case 1: DebugTln("write ringfiles");
          writeRingFiles();
          break;
  case 2: DebugTln("reboot");
          P1Reboot();
          break;
  case 3: DebugTln("reset wifi");
          resetWifi();
          break;
  case 5: DebugTln("/!\\ factory reset");
          FacReset();
          break;
  }
  Tpressed = 0;
  pressed = 0;
}

#endif

void setupAuxButton() {
#ifdef AUX_BUTTON
  pinMode(AUX_BUTTON, INPUT_PULLUP);
  attachInterrupt(AUX_BUTTON, iButton_pressed, FALLING);
  DebugTln(F("AUX_BUTTON setup completed"));
#endif
}
