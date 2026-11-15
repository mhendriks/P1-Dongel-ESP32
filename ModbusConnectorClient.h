#pragma once
#include "ConnectorModel.h"
#include "ConnectorDiagnostics.h"

// The Modbus callback only copies the response. All decoding, configuration,
// polling and dashboard updates run on the main loop, never on the TCP task.
class ModbusConnectorClient {
 public:
  ModbusConnectorClient() : client(socket, 2) {}
  void configure(const ConnectorConfig& config, const ConnectorField* fields, uint8_t count, bool sunSpec) {
    portENTER_CRITICAL(&mux);
    activeToken = ++token;
    received = false;
    portEXIT_CRITICAL(&mux);
    diagnostics.reset(diagnostics.enabled, millis());
    pending = ready = valid = false;
    target = config;
    fieldCount = count;
    for (uint8_t i = 0; i < count; i++) mappings[i] = fields[i];
    useSunSpecSentinels = sunSpec;
    fieldIndex = 0;
    readingScale = false;
    lastCycleMs = 0;
    IPAddress address;
    if (!config.enabled || !address.fromString(config.ip)) return;
    for (uint8_t i = 0; i < count; i++) {
      if (uint32_t(mappings[i].reg) + connectorWordCount(mappings[i]) > 65536UL) return;
    }
    if (!started) {
      client.onDataHandler([this](ModbusMessage msg, uint32_t t) { receive(msg, t); });
      client.onErrorHandler([this](Error error, uint32_t t) { receive(ModbusMessage(), t, uint16_t(error)); });
      client.setTimeout(1500, 200);
      client.begin();
      started = true;
    }
    client.setTarget(address, config.port);
    valid = true;
  }
  bool poll(bool networkAvailable, double* output) {
    if (!valid || !networkAvailable) return false;
    uint16_t first = 0, second = 0;
    bool responseReady, responseOk;
    uint16_t error;
    portENTER_CRITICAL(&mux);
    responseReady = received;
    responseOk = success;
    error = responseError;
    if (received) { first = words[0]; second = words[1]; received = false; }
    portEXIT_CRITICAL(&mux);
    if (responseReady) {
      pending = false;
      const ConnectorField& field = mappings[fieldIndex];
      bool ok = responseOk;
      if (ok && readingScale) ok = applyConnectorExponent(values[fieldIndex], first);
      else if (ok) {
        // SunSpec int16/acc32 invalid values must not become real readings.
        if (useSunSpecSentinels && ((field.type == CONNECTOR_S16 && first == 0x8000) ||
            (field.type == CONNECTOR_U32 && first == 0 && second == 0))) ok = false;
        if (ok) ok = decodeConnectorValue(field, first, second, values[fieldIndex]);
      }
      ConnectorDiagnosticEntry entry;
      if (diagnostics.enabled) {
        entry.atMs = millis(); entry.reg = readingScale ? field.sf : field.reg;
        entry.field = fieldIndex; entry.exponent = readingScale;
        entry.count = readingScale ? 1 : connectorWordCount(field);
        entry.hasRaw = responseOk; entry.raw[0] = first; entry.raw[1] = second;
        entry.hasValue = ok; entry.value = ok ? values[fieldIndex] : 0;
        entry.error = ok ? 0 : (error ? error : (readingScale ? 0x102 : 0x101));
        diagnostics.record(entry);
      }
      if (!ok) {
        fieldIndex = 0; readingScale = false; lastCycleMs = millis();
      } else if (!readingScale && field.sf) {
        readingScale = true;
      } else {
        readingScale = false;
        if (++fieldIndex == fieldCount) {
          fieldIndex = 0;
          lastCycleMs = millis();
          ready = true;
        }
      }
    }
    if (!pending && (fieldIndex || readingScale || !lastCycleMs ||
        uint32_t(millis() - lastCycleMs) >= uint32_t(target.interval) * 1000UL)) {
      const ConnectorField& field = mappings[fieldIndex];
      const uint16_t reg = readingScale ? field.sf : field.reg;
      const uint8_t count = readingScale ? 1 : connectorWordCount(field);
      portENTER_CRITICAL(&mux);
      activeToken = ++token;
      expectedWords = count;
      expectedUnit = target.id;
      portEXIT_CRITICAL(&mux);
      pending = true;
      if (diagnostics.enabled) ++diagnostics.requests;
      Error error = client.addRequest(token, target.id, READ_HOLD_REGISTER, reg, count);
      if (error != SUCCESS) {
        recordFailure(uint16_t(error), fieldIndex, readingScale);
        pending = false; fieldIndex = 0; readingScale = false; lastCycleMs = millis();
      }
    }
    if (!ready) return false;
    ready = false;
    for (uint8_t i = 0; i < fieldCount; i++) output[i] = values[i];
    return true;
  }
  void setDiagnosticsEnabled(bool enabled) { diagnostics.reset(enabled, millis()); }
  const ConnectorDiagnostics& diagnosticSnapshot() const { return diagnostics; }
  const ConnectorConfig& configuration() const { return target; }
  const ConnectorField& mapping(uint8_t index) const { return mappings[index]; }
  uint8_t mappingCount() const { return fieldCount; }
  const char* diagnosticStatus(bool networkAvailable) const {
    if (!diagnostics.enabled) return "off";
    if (!target.enabled) return "disabled";
    if (!valid) return "invalid_configuration";
    if (!networkAvailable) return "no_network";
    if (diagnostics.latestFailed) return "error";
    if (diagnostics.hasSuccess && uint32_t(millis() - diagnostics.lastSuccessMs) >
        (uint32_t(target.interval) * 2000UL > 20000UL ? uint32_t(target.interval) * 2000UL : 20000UL)) return "stale";
    if (diagnostics.hasSuccess) return "ok";
    return pending ? "reading" : "waiting";
  }
  void sampleAccepted() { diagnostics.accepted(millis()); }
  void sampleRejected(uint8_t field) { recordFailure(0x103, field, false); }
 private:
  void recordFailure(uint16_t error, uint8_t field, bool exponent) {
    if (!diagnostics.enabled) return;
    ConnectorDiagnosticEntry entry;
    entry.atMs = millis(); entry.field = field; entry.exponent = exponent;
    entry.reg = exponent ? mappings[field].sf : mappings[field].reg;
    entry.error = error;
    diagnostics.record(entry);
  }
  void receive(ModbusMessage msg, uint32_t t, uint16_t error = 0) {
    portENTER_CRITICAL(&mux);
    if (t == activeToken) {
      success = msg.size() >= size_t(3 + expectedWords * 2) &&
                msg[0] == expectedUnit && msg[1] == READ_HOLD_REGISTER && msg[2] == expectedWords * 2;
      responseError = success ? 0 : (error ? error : 0x100);
      if (success) {
        words[0] = (uint16_t(msg[3]) << 8) | msg[4];
        words[1] = expectedWords == 2 ? (uint16_t(msg[5]) << 8) | msg[6] : 0;
      }
      received = true;
    }
    portEXIT_CRITICAL(&mux);
  }
  ConnectorDiagnostics diagnostics;
  uint16_t responseError = 0;
  WiFiClient socket;
  ModbusClientTCP client;
  ConnectorConfig target;
  ConnectorField mappings[3];
  double values[3] = {};
  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
  uint32_t token = 0, activeToken = 0, lastCycleMs = 0;
  uint16_t words[2] = {};
  uint8_t expectedUnit = 0, expectedWords = 0, fieldIndex = 0, fieldCount = 0;
  bool received = false, success = false;
  bool started = false, valid = false, pending = false, ready = false;
  bool readingScale = false, useSunSpecSentinels = false;
};
