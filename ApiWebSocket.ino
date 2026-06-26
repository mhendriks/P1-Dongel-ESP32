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
static const uint8_t API_WS_MAX_CLIENTS = 3;
static volatile bool apiWsLiveDirty = false;
static uint32_t apiWsLastLive = 0;
static uint32_t apiWsLastTime = 0;
static uint32_t apiWsLastHist = 0;

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
  String msg = apiWsMessage(source, body);
  return apiWs.sendTXT(clientNum, msg);
}

static bool apiWsBroadcast(const char* source, const String& body) {
  if (!apiWs.connectedClients()) return false;
  String msg = apiWsMessage(source, body);
  return apiWs.broadcastTXT(msg);
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
      apiWs.disconnect(clientNum);
      return;
    }
    DebugTf("API WS client connected: %u\r\n", clientNum);
    apiWsSendSnapshot(clientNum);
  } else if (type == WStype_DISCONNECTED) {
    DebugTf("API WS client disconnected: %u\r\n", clientNum);
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
    bool sent = apiWsBroadcast("dash_live", dashLiveApiResponse().body);
    sent = apiWsBroadcast("actual", smActualJsonDebug()) && sent;
    if (!sent) return;
    apiWsLiveDirty = false;
    apiWsLastLive = nowMs;
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
