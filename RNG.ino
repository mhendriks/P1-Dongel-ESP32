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

static uint8_t ringValueCount(E_ringfiletype type) {
  return RingFiles[type].valueCount;
}

static uint16_t ringRecordLen(E_ringfiletype type) {
  return RingFiles[type].recordLen;
}

static void clearRingRecord(RingRecord& record) {
  strlcpy(record.date, "20000000", sizeof(record.date));
  for (int i = 0; i < RNG_DAYS_VALUE_COUNT; i++) {
    record.values[i] = 0.0f;
  }
}

static uint32_t ringRecordNumber(E_ringfiletype type, time_t t1) {
  if (type == RINGMONTHS) {
    return ((year(t1) - 1) * 12) + month(t1);
  }
  return t1 / RingFiles[type].seconds;
}

static float currentSolarDailyKwh() {
  return totalSolarDailyWh() / 1000.0f;
}

static int formatRingRecord(E_ringfiletype type, char* buffer, size_t bufferSize, const char* key, const float* values) {
  if (ringValueCount(type) == RNG_DAYS_VALUE_COUNT) {
    return snprintf(buffer, bufferSize,
                    "{\"date\":\"%-8.8s\",\"values\":[%10.3f,%10.3f,%10.3f,%10.3f,%10.3f,%10.3f,%10.3f]}",
                    key, values[0], values[1], values[2], values[3], values[4], values[5], values[6]);
  }

  return snprintf(buffer, bufferSize,
                  "{\"date\":\"%-8.8s\",\"values\":[%10.3f,%10.3f,%10.3f,%10.3f,%10.3f,%10.3f]}",
                  key, values[0], values[1], values[2], values[3], values[4], values[5]);
}

static bool writeRingfileRecords(const char* filename, E_ringfiletype type, uint8_t actSlotValue, const RingRecord* records, uint16_t recordCount) {
  File fout = LittleFS.open(filename, "w");
  if (!fout) {
    DebugT(F("writeRingfileRecords: open failed for "));
    Debugln(filename);
    return false;
  }

  fout.printf("{\"actSlot\":%2d,\"data\":[\n", actSlotValue);
  for (uint16_t i = 0; i < recordCount; i++) {
    char buffer[DATA_RECLEN_7];
    formatRingRecord(type, buffer, sizeof(buffer), records[i].date, records[i].values);
    fout.print(buffer);
    fout.print(i < (recordCount - 1) ? ",\n" : "\n");
  }
  fout.print("]}");
  fout.close();

  return true;
}

static bool migrateLegacyRNGdaysFile(const char* sourceFile) {
  File fin = LittleFS.open(sourceFile, "r");
  if (!fin) {
    DebugT(F("RNGdays migration: source open failed for "));
    Debugln(sourceFile);
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, fin);
  fin.close();
  if (err) {
    DebugT(F("RNGdays migration: parse error: "));
    Debugln(err.c_str());
    return false;
  }

  RingRecord newRecords[RNG_DAYS_SLOT_COUNT];
  for (uint8_t i = 0; i < RNG_DAYS_SLOT_COUNT; i++) clearRingRecord(newRecords[i]);

  JsonArray oldData = doc["data"];
  bool haveNewest = false;
  uint32_t newestNr = 0;

  for (JsonObject obj : oldData) {
    const char* date = obj["date"] | "";
    if (strcmp(date, "20000000") == 0 || strlen(date) != 8) continue;

    time_t t1 = epoch(date, strlen(date), false);
    if (t1 <= 0) continue;

    uint32_t nr = ringRecordNumber(RINGDAYS, t1);
    uint8_t slot = nr % RNG_DAYS_SLOT_COUNT;

    strlcpy(newRecords[slot].date, date, sizeof(newRecords[slot].date));
    JsonArray values = obj["values"];
    for (uint8_t i = 0; i < RING_DEFAULT_VALUE_COUNT; i++) {
      newRecords[slot].values[i] = values[i] | 0.0f;
    }
    newRecords[slot].values[6] = 0.0f;

    if (!haveNewest || nr > newestNr) {
      newestNr = nr;
      haveNewest = true;
    }
  }

  const uint8_t migratedActSlot = haveNewest ? (newestNr % RNG_DAYS_SLOT_COUNT) : 0;
  if (!writeRingfileRecords(RingFiles[RINGDAYS].filename, RINGDAYS, migratedActSlot, newRecords, RNG_DAYS_SLOT_COUNT)) {
    DebugTln(F("RNGdays migration: writing new file failed"));
    return false;
  }

  DebugTln(F("RNGdays migration completed"));
  return true;
}

static bool ensureRNGdaysLayout() {
  if (!FSmounted || !LittleFS.exists(RingFiles[RINGDAYS].filename)) return true;

  File f = LittleFS.open(RingFiles[RINGDAYS].filename, "r");
  if (!f) return false;
  const size_t currentSize = f.size();
  f.close();

  if (currentSize == (size_t)RingFiles[RINGDAYS].f_len) return true;

  if (currentSize != (size_t)RNG_DAYS_LEGACY_FILE_LEN) {
    DebugTf("RNGdays has unexpected size [%d], expected [%d] or [%d]\r\n",
            (int)currentSize, RingFiles[RINGDAYS].f_len, RNG_DAYS_LEGACY_FILE_LEN);
    return false;
  }

  if (LittleFS.exists(RNG_DAYS_BACKUP_FILE)) {
    DebugTln(F("RNGdays migration: legacy backup already present, reusing current file as source"));
    return migrateLegacyRNGdaysFile(RingFiles[RINGDAYS].filename);
  }

  if (!LittleFS.rename(RingFiles[RINGDAYS].filename, RNG_DAYS_BACKUP_FILE)) {
    DebugTln(F("RNGdays migration: rename to backup failed"));
    return false;
  }

  if (migrateLegacyRNGdaysFile(RNG_DAYS_BACKUP_FILE)) return true;

  LittleFS.remove(RingFiles[RINGDAYS].filename);
  LittleFS.rename(RNG_DAYS_BACKUP_FILE, RingFiles[RINGDAYS].filename);
  DebugTln(F("RNGdays migration rolled back"));
  return false;
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

    for (int j = 0; j < RNG_DAYS_VALUE_COUNT; j++) {
      Debug(records[i].values[j], 3);
      if (j < (RNG_DAYS_VALUE_COUNT - 1)) Debug(", ");
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
  for (uint16_t slot = 0; slot < RingFiles[type].slots; slot++) clearRingRecord(RNGDayRec[slot]);
  int i = 0;
  for (JsonObject obj : data) {
    if (i >= RingFiles[type].slots) break;
    strlcpy(RNGDayRec[i].date, obj["date"] | "", sizeof(RNGDayRec[i].date));

    JsonArray values = obj["values"];
    for (int j = 0; j < RingFiles[type].valueCount; j++) {
      RNGDayRec[i].values[j] = values[j] | 0.0;
    }
    i++;
  }

  return true;
}

bool loadRNGDaysHistory() {
  if (!loadRingfile(RINGDAYS)) return false;

  const uint16_t daySlots = RingFiles[RINGDAYS].slots;
  time_t now = time(NULL);
  if (now <= 1000000000) {
    DebugTln(F("No valid system time available for RNGdays restore"));
    return false;
  }

  time_t yesterday = now - 86400;
  struct tm* tmYesterday = localtime(&yesterday);
  if (!tmYesterday) return false;

  char targetDay[7];
  snprintf(targetDay, sizeof(targetDay), "%02d%02d%02d", (tmYesterday->tm_year % 100), tmYesterday->tm_mon + 1, tmYesterday->tm_mday);

  int currentSlot = (now / RingFiles[RINGDAYS].seconds) % daySlots;
  int firstSlot = (currentSlot - 1 + daySlots) % daySlots;
  int selectedSlot = -1;

  auto matchesYesterday = [&](int slot) -> bool {
    if (slot < 0 || slot >= daySlots) return false;
    if (strncmp(RNGDayRec[slot].date, "20000000", 8) == 0) return false;
    return strncmp(RNGDayRec[slot].date, targetDay, 6) == 0;
  };

  if (matchesYesterday(firstSlot)) selectedSlot = firstSlot;
  if (selectedSlot < 0) {
    for (int offset = 0; offset < daySlots; offset++) {
      int slot = (firstSlot - offset + daySlots) % daySlots;
      if (matchesYesterday(slot)) {
        selectedSlot = slot;
        break;
      }
    }
  }
  if (selectedSlot < 0) {
    dataYesterday.lastUpdDay = 0;
    DebugTln(F("No matching day slot found in RNGdays; start from current meter values"));
    return false;
  }

  time_t ts = date2epoch(RNGDayRec[selectedSlot].date);
  if (ts == 0) return false;

  dataYesterday.t1    = (uint32_t)(RNGDayRec[selectedSlot].values[0] * 1000.0f + 0.5f);
  dataYesterday.t2    = (uint32_t)(RNGDayRec[selectedSlot].values[1] * 1000.0f + 0.5f);
  dataYesterday.t1r   = (uint32_t)(RNGDayRec[selectedSlot].values[2] * 1000.0f + 0.5f);
  dataYesterday.t2r   = (uint32_t)(RNGDayRec[selectedSlot].values[3] * 1000.0f + 0.5f);
  dataYesterday.gas   = (uint32_t)(RNGDayRec[selectedSlot].values[4] * 1000.0f + 0.5f);
  dataYesterday.water = (uint32_t)(RNGDayRec[selectedSlot].values[5] * 1000.0f + 0.5f);
  dataYesterday.lastUpdDay = ts / (3600 * 24);

  DebugT(F("RNGdays restored day: "));
  Debugln(RNGDayRec[selectedSlot].date);
  DebugT(F("Slot used: "));
  Debugln(selectedSlot);
  DebugT(F("Matched yymmdd: "));
  Debugln(targetDay);
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
    for (int j = 0; j < file.valueCount; j++) {
      f.printf("%10.3f", RNGDayRec[i].values[j]);
      if (j < (file.valueCount - 1)) f.print(",");
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
    for (int j = 0; j < RNG_DAYS_VALUE_COUNT; ++j)
      Debugf("%.3f ", recs[i].values[j]);
    Debugln();
  }
}

void epochToDate(time_t ts, char* out) {
  struct tm* t = localtime(&ts);
  snprintf(out, 9, "%02d%02d%02d%02d", t->tm_year % 100, t->tm_mon + 1, t->tm_mday, t->tm_hour);
}

#define DAY_SEC 86400
#define NUM_VALUES RNG_DAYS_VALUE_COUNT

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
  ensureRNGdaysLayout();

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
    RingRecord emptyRecord;
    clearRingRecord(emptyRecord);
    char buffer[DATA_RECLEN_7];
    formatRingRecord(ringfiletype, buffer, sizeof(buffer), emptyRecord.date, emptyRecord.values);
    RingFile.print(buffer);
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

enum RngWorkerMode : uint8_t {
  RNG_WORKER_CURRENT_HOURS,
  RNG_WORKER_CURRENT_DAYS,
  RNG_WORKER_CURRENT_MONTHS,
  RNG_WORKER_PREV_HOURS,
  RNG_WORKER_PREV_DAYS,
  RNG_WORKER_PREV_MONTHS,
  RNG_WORKER_DST_GAP_HOURS
};

static volatile uint8_t rngWriteJobsPending = 0;

static bool rngWritePending() {
  return rngWriteJobsPending > 0;
}

bool RngWritePending() {
  return rngWritePending();
}

uint8_t CalcSlotAt(E_ringfiletype ringfiletype, time_t slotTime, bool prev_slot)
{
  uint32_t nr = 0;
  if (ringfiletype == RINGMONTHS) nr = ((year(slotTime) - 1) * 12) + month(slotTime);
  else nr = slotTime / RingFiles[ringfiletype].seconds;
  if (prev_slot) nr--;
  uint8_t slot = nr % RingFiles[ringfiletype].slots;
#ifdef XTRA_LOG
  DebugTf("slot: Slot is [%d] nr is [%d]\r\n", slot, nr);
#endif

  return slot;
}

uint8_t CalcSlot(E_ringfiletype ringfiletype, bool prev_slot) 
{
  return CalcSlotAt(ringfiletype, actT, prev_slot);
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
#ifdef XTRA_LOG
  DebugTf("slot: Slot is [%d] nr is [%d]\r\n", slot, nr);
#endif

  return slot;
}

bool writeRingFileAtSlot(E_ringfiletype ringfiletype, uint8_t slot, const char *JsonRec, bool bWinterSummer, bool bPrevRecord) {
  if (!EnableHistory || !FSmounted) return false;
  if (slot >= RingFiles[ringfiletype].slots) return false;

  char key[9] = "";
  JsonDocument rec;
  char buffer[DATA_RECLEN_7];
  float values[RNG_DAYS_VALUE_COUNT] = {0.0f};

  // Parse optioneel JsonRec (alleen als meegegeven)
  bool hasJsonRec = (JsonRec && strlen(JsonRec) > 1);
  if (hasJsonRec) {
    DebugTln(JsonRec);
    DeserializationError error = deserializeJson(rec, JsonRec);
    if (error) {
      DebugT(F("convert:Failed to deserialize RECORD: ")); Debugln(error.c_str());
      #ifdef XTRA_LOG
      LogFile("RNG: convert:Failed to deserialize RECORD", true);
      #endif
      return false;
    }
  }

  // Open ringfile
#ifdef XTRA_LOG
  DebugT(F("read(): Ring file ")); Debugln(RingFiles[ringfiletype].filename);
#endif

  File RingFile = LittleFS.open(RingFiles[ringfiletype].filename, "r+");
  if (!RingFile || (RingFile.size() != RingFiles[ringfiletype].f_len)) {
    DebugT(F("open ring file FAILED!!! --> Bailout\r\n"));
    Debugln(RingFiles[ringfiletype].filename);
    #ifdef XTRA_LOG
    LogFile("RNG: open ring file FAILED!!! --> Bailout", true);
    #endif
    RingFile.close();
    return false;
  }

  // Update header actSlot
  // (actSlot in header = toon in UI waar 'nu' is; laat dit bestaan zoals jij het doet)
  uint8_t actSlot = CalcSlot(ringfiletype, /*bPrev*/ false);
  snprintf(buffer, sizeof(buffer), "{\"actSlot\":%2d,\"data\":[\n", actSlot);
  RingFile.print(buffer);

  // Bouw record payload
  if (hasJsonRec) {
    // Record uit JsonRec
    strncpy(key, rec["recid"] | "", 8);
    key[8] = '\0';

    values[0] = (float)(rec["edt1"] | 0.0f);
    values[1] = (float)(rec["edt2"] | 0.0f);
    values[2] = (float)(rec["ert1"] | 0.0f);
    values[3] = (float)(rec["ert2"] | 0.0f);
    values[4] = (float)(rec["gdt"]  | 0.0f);
    values[5] = (float)(rec["wtr"]  | 0.0f);
    if (ringValueCount(ringfiletype) > RING_DEFAULT_VALUE_COUNT) {
      values[6] = (float)(rec["solar"] | 0.0f);
    }

    formatRingRecord(ringfiletype, buffer, sizeof(buffer), key, values);
  } else {
    // Actuele data (zoals je al deed)
    strncpy(key, actTimestamp, 8);
    key[8] = '\0';
    if (bWinterSummer) { key[6] = '0'; key[7] = '2'; }
    values[0] = (float)DSMRdata.energy_delivered_tariff1;
    values[1] = (float)DSMRdata.energy_delivered_tariff2;
    values[2] = (float)DSMRdata.energy_returned_tariff1;
    values[3] = (float)DSMRdata.energy_returned_tariff2;
    values[4] = (float)gasDelivered;
    values[5] = mbusWater ? (float)waterDelivered
                          : (float)P1Status.wtr_m3 + (float)P1Status.wtr_l / 1000.0f;
    if (ringValueCount(ringfiletype) > RING_DEFAULT_VALUE_COUNT) {
      values[6] = bPrevRecord ? 0.0f : currentSolarDailyKwh();
    }

    formatRingRecord(ringfiletype, buffer, sizeof(buffer), key, values);
  }

  // Seek naar slot-offset en overschrijf record
  uint16_t offset = (slot * ringRecordLen(ringfiletype)) + JSON_HEADER_LEN;
  RingFile.seek(offset, SeekSet);

  int32_t bytesWritten = RingFile.print(buffer);
  if (bytesWritten != (ringRecordLen(ringfiletype) - 2)) {
    DebugTf("ERROR! slot[%02d]: written [%d] bytes but should have been [%d]\r\n",
            slot, bytesWritten, ringRecordLen(ringfiletype));
  }

  if (slot < (RingFiles[ringfiletype].slots - 1)) RingFile.print(",\n");
  else RingFile.print("\n");

  RingFile.close();
  return bytesWritten == (ringRecordLen(ringfiletype) - 2);
}


bool writeRingFile(E_ringfiletype ringfiletype, const char *JsonRec, bool bPrev) {
  if (JsonRec && strlen(JsonRec) > 1) {
    JsonDocument rec;
    if (deserializeJson(rec, JsonRec)) return false;

    char key[9] = "";
    strncpy(key, rec["recid"] | "", 8);
    key[8] = '\0';

    uint8_t slot = CalcSlot(ringfiletype, key);
    return writeRingFileAtSlot(ringfiletype, slot, JsonRec, false, false);
  }

  uint8_t slot = CalcSlot(ringfiletype, bPrev);
  return writeRingFileAtSlot(ringfiletype, slot, /*JsonRec*/ nullptr, false, bPrev);
}

ApiResponse historyMonthsApiResponse(const String& body) {
  if (writeRingFile(RINGMONTHS, body.c_str(), false)) {
    return {200, "application/json", body};
  }
  return {500, "application/json", body};
}

static void writeRingSnapshotAtSlot(const WorkerRngPayload& snapshot,
                                    E_ringfiletype ringfiletype,
                                    uint8_t slot,
                                    const char* keyIn,
                                    bool bWinterSummer,
                                    bool bPrevRecord) {
  if (!EnableHistory || !FSmounted) return;
  if (slot >= RingFiles[ringfiletype].slots) return;

  char key[9] = "";
  char buffer[DATA_RECLEN_7];
  float values[RNG_DAYS_VALUE_COUNT] = {0.0f};

  strlcpy(key, keyIn ? keyIn : "", sizeof(key));
  if (bWinterSummer) { key[6] = '0'; key[7] = '2'; }

  for (uint8_t i = 0; i < ringValueCount(ringfiletype); i++) values[i] = snapshot.values[i];
  if (ringValueCount(ringfiletype) > RING_DEFAULT_VALUE_COUNT && bPrevRecord) values[6] = 0.0f;

#ifdef XTRA_LOG
  DebugT(F("read(): Ring file ")); Debugln(RingFiles[ringfiletype].filename);
#endif

  File RingFile = LittleFS.open(RingFiles[ringfiletype].filename, "r+");
  if (!RingFile || (RingFile.size() != RingFiles[ringfiletype].f_len)) {
    DebugT(F("open ring file FAILED!!! --> Bailout\r\n"));
    Debugln(RingFiles[ringfiletype].filename);
#ifdef XTRA_LOG
    LogFile("RNG: open ring file FAILED!!! --> Bailout", true);
#endif
    RingFile.close();
    return;
  }

  uint8_t actSlot = CalcSlotAt(ringfiletype, snapshot.actT, false);
  snprintf(buffer, sizeof(buffer), "{\"actSlot\":%2d,\"data\":[\n", actSlot);
  RingFile.print(buffer);

  formatRingRecord(ringfiletype, buffer, sizeof(buffer), key, values);

  uint16_t offset = (slot * ringRecordLen(ringfiletype)) + JSON_HEADER_LEN;
  RingFile.seek(offset, SeekSet);

  int32_t bytesWritten = RingFile.print(buffer);
  if (bytesWritten != (ringRecordLen(ringfiletype) - 2)) {
    DebugTf("ERROR! slot[%02d]: written [%d] bytes but should have been [%d]\r\n",
            slot, bytesWritten, ringRecordLen(ringfiletype));
  }

  if (slot < (RingFiles[ringfiletype].slots - 1)) RingFile.print(",\n");
  else RingFile.print("\n");

  RingFile.close();
}


static void rngKeyFromTimestamp(char* key, size_t keySize, const char* timestamp) {
  strlcpy(key, timestamp ? timestamp : "", keySize);
  if (keySize > 8) key[8] = '\0';
}

static void rngPrevMonthKey(char* key, size_t keySize, const char* dsmrTimestamp) {
  int yr = YearFromTimestamp(String(dsmrTimestamp ? dsmrTimestamp : "").c_str());
  int mth = MonthFromTimestamp(String(dsmrTimestamp ? dsmrTimestamp : "").c_str());
  if (mth == 1) {
    mth = 12;
    yr--;
  } else {
    mth--;
  }
  snprintf(key, keySize, "%2.2d%2.2d2823", yr, mth);
}

static void rngWritePrevSnapshotRecord(const WorkerRngPayload& snapshot, E_ringfiletype ringfiletype) {
  char key[9] = "";
  switch (ringfiletype) {
    case RINGHOURS:
      {
        char ts[20] = "";
        epochToTimestamp(snapshot.newT - 3600, ts, sizeof(ts));
        rngKeyFromTimestamp(key, sizeof(key), ts);
      }
      break;
    case RINGDAYS:
      {
        char ts[20] = "";
        epochToTimestamp(snapshot.newT - 86400, ts, sizeof(ts));
        rngKeyFromTimestamp(key, sizeof(key), ts);
      }
      break;
    case RINGMONTHS:
      rngPrevMonthKey(key, sizeof(key), snapshot.dsmrTimestamp);
      break;
  }

  uint8_t slot = CalcSlotAt(ringfiletype, snapshot.actT, true);
  writeRingSnapshotAtSlot(snapshot, ringfiletype, slot, key, false, true);
}

static void rngWriteCurrentSnapshotRecord(const WorkerRngPayload& snapshot, E_ringfiletype ringfiletype) {
  char key[9] = "";
  rngKeyFromTimestamp(key, sizeof(key), snapshot.actTimestamp);
  uint8_t slot = CalcSlotAt(ringfiletype, snapshot.actT, false);
  writeRingSnapshotAtSlot(snapshot, ringfiletype, slot, key, false, false);
}

void RngWriteFromWorker(const WorkerRngPayload& snapshot) {
  if (!EnableHistory) {
    if (rngWriteJobsPending > 0) rngWriteJobsPending--;
    return;
  }

  switch ((RngWorkerMode)snapshot.mode) {
    case RNG_WORKER_CURRENT_HOURS:
      rngWriteCurrentSnapshotRecord(snapshot, RINGHOURS);
      break;
    case RNG_WORKER_CURRENT_DAYS:
      rngWriteCurrentSnapshotRecord(snapshot, RINGDAYS);
      break;
    case RNG_WORKER_CURRENT_MONTHS:
      rngWriteCurrentSnapshotRecord(snapshot, RINGMONTHS);
      break;
    case RNG_WORKER_PREV_HOURS:
      rngWritePrevSnapshotRecord(snapshot, RINGHOURS);
      break;
    case RNG_WORKER_PREV_DAYS:
      rngWritePrevSnapshotRecord(snapshot, RINGDAYS);
      break;
    case RNG_WORKER_PREV_MONTHS:
      rngWritePrevSnapshotRecord(snapshot, RINGMONTHS);
      break;
    case RNG_WORKER_DST_GAP_HOURS:
      {
        char key[9] = "";
        rngKeyFromTimestamp(key, sizeof(key), snapshot.actTimestamp);
        uint8_t slot = CalcSlotAt(RINGHOURS, snapshot.actT, false);
        slot = (slot + 1) % RingFiles[RINGHOURS].slots;
        writeRingSnapshotAtSlot(snapshot, RINGHOURS, slot, key, true, false);
        DebugTln(F("DST gap fix"));
      }
      break;
  }

  if (snapshot.lastInBatch) P1SetDevFirstUse(false);
  if (rngWriteJobsPending > 0) rngWriteJobsPending--;

#ifdef XTRA_LOG
  if (!rngWritePending()) LogFile("RNG: all files writen", true);
#endif
}

//===========================================================================================
void writeRingFiles() {
  if (!EnableHistory || ((telegramCount - telegramErrors ) < 2) ) {
#ifdef XTRA_LOG  
    LogFile("RNG: exit before writing",true); //log only once
#endif    
    return; //do nothing or telegram < 2 skip 
  }

  if (rngWritePending()) {
#ifdef XTRA_LOG
    LogFile("RNG: write already pending", true);
#endif
    return;
  }

  WorkerRngPayload snapshot = {};
  snapshot.actT = actT;
  snapshot.newT = newT;
  strlcpy(snapshot.actTimestamp, actTimestamp, sizeof(snapshot.actTimestamp));
  strlcpy(snapshot.dsmrTimestamp, DSMRdata.timestamp.c_str(), sizeof(snapshot.dsmrTimestamp));
  snapshot.values[0] = (float)DSMRdata.energy_delivered_tariff1;
  snapshot.values[1] = (float)DSMRdata.energy_delivered_tariff2;
  snapshot.values[2] = (float)DSMRdata.energy_returned_tariff1;
  snapshot.values[3] = (float)DSMRdata.energy_returned_tariff2;
  snapshot.values[4] = (float)gasDelivered;
  snapshot.values[5] = mbusWater ? (float)waterDelivered
                                 : (float)P1Status.wtr_m3 + (float)P1Status.wtr_l / 1000.0f;
  snapshot.values[6] = currentSolarDailyKwh();

  RngWorkerMode modes[7];
  uint8_t jobCount = 0;
  if (P1Status.FirstUse) {
    modes[jobCount++] = RNG_WORKER_PREV_HOURS;
    modes[jobCount++] = RNG_WORKER_PREV_DAYS;
    modes[jobCount++] = RNG_WORKER_PREV_MONTHS;
  }
  modes[jobCount++] = RNG_WORKER_CURRENT_HOURS;
  modes[jobCount++] = RNG_WORKER_CURRENT_DAYS;
  modes[jobCount++] = RNG_WORKER_CURRENT_MONTHS;
  if (hour(actT) == 1 && hour(newT) == 3) modes[jobCount++] = RNG_WORKER_DST_GAP_HOURS;

  if (!WorkerHasCapacity(WORKER_PRIO_LOW, jobCount)) {
    LogFile("RNG: queue full, write skipped", true);
    return;
  }

  rngWriteJobsPending = jobCount;
  for (uint8_t i = 0; i < jobCount; i++) {
    WorkerRngPayload jobPayload = snapshot;
    jobPayload.mode = modes[i];
    jobPayload.lastInBatch = (i == jobCount - 1);
    if (!WorkerEnqueueRngWrite(jobPayload)) {
      rngWriteJobsPending = 0;
      LogFile("RNG: queue full, write skipped", true);
      return;
    }
  }
  
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
