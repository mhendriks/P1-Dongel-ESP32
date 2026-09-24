// Master-side snapshot builder. It only translates normalized DSMR data to the
// transport-neutral contract; the ESP-NOW link never interprets measurements.
#include "espnow_modbus_snapshot.h"

static uint32_t modbusSinkPacketSequence = 0;
static uint32_t modbusSinkSampleSequence = 0;

static inline void setModbusSinkValue(smartstuff::modbus_sink::Snapshot& snapshot,
                                      smartstuff::modbus_sink::Field field,
                                      bool present, uint32_t value) {
  if (present) smartstuff::modbus_sink::set(snapshot, field, value);
}

void buildModbusSinkSnapshot(smartstuff::modbus_sink::Snapshot& snapshot) {
  using namespace smartstuff::modbus_sink;
  snapshot = Snapshot();
  snapshot.packet_sequence = ++modbusSinkPacketSequence;
  snapshot.sample_sequence = ++modbusSinkSampleSequence;
  snapshot.meter_epoch_utc = (uint32_t)newT;
  setModbusSinkValue(snapshot, import_t1_wh, DSMRdata.energy_delivered_tariff1_present, DSMRdata.energy_delivered_tariff1.int_val());
  setModbusSinkValue(snapshot, import_t2_wh, DSMRdata.energy_delivered_tariff2_present, DSMRdata.energy_delivered_tariff2.int_val());
  setModbusSinkValue(snapshot, export_t1_wh, DSMRdata.energy_returned_tariff1_present, DSMRdata.energy_returned_tariff1.int_val());
  setModbusSinkValue(snapshot, export_t2_wh, DSMRdata.energy_returned_tariff2_present, DSMRdata.energy_returned_tariff2.int_val());
  setModbusSinkValue(snapshot, import_total_wh, DSMRdata.energy_delivered_total_present, DSMRdata.energy_delivered_total.int_val());
  setModbusSinkValue(snapshot, export_total_wh, DSMRdata.energy_returned_total_present, DSMRdata.energy_returned_total.int_val());
  setModbusSinkValue(snapshot, import_power_w, DSMRdata.power_delivered_present, DSMRdata.power_delivered.int_val());
  setModbusSinkValue(snapshot, export_power_w, DSMRdata.power_returned_present, DSMRdata.power_returned.int_val());
  setModbusSinkValue(snapshot, import_l1_w, DSMRdata.power_delivered_l1_present, DSMRdata.power_delivered_l1.int_val());
  setModbusSinkValue(snapshot, import_l2_w, DSMRdata.power_delivered_l2_present, DSMRdata.power_delivered_l2.int_val());
  setModbusSinkValue(snapshot, import_l3_w, DSMRdata.power_delivered_l3_present, DSMRdata.power_delivered_l3.int_val());
  setModbusSinkValue(snapshot, export_l1_w, DSMRdata.power_returned_l1_present, DSMRdata.power_returned_l1.int_val());
  setModbusSinkValue(snapshot, export_l2_w, DSMRdata.power_returned_l2_present, DSMRdata.power_returned_l2.int_val());
  setModbusSinkValue(snapshot, export_l3_w, DSMRdata.power_returned_l3_present, DSMRdata.power_returned_l3.int_val());
  setModbusSinkValue(snapshot, voltage_l1_mv, DSMRdata.voltage_l1_present, DSMRdata.voltage_l1.int_val());
  setModbusSinkValue(snapshot, voltage_l2_mv, DSMRdata.voltage_l2_present, DSMRdata.voltage_l2.int_val());
  setModbusSinkValue(snapshot, voltage_l3_mv, DSMRdata.voltage_l3_present, DSMRdata.voltage_l3.int_val());
  setModbusSinkValue(snapshot, current_l1_ma, DSMRdata.current_l1_present, DSMRdata.current_l1.int_val());
  setModbusSinkValue(snapshot, current_l2_ma, DSMRdata.current_l2_present, DSMRdata.current_l2.int_val());
  setModbusSinkValue(snapshot, current_l3_ma, DSMRdata.current_l3_present, DSMRdata.current_l3.int_val());
  setModbusSinkValue(snapshot, tariff, DSMRdata.electricity_tariff_present,
                     (uint32_t)DSMRdata.electricity_tariff.toInt());
}
