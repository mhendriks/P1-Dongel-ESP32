/* Derived meter values. Source telegram values are never modified here. */

static MeterCurrent calculatedCurrents[3];

static bool phaseCurrentCanBeCalculated(bool voltagePresent, float voltage,
                                        bool deliveredPresent, uint32_t delivered,
                                        bool returnedPresent, uint32_t returned,
                                        MeterCurrent& current) {
  if (!voltagePresent || voltage <= 0.0f) return false;

  // A zero phase power can be a meter limitation, rather than zero current.
  // Preserve the telegram value as fallback in that case.
  if ((!deliveredPresent || delivered == 0) && (!returnedPresent || returned == 0)) return false;

  current.present = true;
  current.calculated = true;
  current.milliAmps = (uint32_t)(((uint64_t)delivered + returned) * 1000.0f / voltage);
  return true;
}

void UpdateCalculatedCurrents() {
  for (MeterCurrent& current : calculatedCurrents) current = MeterCurrent();
  if (!try_calc_i) return;

  phaseCurrentCanBeCalculated(DSMRdata.voltage_l1_present, DSMRdata.voltage_l1.val(),
    DSMRdata.power_delivered_l1_present, DSMRdata.power_delivered_l1.int_val(),
    DSMRdata.power_returned_l1_present, DSMRdata.power_returned_l1.int_val(), calculatedCurrents[0]);
  phaseCurrentCanBeCalculated(DSMRdata.voltage_l2_present, DSMRdata.voltage_l2.val(),
    DSMRdata.power_delivered_l2_present, DSMRdata.power_delivered_l2.int_val(),
    DSMRdata.power_returned_l2_present, DSMRdata.power_returned_l2.int_val(), calculatedCurrents[1]);
  phaseCurrentCanBeCalculated(DSMRdata.voltage_l3_present, DSMRdata.voltage_l3.val(),
    DSMRdata.power_delivered_l3_present, DSMRdata.power_delivered_l3.int_val(),
    DSMRdata.power_returned_l3_present, DSMRdata.power_returned_l3.int_val(), calculatedCurrents[2]);
}

MeterCurrent GetMeterCurrent(uint8_t phase) {
  if (phase < 1 || phase > 3) return MeterCurrent();
  const uint8_t index = phase - 1;
  if (try_calc_i && calculatedCurrents[index].present) return calculatedCurrents[index];

  MeterCurrent current;
  switch (phase) {
    case 1: current.present = DSMRdata.current_l1_present; current.milliAmps = current.present ? DSMRdata.current_l1.int_val() : 0; break;
    case 2: current.present = DSMRdata.current_l2_present; current.milliAmps = current.present ? DSMRdata.current_l2.int_val() : 0; break;
    case 3: current.present = DSMRdata.current_l3_present; current.milliAmps = current.present ? DSMRdata.current_l3.int_val() : 0; break;
  }
  return current;
}
