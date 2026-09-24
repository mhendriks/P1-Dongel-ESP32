#pragma once

// Stable wire contract for the modbus_slave_sink experiment.  This header is
// deliberately independent of Arduino, DSMR and ESP-NOW so host tests can
// verify both firmware copies against the same fixed vectors.
#include <stddef.h>
#include <stdint.h>

namespace smartstuff::modbus_sink {

static constexpr uint8_t kMessageType = 0xA1;
static constexpr uint8_t kLinkStatusMessageType = 0xA2;
static constexpr uint8_t kProtocolVersion = 1;
static constexpr uint8_t kSatelliteRole = 4;  // modbus_slave_sink

enum Field : uint8_t {
  import_t1_wh, import_t2_wh, export_t1_wh, export_t2_wh,
  import_total_wh, export_total_wh,
  import_power_w, export_power_w,
  import_l1_w, import_l2_w, import_l3_w,
  export_l1_w, export_l2_w, export_l3_w,
  voltage_l1_mv, voltage_l2_mv, voltage_l3_mv,
  current_l1_ma, current_l2_ma, current_l3_ma,
  tariff,
  field_count
};

struct Snapshot {
  uint32_t packet_sequence = 0;
  uint32_t sample_sequence = 0;
  uint32_t meter_epoch_utc = 0;
  uint32_t present = 0;
  uint32_t values[field_count] = {};
};

// type, version, role, packet bytes, then the four 32-bit header members.
static constexpr size_t kHeaderBytes = 20;
static constexpr size_t kPacketBytes = kHeaderBytes + field_count * 4;
static_assert(kPacketBytes < 250, "single ESP-NOW legacy frame required");

static inline void putU32LE(uint8_t* out, uint32_t value) {
  out[0] = (uint8_t)value;
  out[1] = (uint8_t)(value >> 8);
  out[2] = (uint8_t)(value >> 16);
  out[3] = (uint8_t)(value >> 24);
}

static inline uint32_t getU32LE(const uint8_t* in) {
  return (uint32_t)in[0] | ((uint32_t)in[1] << 8) |
         ((uint32_t)in[2] << 16) | ((uint32_t)in[3] << 24);
}

static inline void set(Snapshot& snapshot, Field field, uint32_t value) {
  snapshot.present |= (uint32_t)1 << (uint8_t)field;
  snapshot.values[(uint8_t)field] = value;
}

static inline bool has(const Snapshot& snapshot, Field field) {
  return (snapshot.present & ((uint32_t)1 << (uint8_t)field)) != 0;
}

static inline size_t encode(const Snapshot& snapshot, uint8_t* out, size_t capacity) {
  if (!out || capacity < kPacketBytes) return 0;
  out[0] = kMessageType;
  out[1] = kProtocolVersion;
  out[2] = kSatelliteRole;
  out[3] = (uint8_t)kPacketBytes;
  putU32LE(out + 4, snapshot.packet_sequence);
  putU32LE(out + 8, snapshot.sample_sequence);
  putU32LE(out + 12, snapshot.meter_epoch_utc);
  putU32LE(out + 16, snapshot.present);
  for (uint8_t i = 0; i < field_count; ++i) putU32LE(out + kHeaderBytes + i * 4, snapshot.values[i]);
  return kPacketBytes;
}

static inline bool decode(const uint8_t* in, size_t length, Snapshot& out) {
  if (!in || length != kPacketBytes || in[0] != kMessageType ||
      in[1] != kProtocolVersion || in[2] != kSatelliteRole || in[3] != kPacketBytes) return false;
  out.packet_sequence = getU32LE(in + 4);
  out.sample_sequence = getU32LE(in + 8);
  out.meter_epoch_utc = getU32LE(in + 12);
  out.present = getU32LE(in + 16);
  for (uint8_t i = 0; i < field_count; ++i) out.values[i] = getU32LE(in + kHeaderBytes + i * 4);
  return true;
}

// Satellite-to-gateway link diagnostics. This is a link-layer packet, not a
// measurement payload; it lets the gateway display the receiver's RSSI and
// sequence observations without waiting for a Modbus request.
struct LinkStatus {
  int8_t rssi_dbm = -127;
  uint32_t accepted = 0;
  uint32_t missed = 0;
  uint32_t duplicates = 0;
  uint32_t old = 0;
};
static constexpr size_t kLinkStatusBytes = 20;

static inline size_t encodeLinkStatus(const LinkStatus& status, uint8_t* out, size_t capacity) {
  if (!out || capacity < kLinkStatusBytes) return 0;
  out[0] = kLinkStatusMessageType;
  out[1] = kProtocolVersion;
  out[2] = (uint8_t)status.rssi_dbm;
  out[3] = kSatelliteRole;
  putU32LE(out + 4, status.accepted);
  putU32LE(out + 8, status.missed);
  putU32LE(out + 12, status.duplicates);
  putU32LE(out + 16, status.old);
  return kLinkStatusBytes;
}

static inline bool decodeLinkStatus(const uint8_t* in, size_t length, LinkStatus& out) {
  if (!in || length != kLinkStatusBytes || in[0] != kLinkStatusMessageType ||
      in[1] != kProtocolVersion || in[3] != kSatelliteRole) return false;
  out.rssi_dbm = (int8_t)in[2];
  out.accepted = getU32LE(in + 4);
  out.missed = getU32LE(in + 8);
  out.duplicates = getU32LE(in + 12);
  out.old = getU32LE(in + 16);
  return true;
}

}  // namespace smartstuff::modbus_sink
