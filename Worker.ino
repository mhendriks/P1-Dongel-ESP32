/*
***************************************************************************
**  Program  : Worker queue infrastructure
**  Purpose  : Priority based background jobs for non-realtime work
***************************************************************************
*/

QueueHandle_t qWorkerHigh = nullptr;
QueueHandle_t qWorkerNormal = nullptr;
QueueHandle_t qWorkerLow = nullptr;
TaskHandle_t tWorker = nullptr;
WorkerStats workerStats = {};
static volatile bool workerRngRunPermit = false;
static bool workerDeferredRngJobValid = false;
static WorkerJob workerDeferredRngJob = {};

void LogFileWriteDirect(const char* payload, bool toDebug);
void P1StatusWriteDirect();
void writeSettingsDirect();
void ManifestCheckFromWorker();
void PostWebhookFromWorker(const WorkerWebhookPayload& payload);
void RngWriteFromWorker(const WorkerRngPayload& payload);

static QueueHandle_t workerQueueForPriority(WorkerPriority priority) {
  switch (priority) {
    case WORKER_PRIO_HIGH:   return qWorkerHigh;
    case WORKER_PRIO_NORMAL: return qWorkerNormal;
    case WORKER_PRIO_LOW:    return qWorkerLow;
  }
  return qWorkerNormal;
}

static void workerCountEnqueued(WorkerPriority priority) {
  switch (priority) {
    case WORKER_PRIO_HIGH:   workerStats.enqueuedHigh++; break;
    case WORKER_PRIO_NORMAL: workerStats.enqueuedNormal++; break;
    case WORKER_PRIO_LOW:    workerStats.enqueuedLow++; break;
  }
}

static void workerCountDropped(WorkerPriority priority) {
  switch (priority) {
    case WORKER_PRIO_HIGH:   workerStats.droppedHigh++; break;
    case WORKER_PRIO_NORMAL: workerStats.droppedNormal++; break;
    case WORKER_PRIO_LOW:    workerStats.droppedLow++; break;
  }
}

static bool workerReceiveNext(WorkerJob& job) {
  static uint8_t highBudget = WORKER_HIGH_BUDGET;

  if (workerRngRunPermit && workerDeferredRngJobValid) {
    job = workerDeferredRngJob;
    workerDeferredRngJobValid = false;
    return true;
  }

  if (highBudget > 0 && qWorkerHigh && xQueueReceive(qWorkerHigh, &job, 0) == pdTRUE) {
    highBudget--;
    return true;
  }

  if (qWorkerNormal && xQueueReceive(qWorkerNormal, &job, 0) == pdTRUE) {
    highBudget = WORKER_HIGH_BUDGET;
    return true;
  }

  if (!workerDeferredRngJobValid && qWorkerLow && xQueueReceive(qWorkerLow, &job, 0) == pdTRUE) {
    highBudget = WORKER_HIGH_BUDGET;
    return true;
  }

  if (qWorkerHigh && xQueueReceive(qWorkerHigh, &job, 0) == pdTRUE) {
    highBudget = WORKER_HIGH_BUDGET - 1;
    return true;
  }

  highBudget = WORKER_HIGH_BUDGET;
  return false;
}

static bool workerRngShouldDefer(const WorkerJob& job) {
  return job.type == WORKER_JOB_RNG_WRITE && !workerRngRunPermit;
}

static void workerRngConsumePermit(const WorkerJob& job) {
  if (job.type != WORKER_JOB_RNG_WRITE) return;
  workerRngRunPermit = false;
}

static void workerHandleJob(const WorkerJob& job) {
  switch (job.type) {
    case WORKER_JOB_NONE:
      break;

    case WORKER_JOB_LOG_WRITE:
      LogFileWriteDirect(job.data.log.message, job.data.log.toDebug);
      break;

    case WORKER_JOB_P1_STATUS_WRITE:
      P1StatusWriteDirect();
      break;

    case WORKER_JOB_SETTINGS_WRITE:
      writeSettingsDirect();
      break;

    case WORKER_JOB_MANIFEST_CHECK:
      ManifestCheckFromWorker();
      break;

    case WORKER_JOB_HTTP_POST:
      PostWebhookFromWorker(job.data.webhook);
      break;

    case WORKER_JOB_RNG_WRITE:
      RngWriteFromWorker(job.data.rng);
      break;

    default:
      workerStats.unknown++;
      DebugTf("Worker: unhandled job type %u\r\n", (unsigned)job.type);
      break;
  }
}

void fWorker(void* pvParameters) {
  DebugTln(F("Start Worker Thread"));
  esp_task_wdt_add(nullptr);

  while (true) {
    WorkerJob job;
    if (workerReceiveNext(job)) {
      if (workerRngShouldDefer(job)) {
        workerDeferredRngJob = job;
        workerDeferredRngJobValid = true;
        vTaskDelay(20 / portTICK_PERIOD_MS);
        esp_task_wdt_reset();
        continue;
      }

      workerStats.processed++;
      workerRngConsumePermit(job);
      workerHandleJob(job);
    } else {
      vTaskDelay(20 / portTICK_PERIOD_MS);
    }

    esp_task_wdt_reset();
  }

  DebugTln(F("Worker: unexpected task exit"));
  vTaskDelete(NULL);
}

bool WorkerBegin() {
  if (tWorker) return true;

  qWorkerHigh = xQueueCreate(WORKER_QUEUE_HIGH_LEN, sizeof(WorkerJob));
  qWorkerNormal = xQueueCreate(WORKER_QUEUE_NORMAL_LEN, sizeof(WorkerJob));
  qWorkerLow = xQueueCreate(WORKER_QUEUE_LOW_LEN, sizeof(WorkerJob));

  if (!qWorkerHigh || !qWorkerNormal || !qWorkerLow) {
    DebugTln(F("Worker: queue creation failed"));
    return false;
  }

  if (xTaskCreate(fWorker, "worker", 1024 * 4, NULL, 4, &tWorker) != pdPASS) {
    DebugTln(F("Worker: task creation failed"));
    return false;
  }

  DebugTln(F("Task Worker succesfully created"));
  return true;
}

bool WorkerEnqueue(const WorkerJob& jobIn, WorkerPriority priority, TickType_t waitTicks) {
  QueueHandle_t queue = workerQueueForPriority(priority);
  if (!queue) {
    workerCountDropped(priority);
    return false;
  }

  WorkerJob job = jobIn;
  job.priority = priority;
  if (job.createdMs == 0) job.createdMs = millis();

  if (xQueueSend(queue, &job, waitTicks) == pdTRUE) {
    workerCountEnqueued(priority);
    return true;
  }

  workerCountDropped(priority);
  return false;
}

bool WorkerEnqueueSimple(WorkerJobType type, WorkerPriority priority, uint8_t id, uint16_t flags) {
  WorkerJob job = {};
  job.type = type;
  job.flags = flags;
  job.data.simple.id = id;
  return WorkerEnqueue(job, priority, 0);
}

bool WorkerHasCapacity(WorkerPriority priority, uint8_t needed) {
  QueueHandle_t queue = workerQueueForPriority(priority);
  return queue && uxQueueSpacesAvailable(queue) >= needed;
}

static WorkerJob workerJob(WorkerJobType type) {
  WorkerJob job = {};
  job.type = type;
  return job;
}

bool WorkerEnqueueLog(const char* payload, bool toDebug) {
  if (!tWorker || !qWorkerLow) return false;

  WorkerJob job = workerJob(WORKER_JOB_LOG_WRITE);
  job.data.log.toDebug = toDebug;
  strlcpy(job.data.log.message, payload ? payload : "", sizeof(job.data.log.message));
  return WorkerEnqueue(job, WORKER_PRIO_LOW, 0);
}

bool WorkerEnqueueWebhookPost(const WorkerWebhookPayload& payload) {
  if (!tWorker || !qWorkerNormal) return false;

  WorkerJob job = workerJob(WORKER_JOB_HTTP_POST);
  job.data.webhook = payload;
  return WorkerEnqueue(job, WORKER_PRIO_NORMAL, 0);
}

bool WorkerEnqueueRngWrite(const WorkerRngPayload& payload) {
  if (!tWorker || !qWorkerLow) return false;

  WorkerJob job = workerJob(WORKER_JOB_RNG_WRITE);
  job.data.rng = payload;
  return WorkerEnqueue(job, WORKER_PRIO_LOW, 0);
}

void WorkerNotifyP1TelegramOk() {
  workerRngRunPermit = true;
}

void WorkerPrintStats() {
  DebugTf("Worker queues: H +%lu/-%lu N +%lu/-%lu L +%lu/-%lu processed=%lu unknown=%lu\r\n",
          workerStats.enqueuedHigh,
          workerStats.droppedHigh,
          workerStats.enqueuedNormal,
          workerStats.droppedNormal,
          workerStats.enqueuedLow,
          workerStats.droppedLow,
          workerStats.processed,
          workerStats.unknown);
}
