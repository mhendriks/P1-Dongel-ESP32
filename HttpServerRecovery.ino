/*
***************************************************************************
**  Program  : HttpServerRecovery
**  Purpose  : recover stale synchronous WebServer client state
***************************************************************************
*/

static const uint32_t HTTP_STALE_WAIT_READ_MS = 10000;
static const uint32_t HTTP_RECOVERY_CHECK_INTERVAL_MS = 1000;
static uint32_t httpRecoveryLastCheckMs = 0;

void handleHttpServerClient() {
  httpServerHandleActive = true;
  httpServer.handleClient();
  httpServerHandleActive = false;
}

void serviceHttpServerRecovery() {
  if (httpServerHandleActive) return;

  uint32_t nowMs = millis();
  if ((uint32_t)(nowMs - httpRecoveryLastCheckMs) < HTTP_RECOVERY_CHECK_INTERVAL_MS) return;
  httpRecoveryLastCheckMs = nowMs;

  httpServer.recoverStaleWaitRead(HTTP_STALE_WAIT_READ_MS);
}
