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
