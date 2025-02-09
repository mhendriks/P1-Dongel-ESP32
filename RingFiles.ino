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
  uint32_t  nr = 0;
  time_t    t1 = epoch(Timestamp, strlen(Timestamp), false);
  if (ringfiletype == RINGMONTHS ) nr = ( (year(t1) -1) * 12) + month(t1);    // eg: year(2023) * 12 = 24276 + month(9) = 202309
  else nr = t1 / RingFiles[ringfiletype].seconds;
  uint8_t slot = nr % RingFiles[ringfiletype].slots;
  DebugTf("slot: Slot is [%d] nr is [%d]\r\n", slot, nr);

  return slot;
}

//===========================================================================================

//void writeRingFile(E_ringfiletype ringfiletype,const char *JsonRec) {
////  uint8_t slot = CalcSlot(ringfiletype, false);
//  writeRingFile(ringfiletype, JsonRec, false);
//}

//===========================================================================================

void writeRingFile(E_ringfiletype ringfiletype,const char *JsonRec, bool bPrev) {
#ifdef NO_STORAGE
  return;
#else  
  if ( !EnableHistory || !FSmounted ) return; //do nothing
  time_t start = millis();
  char key[9] = "";
  byte slot = 0;
  uint8_t actSlot = CalcSlot(ringfiletype, bPrev);

  StaticJsonDocument<145> rec;

  char buffer[DATA_RECLEN];
  if (strlen(JsonRec) > 1) {
    DebugTln(JsonRec);
    DeserializationError error = deserializeJson(rec, JsonRec);
    if (error) {
      DebugT(F("convert:Failed to deserialize RECORD: "));Debugln(error.c_str());
      httpServer.send(500, "application/json", httpServer.arg(0));
      #ifdef XTRA_LOG  
      LogFile("RNG: 118 convert:Failed to deserialize RECORD:",true); //log only once
      #endif 
      return;
    } 
  }

  //json openen
  DebugT(F("read(): Ring file ")); Debugln(RingFiles[ringfiletype].filename);
  
  File RingFile = LittleFS.open(RingFiles[ringfiletype].filename, "r+"); // open for reading  
  if (!RingFile || (RingFile.size() != RingFiles[ringfiletype].f_len)) {
    DebugT(F("open ring file FAILED!!! --> Bailout\r\n"));
    Debugln(RingFiles[ringfiletype].filename);
    #ifdef XTRA_LOG  
    LogFile("RNG: 129 open ring file FAILED!!! --> Bailout",true); //log only once
    #endif  
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
    snprintf(buffer, sizeof(buffer), (char*)DATA_FORMAT, key , (float)DSMRdata.energy_delivered_tariff1, (float)DSMRdata.energy_delivered_tariff2, (float)DSMRdata.energy_returned_tariff1, (float)DSMRdata.energy_returned_tariff2, (float)gasDelivered, mbusWater?(float)waterDelivered : (float)P1Status.wtr_m3+(float)P1Status.wtr_l/1000.0);
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
  Debugf( "Time consumed writing RNGFile %s : %d\n", RingFiles[ringfiletype].filename, millis()-start);
  
#endif //NO_STORAGE

} // writeRingFile()

/*

ringfiles previous record
example: 23-04-2023 10:03 ==> in ring file => 23042310

previous records
Hour: 23042309 -> epoch - 3.600 -> to string
Day: 230422xx -> epoch - 86.400 -> to string
Month: 2303xxxx -> month - 1 (if 0 then 12 and year - 1) or epoch - 31 days
 
*/

//===========================================================================================
void WritePrevRingRecord(E_ringfiletype ringfiletype){
//  uint8_t slot = CalcSlot(ringfiletype, true);
  char tempTS[20];
  int yr,mth;
  
  strncpy( tempTS, actTimestamp, sizeof(tempTS) ); // backup act ts
//  Debugf("actTS: %s\r\n",actTimestamp);
  
  switch ( ringfiletype ) {
    //use newT because actT is not set at the first telegram
    case RINGHOURS :  epochToTimestamp( newT-3600, actTimestamp, sizeof(actTimestamp) );
                      break;
    case RINGDAYS :   epochToTimestamp( newT-86400, actTimestamp, sizeof(actTimestamp) );
                      break;
    case RINGMONTHS : { // because not all months are 31 days ;-)
                        yr = YearFromTimestamp(DSMRdata.timestamp.c_str());
                        mth = MonthFromTimestamp(DSMRdata.timestamp.c_str());
                        if ( mth == 1) {
                          mth = 12;
                          yr--;
                        } else mth--;
                        snprintf(actTimestamp, sizeof(actTimestamp),"%2.2d%2.2d2823",yr,mth);
//                        Debugln(actTimestamp);
                      break;
                      }
  }
  
//  Debugf("newTS: %s \r\n",actTimestamp);
  writeRingFile(ringfiletype, "", true);
  strncpy( actTimestamp, tempTS, sizeof(tempTS) ); // restore act ts
}

//===========================================================================================
void writeRingFiles() {
  if (!EnableHistory || ((telegramCount - telegramErrors ) < 2) ) {
#ifdef XTRA_LOG  
    LogFile("RNG: exit before writing",true); //log only once
#endif    
    return; //do nothing or telegram < 2 skip 
  }

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
  writeRingFile(RINGHOURS, "", false);
  yield();
  writeRingFile(RINGDAYS, "", false);
  yield();
  writeRingFile(RINGMONTHS, "", false);
#ifdef XTRA_LOG  
  LogFile("RNG: all files writen",true); //log only once
#endif  
//  bWriteFiles = false;
  
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
