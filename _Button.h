#ifndef _BUTTON_H
#define _BUTTON_H

void P1Reboot();
void FacReset();

void  IRAM_ATTR isrButton() {
    if ( digitalRead(IO_BUTTON) == LOW ) {
  //    Serial.println("Button PRESSED");
      Tpressed = millis();
      bButtonPressed =  false;
    } else {
  //    Serial.println("Button RELEASED");
      Tpressed = (millis() - Tpressed);
      bButtonPressed =  true;  
    }
  }

class Button {

public: 
   void begin(uint8_t pin){
      pinMode(pin, INPUT_PULLUP);
      attachInterrupt(pin, isrButton, CHANGE);
      DebugTln(F("BUTTON setup completed"));
    }
  
  void handler(){
    if ( !bButtonPressed ) return;
    bButtonPressed =  false;
    Debug("Button pressed: ");Debugln(Tpressed);
    if ( Tpressed  > 5000 ) {   
      DebugTln(F("Button LONG Press = Factory Reset"));
      FacReset();
    }
    else {
      // DebugTln(F("Button SHORT Press = Reboot"));
      LogFile("reboot: Button SHORT Press", true);
      P1Reboot();
    }
  }
  
private:
  
} PushButton;

#endif
