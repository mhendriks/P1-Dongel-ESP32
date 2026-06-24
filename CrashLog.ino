/*
***************************************************************************
**  Program  : CrashLog
**  Purpose  : RTC breadcrumbs for post-panic diagnostics
***************************************************************************
*/

#if ENABLE_CRASH_BREADCRUMBS

#if ENABLE_CRASH_COREDUMP_SUMMARY
  #include "esp_core_dump.h"
#endif

#define CRASH_LOG_MAGIC 0x43524C48UL
#define CRASH_LOG_SIZE  5

struct CrashLogEntry {
  uint32_t seq;
  uint32_t ms;
  uint32_t heap;
  uint32_t maxAlloc;
  uint16_t line;
  uint8_t core;
  char tag[18];
};

struct CrashLogState {
  uint32_t magic;
  uint32_t bootCount;
  uint32_t seq;
  uint8_t valid;
  char resetReason[48];
  CrashLogEntry entries[CRASH_LOG_SIZE];
};

RTC_NOINIT_ATTR static CrashLogState crashLogRtc;

static bool crashLogAbnormalReset() {
  esp_reset_reason_t r = esp_reset_reason();
  return r == ESP_RST_PANIC
      || r == ESP_RST_INT_WDT
      || r == ESP_RST_TASK_WDT
      || r == ESP_RST_WDT
      || r == ESP_RST_BROWNOUT;
}

static void crashLogInitIfNeeded() {
  if (crashLogRtc.magic == CRASH_LOG_MAGIC) return;
  memset(&crashLogRtc, 0, sizeof(crashLogRtc));
  crashLogRtc.magic = CRASH_LOG_MAGIC;
}

void CrashLogMark(const char* tag, uint16_t line) {
  crashLogInitIfNeeded();

  uint32_t seq = ++crashLogRtc.seq;
  CrashLogEntry& entry = crashLogRtc.entries[seq % CRASH_LOG_SIZE];
  entry.seq = seq;
  entry.ms = millis();
  entry.heap = ESP.getFreeHeap();
  entry.maxAlloc = ESP.getMaxAllocHeap();
  entry.line = line;
  entry.core = xPortGetCoreID();
  strlcpy(entry.tag, tag ? tag : "-", sizeof(entry.tag));
  crashLogRtc.valid = 1;
}

void CrashLogBegin(const char* resetReason) {
  crashLogInitIfNeeded();
  crashLogRtc.bootCount++;
  strlcpy(crashLogRtc.resetReason, resetReason ? resetReason : "", sizeof(crashLogRtc.resetReason));
}

void CrashLogPrint() {
  crashLogInitIfNeeded();
  if (!crashLogRtc.valid) return;

  DebugTf("CrashLog boot=%lu reset=%s lastSeq=%lu\r\n",
          (unsigned long)crashLogRtc.bootCount,
          crashLogRtc.resetReason,
          (unsigned long)crashLogRtc.seq);

  uint32_t first = crashLogRtc.seq > CRASH_LOG_SIZE ? crashLogRtc.seq - CRASH_LOG_SIZE + 1 : 1;
  for (uint32_t seq = first; seq <= crashLogRtc.seq; seq++) {
    const CrashLogEntry& entry = crashLogRtc.entries[seq % CRASH_LOG_SIZE];
    if (entry.seq != seq) continue;
    char line[128];
    snprintf(line, sizeof(line),
             "crashlog seq=%lu ms=%lu core=%u heap=%lu max=%lu line=%u tag=%s",
             (unsigned long)entry.seq,
             (unsigned long)entry.ms,
             (unsigned)entry.core,
             (unsigned long)entry.heap,
             (unsigned long)entry.maxAlloc,
             (unsigned)entry.line,
             entry.tag);
    DebugTln(line);
  }
}

void CrashLogPersistAbnormalReset() {
  crashLogInitIfNeeded();
  if (!crashLogRtc.valid || !crashLogAbnormalReset()) return;

  char header[128];
  snprintf(header, sizeof(header),
           "crashlog boot=%lu reset=%s lastSeq=%lu",
           (unsigned long)crashLogRtc.bootCount,
           crashLogRtc.resetReason,
           (unsigned long)crashLogRtc.seq);
  LogFile(header, true);

  uint32_t first = crashLogRtc.seq > CRASH_LOG_SIZE ? crashLogRtc.seq - CRASH_LOG_SIZE + 1 : 1;
  for (uint32_t seq = first; seq <= crashLogRtc.seq; seq++) {
    const CrashLogEntry& entry = crashLogRtc.entries[seq % CRASH_LOG_SIZE];
    if (entry.seq != seq) continue;
    char line[128];
    snprintf(line, sizeof(line),
             "crashlog seq=%lu ms=%lu core=%u heap=%lu max=%lu line=%u tag=%s",
             (unsigned long)entry.seq,
             (unsigned long)entry.ms,
             (unsigned)entry.core,
             (unsigned long)entry.heap,
             (unsigned long)entry.maxAlloc,
             (unsigned)entry.line,
             entry.tag);
    LogFile(line, true);
  }

#if ENABLE_CRASH_COREDUMP_SUMMARY
  esp_err_t dumpErr = esp_core_dump_image_check();
  if (dumpErr == ESP_OK) {
    size_t dumpAddr = 0;
    size_t dumpSize = 0;
    if (esp_core_dump_image_get(&dumpAddr, &dumpSize) == ESP_OK) {
      char dumpLine[128];
      snprintf(dumpLine, sizeof(dumpLine),
               "coredump image addr=0x%lx size=%lu",
               (unsigned long)dumpAddr,
               (unsigned long)dumpSize);
      LogFile(dumpLine, true);
    }

    char panicReason[160];
    if (esp_core_dump_get_panic_reason(panicReason, sizeof(panicReason)) == ESP_OK) {
      char reasonLine[192];
      snprintf(reasonLine, sizeof(reasonLine), "coredump reason=%s", panicReason);
      LogFile(reasonLine, true);
    }

    esp_core_dump_summary_t* summary = (esp_core_dump_summary_t*)malloc(sizeof(esp_core_dump_summary_t));
    if (summary) {
      if (esp_core_dump_get_summary(summary) == ESP_OK) {
        char summaryLine[192];
        char elfLine[96];
        snprintf(elfLine, sizeof(elfLine), "coredump elf_sha=%s", summary->app_elf_sha256);
        LogFile(elfLine, true);

#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32P4)
        snprintf(summaryLine, sizeof(summaryLine),
                 "coredump task=%s tcb=0x%lx pc=0x%lx ra=0x%lx sp=0x%lx mcause=0x%lx mtval=0x%lx stack=%lu",
                 summary->exc_task,
                 (unsigned long)summary->exc_tcb,
                 (unsigned long)summary->exc_pc,
                 (unsigned long)summary->ex_info.ra,
                 (unsigned long)summary->ex_info.sp,
                 (unsigned long)summary->ex_info.mcause,
                 (unsigned long)summary->ex_info.mtval,
                 (unsigned long)summary->exc_bt_info.dump_size);
        LogFile(summaryLine, true);
#else
        snprintf(summaryLine, sizeof(summaryLine),
                 "coredump task=%s tcb=0x%lx pc=0x%lx cause=0x%lx vaddr=0x%lx btDepth=%lu corrupt=%u",
                 summary->exc_task,
                 (unsigned long)summary->exc_tcb,
                 (unsigned long)summary->exc_pc,
                 (unsigned long)summary->ex_info.exc_cause,
                 (unsigned long)summary->ex_info.exc_vaddr,
                 (unsigned long)summary->exc_bt_info.depth,
                 summary->exc_bt_info.corrupted ? 1 : 0);
        LogFile(summaryLine, true);

        if (summary->exc_bt_info.depth > 0) {
          char btLine[192];
          size_t pos = snprintf(btLine, sizeof(btLine), "coredump bt=");
          uint32_t count = summary->exc_bt_info.depth;
          if (count > 8) count = 8;
          for (uint32_t i = 0; i < count && pos < sizeof(btLine); i++) {
            pos += snprintf(btLine + pos, sizeof(btLine) - pos,
                            "%s0x%lx", i ? "," : "",
                            (unsigned long)summary->exc_bt_info.bt[i]);
          }
          LogFile(btLine, true);
        }
#endif
      }
      free(summary);
    } else {
      LogFile("coredump summary skipped: no heap", true);
    }
  } else {
    char dumpLine[96];
    snprintf(dumpLine, sizeof(dumpLine), "coredump not available err=0x%lx", (unsigned long)dumpErr);
    LogFile(dumpLine, true);
  }
#endif
}

#endif
