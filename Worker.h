/*
***************************************************************************
**  Program  : Worker queue infrastructure
**  Purpose  : Priority based background jobs for non-realtime work
***************************************************************************
*/
#ifndef _WORKER_H
#define _WORKER_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define WORKER_QUEUE_HIGH_LEN    8
#define WORKER_QUEUE_NORMAL_LEN 16
#define WORKER_QUEUE_LOW_LEN    16
#define WORKER_HIGH_BUDGET       5

enum WorkerPriority : uint8_t {
  WORKER_PRIO_HIGH = 0,
  WORKER_PRIO_NORMAL,
  WORKER_PRIO_LOW
};

enum WorkerJobType : uint8_t {
  WORKER_JOB_NONE = 0,
  WORKER_JOB_LOG_WRITE,
  WORKER_JOB_SETTINGS_WRITE,
  WORKER_JOB_P1_STATUS_WRITE,
  WORKER_JOB_RNG_WRITE,
  WORKER_JOB_HTTP_POST,
  WORKER_JOB_MANIFEST_CHECK
};

struct WorkerLogPayload {
  char message[96];
  bool toDebug;
};

struct WorkerSimplePayload {
  uint8_t id;
};

struct WorkerWebhookPayload {
  uint64_t id;
  int32_t pFromGrid;
  int32_t pToGrid;
  time_t timestamp;
};

struct WorkerRngPayload {
  uint8_t mode;
  bool lastInBatch;
  time_t actT;
  time_t newT;
  char actTimestamp[20];
  char dsmrTimestamp[20];
  float values[7];
};

struct WorkerJob {
  WorkerJobType type;
  WorkerPriority priority;
  uint32_t createdMs;
  uint16_t flags;
  union {
    WorkerLogPayload log;
    WorkerSimplePayload simple;
    WorkerWebhookPayload webhook;
    WorkerRngPayload rng;
    uint8_t raw[112];
  } data;
};

struct WorkerStats {
  uint32_t enqueuedHigh;
  uint32_t enqueuedNormal;
  uint32_t enqueuedLow;
  uint32_t droppedHigh;
  uint32_t droppedNormal;
  uint32_t droppedLow;
  uint32_t processed;
  uint32_t unknown;
};

bool WorkerBegin();
bool WorkerEnqueue(const WorkerJob& job, WorkerPriority priority, TickType_t waitTicks = 0);
bool WorkerEnqueueSimple(WorkerJobType type, WorkerPriority priority, uint8_t id = 0, uint16_t flags = 0);
bool WorkerHasCapacity(WorkerPriority priority, uint8_t needed);
bool WorkerEnqueueLog(const char* payload, bool toDebug);
bool WorkerEnqueueWebhookPost(const WorkerWebhookPayload& payload);
bool WorkerEnqueueRngWrite(const WorkerRngPayload& payload);
void WorkerNotifyP1TelegramOk();
void WorkerPrintStats();

#endif
