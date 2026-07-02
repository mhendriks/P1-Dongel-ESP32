/*
***************************************************************************
**  Program  : ApiWebSocket, live browser API stream for DSMRloggerAPI
**
**  TERMS OF USE: MIT License.
***************************************************************************
*/

static const uint32_t API_WS_LIVE_MIN_INTERVAL_MS = 1000;
static const uint32_t API_WS_TIME_INTERVAL_MS = 5000;
static const uint32_t API_WS_HIST_INTERVAL_MS = 3600000;
static const uint32_t API_WS_SEND_WARN_MS = 500;
static const uint32_t API_WS_WRITE_READY_TIMEOUT_MS = 20;
static const uint8_t API_WS_MAX_CLIENTS = 3;
static volatile bool apiWsLiveDirty = false;
static uint32_t apiWsLastLive = 0;
static uint32_t apiWsLastTime = 0;
static uint32_t apiWsLastHist = 0;

static void apiWsDropClient(uint8_t clientNum, const char* reason) {
  if (clientNum >= WEBSOCKETS_SERVER_CLIENT_MAX) return;
  if (!apiWs.clientIsConnected(clientNum)) return;

  DebugTf("API WS client disconnecting: slot=%u reason=%s\r\n", clientNum, reason);
  apiWs.forceDisconnect(clientNum);
}

static String apiWsMessage(const char* source, const String& body) {
  String out;
  out.reserve(strlen(source) + body.length() + 22);
  out += F("{\"source\":\"");
  out += source;
  out += F("\",\"data\":");
  out += body;
  out += '}';
  return out;
}

static bool apiWsSend(uint8_t clientNum, const char* source, const String& body) {
  if (!apiWs.clientCanWrite(clientNum, API_WS_WRITE_READY_TIMEOUT_MS)) {
    apiWsDropClient(clientNum, "not-writable");
    return false;
  }

  String msg = apiWsMessage(source, body);
  uint32_t started = millis();
  bool sent = apiWs.sendTXT(clientNum, msg);
  uint32_t elapsed = millis() - started;

  if (elapsed > API_WS_SEND_WARN_MS) {
    DebugTf("API WS send slow: slot=%u source=%s ms=%lu sent=%d\r\n", clientNum, source, elapsed, sent);
  }
  if (!sent) {
    apiWsDropClient(clientNum, "send-failed");
    return false;
  }
  return true;
}

static bool apiWsBroadcast(const char* source, const String& body) {
  if (!apiWs.connectedClients()) return false;
  String msg = apiWsMessage(source, body);
  bool sent = false;

  for (uint8_t clientNum = 0; clientNum < WEBSOCKETS_SERVER_CLIENT_MAX; clientNum++) {
    if (!apiWs.clientIsConnected(clientNum)) continue;

    if (!apiWs.clientCanWrite(clientNum, API_WS_WRITE_READY_TIMEOUT_MS)) {
      apiWsDropClient(clientNum, "not-writable");
      continue;
    }

    uint32_t started = millis();
    bool clientSent = apiWs.sendTXT(clientNum, msg);
    uint32_t elapsed = millis() - started;

    if (elapsed > API_WS_SEND_WARN_MS) {
      DebugTf("API WS send slow: slot=%u source=%s ms=%lu sent=%d\r\n", clientNum, source, elapsed, clientSent);
    }
    if (!clientSent) {
      apiWsDropClient(clientNum, "send-failed");
      continue;
    }
    sent = true;
  }
  return sent;
}

static void apiWsSendSnapshot(uint8_t clientNum) {
  apiWsSend(clientNum, "time", deviceTimeJson());
  apiWsSend(clientNum, "dash_live", dashLiveApiResponse().body);
  apiWsSend(clientNum, "actual", smActualJsonDebug());
  apiWsSend(clientNum, "dash_hist", dashHistoryApiResponse().body);
}

static void apiWsEvent(uint8_t clientNum, WStype_t type, uint8_t* payload, size_t len) {
  (void)len;

  if (type == WStype_CONNECTED) {
    if (strcmp((const char*)payload, "/ws") != 0) {
      apiWs.disconnect(clientNum);
      return;
    }
    if (apiWs.connectedClients() > API_WS_MAX_CLIENTS) {
      DebugTf("API WS max clients reached: slot=%u clients=%d\r\n", clientNum, apiWs.connectedClients());
      apiWs.disconnect(clientNum);
      return;
    }
    DebugTf("API WS client connected: slot=%u clients=%d\r\n", clientNum, apiWs.connectedClients());
    apiWsSendSnapshot(clientNum);
  } else if (type == WStype_DISCONNECTED) {
    DebugTf("API WS client disconnected: slot=%u clients=%d\r\n", clientNum, apiWs.connectedClients());
  }
}

void setupApiWebSocket() {
  if (skipNetwork) return;
  if (strlen(bAuthUser)) apiWs.setAuthorization(bAuthUser, bAuthPW);
  apiWs.onEvent(apiWsEvent);
  apiWs.enableHeartbeat(15000, 3000, 2);
  apiWs.begin();
  DebugTln(F("API WebSocket server started on port 81\r"));
}

void apiWsMarkLiveDirty() {
  apiWsLiveDirty = true;
}

void handleApiWebSocket() {
  if (skipNetwork) return;
  apiWs.loop();
  if (!apiWs.connectedClients()) return;

  uint32_t nowMs = millis();
  if (apiWsLiveDirty && nowMs - apiWsLastLive >= API_WS_LIVE_MIN_INTERVAL_MS) {
    apiWsLastLive = nowMs;
    bool sent = apiWsBroadcast("dash_live", dashLiveApiResponse().body);
    sent = apiWsBroadcast("actual", smActualJsonDebug()) && sent;
    if (!sent) return;
    apiWsLiveDirty = false;
  }

  if (nowMs - apiWsLastTime >= API_WS_TIME_INTERVAL_MS) {
    apiWsLastTime = nowMs;
    apiWsBroadcast("time", deviceTimeJson());
  }

  if (nowMs - apiWsLastHist >= API_WS_HIST_INTERVAL_MS) {
    apiWsLastHist = nowMs;
    apiWsBroadcast("dash_hist", dashHistoryApiResponse().body);
  }
}
