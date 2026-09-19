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

#define WORKER_QUEUE_NORMAL_LEN  8
#define WORKER_QUEUE_LOW_LEN     8
#define WORKER_STACK_BYTES   (1024 * 12)

enum WorkerPriority : uint8_t {
  WORKER_PRIO_NORMAL = 0,
  WORKER_PRIO_LOW
};

enum WorkerJobType : uint8_t {
  WORKER_JOB_NONE = 0,
  WORKER_JOB_LOG_WRITE,
  WORKER_JOB_SETTINGS_WRITE,
  WORKER_JOB_P1_STATUS_WRITE,
  WORKER_JOB_RNG_WRITE,
  WORKER_JOB_HTTP_POST,
  WORKER_JOB_MEENT_PROVISION,
  WORKER_JOB_MANIFEST_CHECK,
  WORKER_JOB_SOLAR_FETCH
};

struct WorkerLogPayload {
  char message[96];
  bool toDebug;
};

struct WorkerWebhookPayload {
  uint64_t id;
  int32_t pFromGrid;
  int32_t pToGrid;
  uint64_t t1;
  uint64_t t2;
  uint64_t t1r;
  uint64_t t2r;
  time_t timestamp;
  uint32_t voltage[3];
  uint32_t voltageSags[3];
  uint32_t voltageSwells[3];
  uint8_t voltagePresentMask;
  uint8_t sagSwellPresentMask;
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
  union {
    WorkerLogPayload log;
    WorkerWebhookPayload webhook;
    WorkerRngPayload rng;
  } data;
};

bool WorkerBegin();
bool WorkerEnqueue(const WorkerJob& job, WorkerPriority priority, TickType_t waitTicks = 0);
bool WorkerEnqueueSimple(WorkerJobType type, WorkerPriority priority);
bool WorkerHasCapacity(WorkerPriority priority, uint8_t needed);
bool WorkerEnqueueLog(const char* payload, bool toDebug);
bool WorkerEnqueueWebhookPost(const WorkerWebhookPayload& payload);
bool WorkerEnqueueRngWrite(const WorkerRngPayload& payload);
bool WorkerEnqueueSolarFetch();
void WorkerNotifyP1TelegramOk();
void WorkerPrintStats();

#endif
