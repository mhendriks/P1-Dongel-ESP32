#ifndef _P1STATUS_H
#define _P1STATUS_H
#include <Preferences.h>
Preferences preferences;

void P1StatusBegin(){ 
    preferences.begin("P1Status", false);
    P1StatusRead();
    P1Status.reboots++;
    preferences.putUInt("reboots", P1Status.reboots);

#ifdef DEVTYPE_H2OV2
    P1Status.dev_type = PRO_H20_2;    
    P1SetDevType();  
#endif                      

    P1StatusPrint(); //print nieuwe data
}

void P1StatusEnd(){
  preferences.end();
}

void P1StatusPrint(){
      DebugTf("P1 Status: reboots[%i] | sloterrors [%i] | Timestamp [%s] | water [%i] m3 [%i] liter\n",P1Status.reboots, P1Status.sloterrors, P1Status.timestamp, P1Status.wtr_m3, P1Status.wtr_l);
  }

void P1StatusDefType(){
  Debugln(F("Devices types:\n0 = Pro\n1 = Pro Bridge\n2 = Pro Ethernet\n3 = Pro H2O+"));
  Debug(F("Device Type     : "));Debugln(P1Status.dev_type);
  Debug(F("Device First Use: "));Debugln(P1Status.FirstUse);
}
  
void P1StatusRead(){
    P1Status.reboots = preferences.getUInt("reboots", 0);
//    P1Status.sloterrors = preferences.getUInt("sloterrors", 0);
    preferences.getString("timestamp",P1Status.timestamp,14);
    P1Status.wtr_m3 = preferences.getShort("wtr_m3", 0);
    P1Status.wtr_l = preferences.getUInt("wtr_l", 0);
#ifdef ETHERNET
    P1Status.dev_type = PRO_ETH;
#else
    P1Status.dev_type = preferences.getUShort("dev_type", PRO);
#endif    
    P1Status.FirstUse = preferences.getBool("first_use", false);
    if (strlen(P1Status.timestamp)!=13) strcpy(P1Status.timestamp,"010101010101X"); 
    strcpy(actTimestamp, P1Status.timestamp);
#ifdef EID
    P1Status.eid_state = (E_eid_states) preferences.getUShort("eid_state", EID_IDLE);
#endif
    P1StatusPrint();
}

void P1SetDevFirstUse(bool first_use){
  P1Status.FirstUse = first_use;
  preferences.putBool("first_use", first_use);
}

void P1SetDevType(){
   preferences.putUShort("dev_type", P1Status.dev_type);
   SetConfig();
}

void P1StatusWrite(){
    strcpy(P1Status.timestamp, actTimestamp);
    preferences.putUInt("reboots", P1Status.reboots);
//    preferences.putUInt("sloterrors", P1Status.sloterrors);
    preferences.putString("timestamp",P1Status.timestamp);
    preferences.putShort("wtr_m3", P1Status.wtr_m3);
    preferences.putUInt("wtr_l", P1Status.wtr_l);
#ifdef EID    
    preferences.putUShort("eid_state", (uint16_t)P1Status.eid_state);
#endif
    DebugTln(F("P1Status successfully writen"));
}

void P1StatusReset(){
    if (preferences.clear()) DebugTln(F("P1Status cleared"));
    else DebugTln(F("ERROR! EEPROM commit failed"));   
}

void P1StatusClear(){
//  P1Status.sloterrors = 0;
  P1Status.reboots    = 0;
  P1Status.wtr_m3     = 0;
  P1Status.wtr_l      = 0;
  P1Status.FirstUse   = false;
  telegramCount       = 0;
  telegramErrors      = 0;
  mqttCount           = 0;
#ifdef EID
  P1Status.eid_state  = EID_IDLE;
#endif  
  P1StatusWrite();
}

#endif
