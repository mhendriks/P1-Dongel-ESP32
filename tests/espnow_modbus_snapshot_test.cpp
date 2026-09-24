#include "../espnow_modbus_snapshot.h"
#include <stdio.h>

using namespace smartstuff::modbus_sink;

int main() {
  Snapshot source;
  source.packet_sequence = 0x01020304u;
  source.sample_sequence = 42;
  source.meter_epoch_utc = 1700000000u;
  set(source, import_t1_wh, 123456u);
  set(source, export_power_w, 789u);
  set(source, voltage_l1_mv, 230100u);

  uint8_t bytes[kPacketBytes] = {};
  if (encode(source, bytes, sizeof(bytes)) != kPacketBytes) return 1;
  if (bytes[0] != kMessageType || bytes[1] != kProtocolVersion || bytes[2] != kSatelliteRole ||
      bytes[4] != 0x04 || bytes[5] != 0x03 || bytes[6] != 0x02 || bytes[7] != 0x01) return 2;

  Snapshot decoded;
  if (!decode(bytes, sizeof(bytes), decoded)) return 3;
  if (decoded.packet_sequence != source.packet_sequence || decoded.sample_sequence != source.sample_sequence ||
      decoded.meter_epoch_utc != source.meter_epoch_utc || !has(decoded, import_t1_wh) ||
      decoded.values[import_t1_wh] != 123456u || has(decoded, current_l3_ma)) return 4;

  bytes[3]--;
  if (decode(bytes, sizeof(bytes), decoded)) return 5;

  LinkStatus status;
  status.rssi_dbm = -46;
  status.accepted = 3;
  status.missed = 2;
  uint8_t statusBytes[kLinkStatusBytes] = {};
  if (encodeLinkStatus(status, statusBytes, sizeof(statusBytes)) != kLinkStatusBytes) return 6;
  LinkStatus decodedStatus;
  if (!decodeLinkStatus(statusBytes, sizeof(statusBytes), decodedStatus) ||
      decodedStatus.rssi_dbm != -46 || decodedStatus.accepted != 3 || decodedStatus.missed != 2) return 7;
  return 0;
}
