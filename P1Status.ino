#include <Preferences.h>
Preferences preferences;

#define Eaddr   0
#define Esize  50
#define Ypos   30

void P1StatusBegin(){ 
    preferences.begin("P1Status", false);
    P1StatusRead();
    P1Status.reboots++;
    preferences.putUInt("reboots", P1Status.reboots);
    P1StatusPrint(); //print nieuwe data
//    DebugTln(F("P1 Status succesfully initialised"));
}

void P1StatusEnd(){
  preferences.end();
}

void P1StatusPrint(){
      DebugTf("P1 Status: reboots[%i] | sloterrors [%i] | Timestamp [%s] | water [%i] m3 [%i] liter\n",P1Status.reboots, P1Status.sloterrors, P1Status.timestamp, P1Status.wtr_m3, P1Status.wtr_l);
  }
  
  void P1StatusRead(){
//    uint32_t msec = millis();
    /*  
    size_t schLen = preferences.getBytes("Status", NULL, NULL);
    char buffer[schLen]; // prepare a buffer for the data
    preferences.getBytes("Status", buffer, schLen);
    */

    P1Status.reboots = preferences.getUInt("reboots", 0);
    P1Status.sloterrors = preferences.getUInt("sloterrors", 0);
    preferences.getString("timestamp",P1Status.timestamp,14);
    P1Status.wtr_m3 = preferences.getShort("wtr_m3", 0);
    P1Status.wtr_l = preferences.getUInt("wtr_l", 0);
    
    if (strlen(P1Status.timestamp)!=13) strcpy(P1Status.timestamp,"010101010101X"); 
    strcpy(actTimestamp, P1Status.timestamp);
//    DebugT("P1StatusRead duration: "); Debugln(millis() - msec);
    P1StatusPrint();
}

  void P1StatusWrite(){
    strcpy(P1Status.timestamp, actTimestamp);
//    uint32_t msec = millis();

    //EEPROM.put(Eaddr, P1Status);
    preferences.putUInt("reboots", P1Status.reboots);
    preferences.putUInt("sloterrors", P1Status.sloterrors);
    preferences.putString("timestamp",P1Status.timestamp);
    preferences.putShort("wtr_m3", P1Status.wtr_m3);
    preferences.putUInt("wtr_l", P1Status.wtr_l);
//    DebugT("P1StatusWrite duration: "); Debugln(millis() - msec);
    DebugTln(F("P1Status successfully writen"));
    //if (EEPROM.commit()) DebugTln(F("P1Status successfully writen to EEPROM"));
    //else DebugTln(F("ERROR! EEPROM commit failed"));      
}

  void P1StatusReset(){
//    for (int i = 0; i < Esize; i++) EEPROM.write(i, 255);
    if (preferences.clear()) DebugTln(F("P1Status successfully writen to preferences"));
    else DebugTln(F("ERROR! EEPROM commit failed"));   
}
  
void ReadEepromBlock(){
//  byte value;
//  for(byte address = 0; address < Esize; address++){
//    value = EEPROM.read(address);
//    Debugf("EEPROM - %i \t", address);Debug(value, DEC);Debug(" - ");Debugln((char)value);
//  }
}

    
