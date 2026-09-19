/* Generic HTTP push connector; configured through DSMRsettings.json. */

static WiFiClientSecure webhookTlsClient;
static uint8_t webhookPostErrors = 0;
static uint32_t webhookLastPostMs = 0;
static bool webhookPostPending = false;
static int webhookLastHttpStatus = 0;
static time_t webhookLastSuccessfulPost = 0;
static bool webhookFailureLogged = false;

static bool httpPostConfigured() {
  return bHttpPostEnabled && strlen(settingHttpPostUrl) > 0;
}

static uint32_t httpPostIntervalMs() {
  return (uint32_t)constrain(settingHttpPostInterval, (uint16_t)1, (uint16_t)3600) * 1000UL;
}

static const char* httpPostStatusText() {
  if (!bHttpPostEnabled) return "uitgeschakeld";
  if (!strlen(settingHttpPostUrl)) return "onvolledig ingesteld";
  if (webhookLastHttpStatus >= 200 && webhookLastHttpStatus < 300) return "connected";
  if (webhookLastHttpStatus != 0) return "fout";
  return "wacht op verzending";
}

static void logHttpPostFailureOnce(int status) {
  if (webhookFailureLogged) return;
  char message[64];
  if (status < 0) snprintf(message, sizeof(message), "HTTP POST fout: verbinding mislukt");
  else snprintf(message, sizeof(message), "HTTP POST fout: HTTP %d", status);
  LogFile(message, true);
  webhookFailureLogged = true;
}

static void logHttpPostRecovery() {
  if (!webhookFailureLogged) return;
  LogFile("HTTP POST hersteld: verzending gelukt", true);
  webhookFailureLogged = false;
}

void AppendHttpPostStatus(JsonDocument& doc) {
  doc["http_post_status"] = httpPostStatusText();
  doc["http_post_enabled"] = bHttpPostEnabled;
  if (webhookLastSuccessfulPost) doc["http_post_last_success"] = webhookLastSuccessfulPost;
  if (webhookLastHttpStatus) doc["http_post_http_status"] = webhookLastHttpStatus;
}

static String jsonWebhook(const WorkerWebhookPayload& payload) {
  JsonDocument doc;
  char idbuf[21];
  snprintf(idbuf, sizeof(idbuf), "%llu", (unsigned long long)payload.id);
  doc["id"] = idbuf;
  doc["p_from_grid"] = payload.pFromGrid;
  doc["p_to_grid"] = payload.pToGrid;
  doc["timestamp"] = payload.timestamp;
  if (settingHttpPostPayload == 1 || settingHttpPostPayload == 3) {
    doc["t1"] = payload.t1;
    doc["t2"] = payload.t2;
    doc["t1r"] = payload.t1r;
    doc["t2r"] = payload.t2r;
  }
  if (settingHttpPostPayload == 2 || settingHttpPostPayload == 3) {
    const char* voltageKeys[] = { "v_l1", "v_l2", "v_l3" };
    const char* sagKeys[] = { "voltage_sag_l1_count", "voltage_sag_l2_count", "voltage_sag_l3_count" };
    const char* swellKeys[] = { "voltage_swell_l1_count", "voltage_swell_l2_count", "voltage_swell_l3_count" };
    for (uint8_t phase = 0; phase < 3; phase++) {
      if (payload.voltagePresentMask & (1U << phase)) doc[voltageKeys[phase]] = payload.voltage[phase];
      if (payload.sagSwellPresentMask & (1U << phase)) doc[sagKeys[phase]] = payload.voltageSags[phase];
      if (payload.sagSwellPresentMask & (1U << (phase + 3))) doc[swellKeys[phase]] = payload.voltageSwells[phase];
    }
  }
  if (settingHttpPostAuth == 3 && strlen(settingHttpPostAuthName) && strlen(settingHttpPostAuthKey)) {
    doc[settingHttpPostAuthName] = settingHttpPostAuthKey;
  }
  String output;
  serializeJson(doc, output);
  return output;
}

static void applyServerInterval(const String& response) {
  if (!settingHttpPostAcceptInterval) return;
  JsonDocument doc;
  if (deserializeJson(doc, response) || !doc["interval"].is<int>()) return;
  const int requestedInterval = doc["interval"].as<int>();
  if (requestedInterval < 1 || requestedInterval > 3600) return;
  const uint16_t interval = (uint16_t)requestedInterval;
  if (interval == settingHttpPostInterval) return;
  settingHttpPostInterval = interval;
  writeSettingsDirect();
}

void PostWebhook() {
  if (!bNewTelegramWebhook || netw_state == NW_NONE || webhookPostPending || !httpPostConfigured()) return;
  if (webhookLastPostMs && (uint32_t)(millis() - webhookLastPostMs) < httpPostIntervalMs()) return;
  WorkerWebhookPayload payload = {};
  payload.id = _getChipId();
  payload.pFromGrid = outputPowerInt(DSMRdata.power_delivered.int_val());
  payload.pToGrid = outputPowerInt(DSMRdata.power_returned.int_val());
  payload.t1 = DSMRdata.energy_delivered_tariff1_present ? outputEnergyUint64(DSMRdata.energy_delivered_tariff1.int_val()) : 0;
  payload.t2 = DSMRdata.energy_delivered_tariff2_present ? outputEnergyUint64(DSMRdata.energy_delivered_tariff2.int_val()) : 0;
  payload.t1r = DSMRdata.energy_returned_tariff1_present ? outputEnergyUint64(DSMRdata.energy_returned_tariff1.int_val()) : 0;
  payload.t2r = DSMRdata.energy_returned_tariff2_present ? outputEnergyUint64(DSMRdata.energy_returned_tariff2.int_val()) : 0;
  payload.timestamp = actT;
  if (DSMRdata.voltage_l1_present) { payload.voltage[0] = (uint32_t)lroundf(outputVoltage(DSMRdata.voltage_l1.val()) * 10.0f); payload.voltagePresentMask |= 1U << 0; }
  if (DSMRdata.voltage_l2_present) { payload.voltage[1] = (uint32_t)lroundf(outputVoltage(DSMRdata.voltage_l2.val()) * 10.0f); payload.voltagePresentMask |= 1U << 1; }
  if (DSMRdata.voltage_l3_present) { payload.voltage[2] = (uint32_t)lroundf(outputVoltage(DSMRdata.voltage_l3.val()) * 10.0f); payload.voltagePresentMask |= 1U << 2; }
  if (DSMRdata.electricity_sags_l1_present) { payload.voltageSags[0] = DSMRdata.electricity_sags_l1; payload.sagSwellPresentMask |= 1U << 0; }
  if (DSMRdata.electricity_sags_l2_present) { payload.voltageSags[1] = DSMRdata.electricity_sags_l2; payload.sagSwellPresentMask |= 1U << 1; }
  if (DSMRdata.electricity_sags_l3_present) { payload.voltageSags[2] = DSMRdata.electricity_sags_l3; payload.sagSwellPresentMask |= 1U << 2; }
  if (DSMRdata.electricity_swells_l1_present) { payload.voltageSwells[0] = DSMRdata.electricity_swells_l1; payload.sagSwellPresentMask |= 1U << 3; }
  if (DSMRdata.electricity_swells_l2_present) { payload.voltageSwells[1] = DSMRdata.electricity_swells_l2; payload.sagSwellPresentMask |= 1U << 4; }
  if (DSMRdata.electricity_swells_l3_present) { payload.voltageSwells[2] = DSMRdata.electricity_swells_l3; payload.sagSwellPresentMask |= 1U << 5; }
  if (!WorkerEnqueueWebhookPost(payload)) return;
  bNewTelegramWebhook = false;
  webhookPostPending = true;
}

void PostWebhookFromWorker(const WorkerWebhookPayload& payload) {
  if (netw_state == NW_NONE || !httpPostConfigured()) { webhookPostPending = false; return; }
  HTTPClient http;
  if (!http.begin(webhookTlsClient, settingHttpPostUrl)) {
    webhookLastHttpStatus = -1;
    webhookPostErrors++;
    webhookLastPostMs = millis(); // Retry at the configured interval, not every telegram.
    logHttpPostFailureOnce(webhookLastHttpStatus);
    webhookTlsClient.stop();
    webhookPostPending = false;
    return;
  }
  http.setTimeout(15000);
  http.addHeader("Content-Type", "application/json");
  if (settingHttpPostAuth == 1 && strlen(settingHttpPostAuthKey)) {
    http.addHeader("Authorization", "Bearer " + String(settingHttpPostAuthKey));
  } else if (settingHttpPostAuth == 2 && strlen(settingHttpPostAuthName) && strlen(settingHttpPostAuthKey)) {
    http.addHeader(settingHttpPostAuthName, settingHttpPostAuthKey);
  }
  if (strlen(settingHttpPostExtraHeaderName) && strlen(settingHttpPostExtraHeaderValue)) {
    String value = settingHttpPostExtraHeaderValue;
    value.replace("{mac}", macID);
    http.addHeader(settingHttpPostExtraHeaderName, value);
  }
  webhookLastHttpStatus = http.POST(jsonWebhook(payload));
  webhookLastPostMs = millis();
  const String response = http.getString();
  if (webhookLastHttpStatus >= 200 && webhookLastHttpStatus < 300) {
    webhookPostErrors = 0;
    webhookLastSuccessfulPost = payload.timestamp;
    applyServerInterval(response);
    logHttpPostRecovery();
  } else {
    webhookPostErrors++;
    logHttpPostFailureOnce(webhookLastHttpStatus);
    webhookTlsClient.stop();
  }
  http.end();
  webhookPostPending = false;
}

void StartWebhook() {
  webhookTlsClient.setInsecure();
  webhookTlsClient.setTimeout(15000);
}
