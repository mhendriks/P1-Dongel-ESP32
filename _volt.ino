void PrintVarr(){
  char buffer[DATA_RECLEN_V+1];    
  DebugT(F("Current Max Voltage: "));Debugln(MaxVoltage);
  for ( byte i = 0; i < VmaxSlots; i++){
  
  //"%-12.12s,%3.1f,%3.1f,%3.1f,%3d"
   snprintf(buffer, sizeof(buffer), (char*)DATA_FORMAT_V, MaxVarr[i].ts , (uint16_t)MaxVarr[i].L1, (uint16_t)MaxVarr[i].L2, (uint16_t)MaxVarr[i].L3, (uint16_t)MaxVarr[i].MaxV, (char) MaxVarr[i].Over );
   Debugln(buffer);
//   Debug(MaxVarr[i].L1);Debug(",");
//   Debug(MaxVarr[i].L2);Debug(",");
//   Debugln(MaxVarr[i].Over);
  }
}

//-------------------------------------------------

void SaveVoltage(){
  //todo
}

void StoreVoltage(bool over){
  strncpy(MaxVarr[Vcount].ts, actTimestamp, 12);
  MaxVarr[Vcount].L1    = (uint16_t) DSMRdata.voltage_l1 * 10;
  MaxVarr[Vcount].L2    = (uint16_t) DSMRdata.voltage_l2 * 10;
  MaxVarr[Vcount].L3    = (uint16_t) DSMRdata.voltage_l3 * 10;
  MaxVarr[Vcount].MaxV  = MaxVoltage;
  MaxVarr[Vcount].Over  = over?'>':'<';
  
  Vcount++;
  if ( Vcount == VmaxSlots ) Vcount = 0;
} //StoreVoltage

//-------------------------------------------------

void ProcessMaxVoltage(){
  if ( MaxVoltage < 207 || !DSMRdata.voltage_l1_present ) return;
  if ( DSMRdata.voltage_l1 >= MaxVoltage || DSMRdata.voltage_l2 >= MaxVoltage || DSMRdata.voltage_l3 >= MaxVoltage ){
     if ( !bMaxV ) {
        bMaxV = true;
        StoreVoltage(true);
     }
  } else if ( bMaxV ) {
    bMaxV = false;
    StoreVoltage(false);
  }
} //ProcessMaxVoltage

#ifdef VOLTAGE

/*
 * Bijhouden spanning
 * overspanning situatie per fase
 * opgeven maximale spanning (cut off = 253 default)
 * balancing ook meenemen balancing eens per 5 minuten ... per uur, per dag
 * verloop van afgelopen 400 samples onthouden (eens per 30 sec een sample)
 * 
 */

int iVSlots = 0;
DECLARE_TIMER_SEC(VTimer, 30); 

void CreateRingVoltage(){
   File RingFile = LittleFS.open(RingFiles[RINGVOLTAGE].filename, "w"); // open for writing  
  if (!RingFile) {
    DebugT(F("open ring file FAILED!!! --> Bailout\r\n"));Debugln(RingFiles[RINGVOLTAGE].filename);
    return;
  }
  //write json to ring file
  DebugT(F("created file: ")); Debugln(RingFiles[RINGVOLTAGE].filename);
    
  RingFile.close();
}

void WriteRingVoltage(){
  if ( !FSmounted || !LittleFS.exists("/volt")) return; //do nothing when FS not mounted or volt file not exists
  
  File RingFile = LittleFS.open(RingFiles[RINGVOLTAGE].filename, "r+"); // open for reading  
  if ( !RingFile ) {
    DebugT(F("open ring file FAILED!!! --> Bailout\r\n"));
    Debugln(RingFiles[RINGVOLTAGE].filename);
    RingFile.close();
    return;
  }
  
    //write actual data
    char key[13] = "";
    char buffer[DATA_RECLEN_V];

    strncpy(key, actTimestamp, 12);  
//    DebugTln("actslot: "+actSlot);
//    DebugT(F("update date: "));Debugln(key);
    //create record
    snprintf(buffer, sizeof(buffer), (char*)DATA_FORMAT_V, key , (float)DSMRdata.voltage_l1, (float)DSMRdata.voltage_l2, (float)DSMRdata.voltage_l3);
  
  //DebugT("update timeslot: ");Debugln(slot);
  //goto writing starting point  
  uint16_t offset = (iVSlots * DATA_RECLEN_V);
  RingFile.seek(offset, SeekSet);   
 
  //overwrite record in file
  int32_t bytesWritten = RingFile.println(buffer);
 
  RingFile.close();
  iVSlots = (iVSlots + 1) % V_SLOTS; // it is a ring file ;-)
}

void ProcessVoltage(){
  if ( DUE(VTimer) ) WriteRingVoltage();
}

#endif
