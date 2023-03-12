/* 
***************************************************************************  
**  Program  : JsonCalls, part of DSMRloggerAPI
**  Version  : v4.2.1
**
**  Copyright (c) 2023 Martijn Hendriks
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

void CheckRingExists(){
  for (byte i = 0; i< 3; i++){
    if ( !LittleFS.exists(RingFiles[i].filename) ) {
      createRingFile( (E_ringfiletype)i );
      P1SetDevFirstUse( true ); //when one or more files are missing first use mode triggers
      DebugTln(F("First Use mode"));
    }
  }
}
//===========================================================================================

void createRingFile(E_ringfiletype ringfiletype) 
{
  File RingFile = LittleFS.open(RingFiles[ringfiletype].filename, "w"); // open for writing  
  if (!RingFile) {
    DebugT(F("open ring file FAILED!!! --> Bailout\r\n"));Debugln(RingFiles[ringfiletype].filename);
    return;
  }
  //write json to ring file
  DebugT(F("createFile: ")); Debugln(RingFiles[ringfiletype].filename);
    
  //fill file with 0 values
  RingFile.print("{\"actSlot\": 0,\"data\":[\n"); //start the json file 
  for (uint8_t slot=0; slot < RingFiles[ringfiletype].slots; slot++ ) 
  { 
    //{"date":"20000000","values":[     0.000,     0.000,     0.000,     0.000,     0.000,     0.000]}
    RingFile.print("{\"date\":\"20000000\",\"values\":[     0.000,     0.000,     0.000,     0.000,     0.000,     0.000]}"); // one empty record
   if (slot < (RingFiles[ringfiletype].slots - 1) ) RingFile.print(",\n");

  }
  if (RingFile.size() != RingFiles[ringfiletype].f_len) {
    DebugTln(F("ringfile size incorrect"));
    //todo log error to logfile
  }
  RingFile.print("\n]}"); //terminate the json string
  RingFile.close();
}

//===========================================================================================

void createRingFile(String filename) 
{
  if (filename==RingFiles[0].filename) createRingFile(RINGHOURS);
  else if (filename==RingFiles[1].filename) createRingFile(RINGDAYS);
  else if (filename==RingFiles[2].filename) createRingFile(RINGMONTHS);
}

//===========================================================================================
// Calc slot based on actT

uint8_t CalcSlot(E_ringfiletype ringfiletype, bool prev_slot) 
{
  uint32_t  nr=0;
  if (ringfiletype == RINGMONTHS ) nr = ( (year(actT) -1) * 12) + month(actT);    // eg: year(2023) * 12 = 24276 + month(9) = 202309
  else nr = actT / RingFiles[ringfiletype].seconds;
  if ( prev_slot ) nr--;
  uint8_t slot = nr  % RingFiles[ringfiletype].slots;
  DebugTf("slot: Slot is [%d] nr is [%d]\r\n", slot, nr);

  return slot;
}

//===========================================================================================

uint8_t CalcSlot(E_ringfiletype ringfiletype, char* Timestamp) 
{
  //slot positie bepalen
  uint32_t  nr=0;
  time_t    t1 = epoch(Timestamp, strlen(Timestamp), false);
  if (ringfiletype == RINGMONTHS ) nr = ( (year(t1) -1) * 12) + month(t1);    // eg: year(2023) * 12 = 24276 + month(9) = 202309
  else nr = t1 / RingFiles[ringfiletype].seconds;
  uint8_t slot = nr % RingFiles[ringfiletype].slots;
  DebugTf("slot: Slot is [%d] nr is [%d]\r\n", slot, nr);

  return slot;
}

//===========================================================================================

void writeRingFile(E_ringfiletype ringfiletype,const char *JsonRec) {
  uint8_t slot = CalcSlot(ringfiletype, false);
  writeRingFile(ringfiletype, JsonRec, slot);
}

//===========================================================================================

void writeRingFile(E_ringfiletype ringfiletype,const char *JsonRec, byte actSlot) 
{
  if ( !EnableHistory || !FSmounted ) return; //do nothing
  char key[9] = "";
  byte slot = 0;
//  uint8_t actSlot = CalcSlot(ringfiletype, false);
//  if ( actSlot == 99 ) return;  // stop if error occured
  StaticJsonDocument<145> rec;

  char buffer[DATA_RECLEN];
  if (strlen(JsonRec) > 1) {
    DebugTln(JsonRec);
    DeserializationError error = deserializeJson(rec, JsonRec);
    if (error) {
      DebugT(F("convert:Failed to deserialize RECORD: "));Debugln(error.c_str());
      httpServer.send(500, "application/json", httpServer.arg(0));
      return;
    } 
  }

  //json openen
  DebugT(F("read(): Ring file ")); Debugln(RingFiles[ringfiletype].filename);
  
  File RingFile = LittleFS.open(RingFiles[ringfiletype].filename, "r+"); // open for reading  
  if (!RingFile || (RingFile.size() != RingFiles[ringfiletype].f_len)) {
    DebugT(F("open ring file FAILED!!! --> Bailout\r\n"));
    Debugln(RingFiles[ringfiletype].filename);
    RingFile.close();
    return;
  }

  // add/replace new actslot to json object
  snprintf(buffer, sizeof(buffer), "{\"actSlot\":%2d,\"data\":[\n", actSlot);
  RingFile.print(buffer);
  
  if (strlen(JsonRec) > 1) {
    //write data from onerecord
    strncpy(key, rec["recid"], 8); 
    slot = CalcSlot(ringfiletype, key);
//    DebugTln("slot from rec: "+slot);
//    DebugT(F("update date: "));Debugln(key);

    //create record
    snprintf(buffer, sizeof(buffer), (char*)DATA_FORMAT, key , (float)rec["edt1"], (float)rec["edt2"], (float)rec["ert1"], (float)rec["ert2"], (float)rec["gdt"], (float)rec["wtr"]);
    httpServer.send(200, "application/json", httpServer.arg(0));
  } else {
    //write actual data
    strncpy(key, actTimestamp, 8);  
    slot = actSlot;
//    DebugTln("actslot: "+actSlot);
//    DebugT(F("update date: "));Debugln(key);
    //create record
#ifdef SE_VERSION
    snprintf(buffer, sizeof(buffer), (char*)DATA_FORMAT, key , (float)DSMRdata.energy_delivered_total, 0, (float)DSMRdata.energy_returned_total, 0, (float)gasDelivered, (float)P1Status.wtr_m3+(float)P1Status.wtr_l/1000.0);
#else    
    snprintf(buffer, sizeof(buffer), (char*)DATA_FORMAT, key , (float)DSMRdata.energy_delivered_tariff1, (float)DSMRdata.energy_delivered_tariff2, (float)DSMRdata.energy_returned_tariff1, (float)DSMRdata.energy_returned_tariff2, (float)gasDelivered, (float)P1Status.wtr_m3+(float)P1Status.wtr_l/1000.0);
#endif    
  }
  //DebugT("update timeslot: ");Debugln(slot);
  //goto writing starting point  
  uint16_t offset = (slot * DATA_RECLEN) + JSON_HEADER_LEN;
  RingFile.seek(offset, SeekSet);   
 
  //overwrite record in file
  int32_t bytesWritten = RingFile.print(buffer);
  if (bytesWritten != (DATA_RECLEN - 2)) DebugTf("ERROR! slot[%02d]: written [%d] bytes but should have been [%d]\r\n", slot, bytesWritten, DATA_RECLEN);
  if ( slot < (RingFiles[ringfiletype].slots -1)) RingFile.print(",\n");
  else RingFile.print("\n"); // no comma at last record
  
  RingFile.close();
//    String log_temp = "Ringfile " + String(RingFiles[ringfiletype].filename) + " writen. actT:[" + String(actT) + "] newT:[" + String(newT) +"] ActSlot:[" + String(slot) + "]";
//    LogFile(log_temp.c_str(),true);
} // writeRingFile()

//===========================================================================================
void WritePrevRingRecord(E_ringfiletype ringfiletype){
  uint8_t slot = CalcSlot(ringfiletype, true);
  writeRingFile(ringfiletype, "", slot);
}

//===========================================================================================
void writeRingFiles() {
  if (!EnableHistory) return; //do nothing

  if ( P1Status.FirstUse ) {
    // write previous slot first because of the current act_slot in the file
    WritePrevRingRecord(RINGHOURS);
    yield();
    WritePrevRingRecord(RINGDAYS);
    yield();
    WritePrevRingRecord(RINGMONTHS);
    P1SetDevFirstUse( false ); 
  }
  //normal 
  writeRingFile(RINGHOURS, "");
  yield();
  writeRingFile(RINGDAYS, "");
  yield();
  writeRingFile(RINGMONTHS, "");
  
} // writeRingFiles()
 

/***************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the
* following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
* 
***************************************************************************/
