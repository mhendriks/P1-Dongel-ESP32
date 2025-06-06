/* 
***************************************************************************  
**  Program  : JsonCalls, part of DSMRloggerAPI
**  Version  : v4.2.1
**
**  Copyright (c) 2025 Smartstuff
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

int recordCount = 0;
int actSlot = 0;

RingRecord repaired[30];
int repairedCount = 0;

bool convertRingfileWithSlotExpansion(E_ringfiletype type, uint8_t newSlotCount) {
  const S_ringfile& file = RingFiles[type];
  File f = LittleFS.open(file.filename, "r");
  if (!f) {
    Debugln("ERROR: input file open failed.");
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Debug("JSON parse error: ");
    Debugln(err.c_str());
    return false;
  }

  JsonArray oldData = doc["data"];

  struct Record {
    char date[9];
    float values[7];
  };

  Record newRecords[newSlotCount];
  
  //fill all data with defaults
  for (int i = 0; i < newSlotCount; i++) {
    strcpy(newRecords[i].date, "20000000");
    for (int j = 0; j < 7; j++) newRecords[i].values[j] = 0.000;
  }

  // Oude records inladen op basis van nieuwe slotpositie
  for (JsonObject obj : oldData) {
    const char* date = obj["date"] | "";
    
    if (strcmp(date, "20000000") == 0) continue; // skip

    time_t t1 = epoch(date, strlen(date), false);
    uint32_t nr = (type == RINGMONTHS)
      ? ((year(t1) - 1) * 12) + month(t1)
      : t1 / RingFiles[type].seconds;
    uint8_t slot = nr % newSlotCount;

    strncpy(newRecords[slot].date, date, 9);
    JsonArray values = obj["values"];
    for (int i = 0; i < 6; i++) {
      newRecords[slot].values[i] = values[i] | 0.0;
    }
    newRecords[slot].values[6] = 0.000;
  }

  // actSlot bepalen: slot van laatste geldige datum
  const char* lastDate = oldData[oldData.size() - 1]["date"];
  time_t t_last = epoch(lastDate, strlen(lastDate), false);
  uint32_t nr_last = (type == RINGMONTHS)
      ? ((year(t_last) - 1) * 12) + month(t_last)
      : t_last / RingFiles[type].seconds;
  uint8_t actSlot = nr_last % newSlotCount;

  // Bestand wegschrijven
  File fout = LittleFS.open("/RNGdays32_7.json", "w");
  if (!fout) {
    Debugln("ERROR: output file open failed.");
    return false;
  }

  fout.printf("{\"actSlot\":%d,\"data\":[\n", actSlot);
  for (int i = 0; i < newSlotCount; i++) {
    fout.printf("{\"date\":\"%s\",\"values\":[", newRecords[i].date);
    for (int j = 0; j < 7; j++) {
      fout.printf("%10.3f", newRecords[i].values[j]);
      if (j < 6) fout.print(",");
    }
    fout.print("]}");
    fout.print(i < newSlotCount - 1 ? ",\n" : "\n");
  }
  fout.print("]}");
  fout.close();

  Debugln("Conversie succesvol");
  return true;
}


bool patchJsonFile_Add7thValue(E_ringfiletype type) {
  const S_ringfile& file = RingFiles[type];
  File fin = LittleFS.open(file.filename, "r");
  if (!fin) {
    Debugln("ERROR: input file");
    return false;
  }

  String outname = String(file.filename);
  outname.replace(".json", "_7.json");
  File fout = LittleFS.open(outname, "w");
  if (!fout) {
    Debugln("ERROR: output file.");
    fin.close();
    return false;
  }

  int linenr = 0;
  while (fin.available()) {
    String line = fin.readStringUntil('\n');
    // Skip eerste en laatste regel (actSlot en afsluitende ]})
    if ( linenr > 0 && linenr <= file.slots ) line.replace("]}", ",     0.000]}");
    fout.println(line);
    linenr++;
  }

  fin.close();
  fout.close();
  Debugln("Conversie succesvol");
  return true;
}


void printRecordArray(const RingRecord* records, int slots, const char* label = nullptr) {
  if (label) {
    Debug("=== ");
    Debug(label);
    Debugln(" ===");
  }

  for (int i = 0; i < slots; i++) {
    Debug("Slot ");
    Debug(i);
    Debug(" | Date: ");
    Debug(records[i].date);
    Debug(" | Values: ");

    for (int j = 0; j < 6; j++) {
      Debug(records[i].values[j], 3);
      if (j < 5) Debug(", ");
    }

    Debugln();
  }
}


bool loadRingfile(E_ringfiletype type) {
  File f = LittleFS.open(RingFiles[type].filename, "r");
  if (!f) {
    Debugln("Fout bij openen bestand");
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) {
    Debug("Fout bij JSON parsing: ");
    Debugln(err.c_str());
    return false;
  }

  actSlot = doc["actSlot"] | 0;
  JsonArray data = doc["data"];
  int i = 0;
  for (JsonObject obj : data) {
    if (i >= RingFiles[type].slots) break;
    strncpy(RNGDayRec[i].date, obj["date"] | "", 9);

    JsonArray values = obj["values"];
    for (int j = 0; j < 6; j++) {
      RNGDayRec[i].values[j] = values[j] | 0.0;
    }
    i++;
  }

  return true;
}

bool saveRingfile(E_ringfiletype type) {
  const S_ringfile& file = RingFiles[type];
  File f = LittleFS.open("/testDays.json", "w");
  if (!f) {
    Debug("Kan bestand niet openen voor schrijven: ");
    Debugln(file.filename);
    return false;
  }
  f.print("{\"actSlot\":");
  f.print(actSlot);
  f.print(",\"data\":[\n");

  for (int i = 0; i < file.slots; i++) {
    f.print("{\"date\":\"");
    f.print(RNGDayRec[i].date);
    f.print("\",\"values\":[");

    // Handmatig met vaste spacing en precisie
    for (int j = 0; j < 6; j++) {
      f.printf("%10.3f", RNGDayRec[i].values[j]);
      if (j < 5) f.print(", ");
    }

    f.print("]}");
    if (i < file.slots - 1) f.print(",\n");
  }

  f.print("\n]}");
  f.close();

  Debug("Bestand opgeslagen in correct formaat: "); Debugln(file.filename);
  return true;
}

time_t date2epoch(const char* date){
  struct tm tNew = {};
  if (strlen(date) != 8) return 0;

  sscanf(date, "%2d%2d%2d%2d", &tNew.tm_year, &tNew.tm_mon, &tNew.tm_mday, &tNew.tm_hour);
  tNew.tm_hour = 23; //hard end of day 
  tNew.tm_year += 100;
  tNew.tm_mon -= 1;

  time_t ts = mktime(&tNew);
  return ts > 0 ? ts : 0;
}

bool isPreviousDay(const char* newerDate, const char* olderDate) {

  time_t dateNew = date2epoch(newerDate);
  time_t dateOld = date2epoch(olderDate);
  
  // Debug("dateNew: ");Debugln(dateNew);
  // Debug("dateOld: ");Debugln(dateOld);
  
  // Debug("(dateNew - dateOld): ");Debugln((dateNew - dateOld));
  
  return (dateNew - dateOld) == 86400;
}

void printRec(struct RingRecord* recs, int count) {
  for (int i = 0; i < count; ++i) {
    Debugf("Record %2d - date: %s | values: ", i, recs[i].date);
    for (int j = 0; j < 6; ++j)
      Debugf("%.3f ", recs[i].values[j]);
    Debugln();
  }
}

void epochToDate(time_t ts, char* out) {
  struct tm* t = localtime(&ts);
  snprintf(out, 9, "%02d%02d%02d%02d", t->tm_year % 100, t->tm_mon + 1, t->tm_mday, t->tm_hour);
}

#define DAY_SEC 86400
#define NUM_VALUES 6

void repairRingRecords( RingRecord* records, int len) {
  for (int i = 0; i < len - 1; i++) {
    time_t tCurrent = date2epoch(records[i].date);
    time_t tNext = date2epoch(records[i + 1].date);

    if (tCurrent == 0 || tNext == 0) {
      Debugf("❌ Ongeldige datum bij record %d of %d\n", i, i + 1);
      continue;
    }

    // Verwacht dat tNext precies 1 dag eerder is
    if ((tCurrent - tNext) != DAY_SEC) {
      Debugf("📉 Afwijking bij record %d ➝ %d\n", i, i + 1);

      // Zoek eerstvolgende correcte record
      int j = i + 2;
      while (j < len) {
        time_t tCandidate = date2epoch(records[j].date);
        if (tCandidate != 0 && (tCurrent - tCandidate) % DAY_SEC == 0) break;
        j++;
      }

      if (j >= len) {
        // Geen geldig record meer gevonden: vul rest met kopieën van i
        Debugln("⚠️ Geen volgend correct record — kopieer vanaf huidig record naar einde");

        for (int k = i + 1; k < len; k++) {
          time_t tNew = tCurrent - ((k - i) * DAY_SEC);
          epochToDate(tNew, records[k].date);
          for (int v = 0; v < NUM_VALUES; v++) {
            records[k].values[v] = records[i].values[v];
          }
          Debugf("📋 Record %d gekopieerd van %d (datum: %s)\n", k, i, records[k].date);
        }
        break;  // Niks meer te doen
      }

      int gap = j - i;
      time_t tStart = tCurrent;
      time_t tEnd = date2epoch(records[j].date);

      for (int k = 1; k < gap; k++) {
        time_t tNew = tStart - (DAY_SEC * k);
        epochToDate(tNew, records[i + k].date);

        float f = (float)k / gap;
        for (int v = 0; v < NUM_VALUES; v++) {
          float v1 = records[i].values[v];
          float v2 = records[j].values[v];
          records[i + k].values[v] = v1 + (v2 - v1) * f;
        }

        Debugf("🔧 Record %d hersteld (date: %s)\n", i + k, records[i + k].date);
      }

      i = j - 1; // spring verder
    }
  }

  Debugln("✅ Herstel voltooid.");
}


// void CheckRingFile(E_ringfiletype ringfiletype) {
//   File f = LittleFS.open(RingFiles[ringfiletype].filename, "r");
//   if (!f) {
//     Serial.println("Kan bestand niet openen.");
//     return;
//   }

//   JsonDocument doc;
//   DeserializationError err = deserializeJson(doc, f);
//   f.close();
//   if (err) {
//     Debug("Parse fout: ");Debugln(err.c_str());
//     return;
//   }

//   actSlot = doc["actSlot"].as<int>();
//   int total = doc["data"].size();
  
//   Debugln("json to array");
//   for (int i = 0; i < total; i++) {
//     int idx = (total + actSlot - i) % total;
//     strncpy(ringRecords[i].date, doc["data"][idx]["date"], 9);
//     for (int j = 0; j < 6; j++) {
//       ringRecords[i].values[j] = doc["data"][idx]["values"][j].as<float>();
//     }
//     Debugf("i: %02d - idx: %02d - date: %s\n",i,idx,ringRecords[i].date);
//   }
//   Debugln("VOOR herstel");
//   printRec(ringRecords,total);

//   repairRingRecords(ringRecords, total);
//   Debugln("NA herstel");
//   printRec(ringRecords,total);
  
//   // Check op 1 dag verschil tussen elke 2 records
//   // for (int i = 1; i < total; i++) {
//   //   if (!isPreviousDay(ringRecords[i - 1].date, ringRecords[i].date)) {
//   //     Debug("Datumverschil ongeldig tussen ");
//   //     Debug(ringRecords[i - 1].date);
//   //     Debug(" en ");
//   //     Debugln(ringRecords[i].date);
//   //   } else Debugln("No gap between records");
//   // }
// }


void CheckRingExists(){
  for (byte i = 0; i< 3; i++){
    if ( !LittleFS.exists(RingFiles[i].filename) ) {
      createRingFile( (E_ringfiletype)i );
      P1SetDevFirstUse( true ); //when one or more files are missing first use mode is triggered
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

uint8_t CalcSlot(E_ringfiletype ringfiletype, const char* Timestamp) 
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

  JsonDocument rec;

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
    
  snprintf(buffer, sizeof(buffer), (char*)DATA_FORMAT, key , (float)DSMRdata.energy_delivered_tariff1, (float)DSMRdata.energy_delivered_tariff2, (float)DSMRdata.energy_returned_tariff1, (float)DSMRdata.energy_returned_tariff2, (float)gasDelivered, mbusWater?(float)waterDelivered : (float)P1Status.wtr_m3+(float)P1Status.wtr_l/1000.0);
  
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
