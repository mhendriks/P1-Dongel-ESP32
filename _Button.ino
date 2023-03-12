void handleButton(){
#ifdef AUX_BUTTON
  if ( Tpressed && ((millis() - Tpressed) > 1500 ) ) handleButtonPressed();
#endif
}

#ifdef AUX_BUTTON

void IRAM_ATTR iButton_pressed() {
  pressed++;
  Tpressed = millis();
  //Serial.println("AUX_BUTTON pressed"); // zo min mogelijk tijd tijdens een interupt gebruiken
}

void handleButtonPressed(){
  DebugTf("Button %d times pressed\n", pressed );
  switch(pressed){
  case 1: DebugTln(F("Update ringfiles"));
          writeRingFiles();
          break;
  case 2: DebugTln(F("Reboot"));
          P1Reboot();
          break;
  case 3: DebugTln(F("Reset wifi"));
          resetWifi();
          break;
  case 5: DebugTln(F("/!\\ Factory reset"));
          FacReset();
          break;
  case 8: DebugTln(F("Firmware update naar laaste versie"));
          RemoteUpdate("4-sketch-latest", true);
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
  DebugTln(F("BUTTON setup completed"));
//  USBSerial.println(F("BUTTON setup completed"));
#endif
}
