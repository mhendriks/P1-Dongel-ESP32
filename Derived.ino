/*
***************************************************************************
**  Program  : Always-needed derived meter data, part of DSMRloggerAPI
**
**  Copyright (c) 2026 Smartstuff
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/

static uint32_t derivedAbsPowerSumW(uint32_t deliveredW, uint32_t returnedW) {
  return deliveredW + returnedW;
}

static bool derivedPhasePowerPresent(bool deliveredPresent, uint32_t deliveredW, bool returnedPresent, uint32_t returnedW) {
  return (deliveredPresent && deliveredW) || (returnedPresent && returnedW);
}

static bool derivedCurrentFromPower(uint32_t powerW, float voltageV, uint32_t& outmA) {
  if (voltageV <= 0.0f) return false;
  outmA = (uint32_t)((powerW / voltageV) * 1000.0f);
  return true;
}

static MeterCurrent sourceMeterCurrent(uint8_t phase) {
  MeterCurrent current;

  switch (phase) {
    case 1:
      current.present = DSMRdata.current_l1_present;
      current.mA = current.present ? DSMRdata.current_l1.int_val() : 0;
      break;
    case 2:
      current.present = DSMRdata.current_l2_present;
      current.mA = current.present ? DSMRdata.current_l2.int_val() : 0;
      break;
    case 3:
      current.present = DSMRdata.current_l3_present;
      current.mA = current.present ? DSMRdata.current_l3.int_val() : 0;
      break;
  }

  return current;
}

MeterCurrent GetMeterCurrent(uint8_t phase) {
  MeterCurrent current = sourceMeterCurrent(phase);
  if (!try_calc_i) return current;

  switch (phase) {
    case 1:
      if (meterState.derived.calculatedCurrentL1Present) {
        current.present = true;
        current.mA = meterState.derived.calculatedCurrentL1mA;
        current.calculated = true;
      }
      break;
    case 2:
      if (meterState.derived.calculatedCurrentL2Present) {
        current.present = true;
        current.mA = meterState.derived.calculatedCurrentL2mA;
        current.calculated = true;
      }
      break;
    case 3:
      if (meterState.derived.calculatedCurrentL3Present) {
        current.present = true;
        current.mA = meterState.derived.calculatedCurrentL3mA;
        current.calculated = true;
      }
      break;
    default:
      current.present = false;
      current.mA = 0;
      break;
  }

  return current;
}

MeterPower GetMeterPhasePower(uint8_t phase) {
  MeterPower power;
  bool deliveredPresent = false;
  bool returnedPresent = false;
  uint32_t deliveredW = 0;
  uint32_t returnedW = 0;

  switch (phase) {
    case 1:
      deliveredPresent = DSMRdata.power_delivered_l1_present;
      returnedPresent = DSMRdata.power_returned_l1_present;
      deliveredW = DSMRdata.power_delivered_l1.int_val();
      returnedW = DSMRdata.power_returned_l1.int_val();
      break;
    case 2:
      deliveredPresent = DSMRdata.power_delivered_l2_present;
      returnedPresent = DSMRdata.power_returned_l2_present;
      deliveredW = DSMRdata.power_delivered_l2.int_val();
      returnedW = DSMRdata.power_returned_l2.int_val();
      break;
    case 3:
      deliveredPresent = DSMRdata.power_delivered_l3_present;
      returnedPresent = DSMRdata.power_returned_l3_present;
      deliveredW = DSMRdata.power_delivered_l3.int_val();
      returnedW = DSMRdata.power_returned_l3.int_val();
      break;
    default:
      return power;
  }

  power.present = deliveredPresent || returnedPresent;
  power.W = (int32_t)(deliveredPresent ? deliveredW : 0) - (int32_t)(returnedPresent ? returnedW : 0);
  return power;
}

MeterVoltage GetMeterVoltage(uint8_t phase) {
  MeterVoltage voltage;

  switch (phase) {
    case 1:
      voltage.present = DSMRdata.voltage_l1_present;
      voltage.mV = voltage.present ? DSMRdata.voltage_l1.int_val() : 0;
      break;
    case 2:
      voltage.present = DSMRdata.voltage_l2_present;
      voltage.mV = voltage.present ? DSMRdata.voltage_l2.int_val() : 0;
      break;
    case 3:
      voltage.present = DSMRdata.voltage_l3_present;
      voltage.mV = voltage.present ? DSMRdata.voltage_l3.int_val() : 0;
      break;
  }

  return voltage;
}

void UpdateMeterDerived() {
  meterState.derived = MeterDerived();
  meterState.capabilities.energyTotalsOnly =
    (DSMRdata.energy_delivered_total_present || DSMRdata.energy_returned_total_present) &&
    !DSMRdata.energy_delivered_tariff1_present &&
    !DSMRdata.energy_delivered_tariff2_present &&
    !DSMRdata.energy_returned_tariff1_present &&
    !DSMRdata.energy_returned_tariff2_present;

  if (DSMRdata.energy_delivered_total_present) {
    meterState.derived.deliveredTotalWh = DSMRdata.energy_delivered_total.int_val();
    meterState.derived.deliveredTotalPresent = true;
  } else if (DSMRdata.energy_delivered_tariff1_present || DSMRdata.energy_delivered_tariff2_present) {
    meterState.derived.deliveredTotalWh =
      (DSMRdata.energy_delivered_tariff1_present ? DSMRdata.energy_delivered_tariff1.int_val() : 0) +
      (DSMRdata.energy_delivered_tariff2_present ? DSMRdata.energy_delivered_tariff2.int_val() : 0);
    meterState.derived.deliveredTotalPresent = true;
  }

  if (DSMRdata.energy_returned_total_present) {
    meterState.derived.returnedTotalWh = DSMRdata.energy_returned_total.int_val();
    meterState.derived.returnedTotalPresent = true;
  } else if (DSMRdata.energy_returned_tariff1_present || DSMRdata.energy_returned_tariff2_present) {
    meterState.derived.returnedTotalWh =
      (DSMRdata.energy_returned_tariff1_present ? DSMRdata.energy_returned_tariff1.int_val() : 0) +
      (DSMRdata.energy_returned_tariff2_present ? DSMRdata.energy_returned_tariff2.int_val() : 0);
    meterState.derived.returnedTotalPresent = true;
  }

  if (DSMRdata.power_delivered_present || DSMRdata.power_returned_present) {
    meterState.derived.netPowerW =
      (int32_t)(DSMRdata.power_delivered_present ? DSMRdata.power_delivered.int_val() : 0) -
      (int32_t)(DSMRdata.power_returned_present ? DSMRdata.power_returned.int_val() : 0);
    meterState.derived.netPowerPresent = true;
  }

  if (DSMRdata.voltage_l1_present &&
             derivedPhasePowerPresent(
               DSMRdata.power_delivered_l1_present, DSMRdata.power_delivered_l1.int_val(),
               DSMRdata.power_returned_l1_present, DSMRdata.power_returned_l1.int_val())) {
    meterState.derived.calculatedCurrentL1Present = derivedCurrentFromPower(
      derivedAbsPowerSumW(DSMRdata.power_delivered_l1.int_val(), DSMRdata.power_returned_l1.int_val()),
      DSMRdata.voltage_l1.val(),
      meterState.derived.calculatedCurrentL1mA);
  }

  if (DSMRdata.voltage_l2_present &&
             derivedPhasePowerPresent(
               DSMRdata.power_delivered_l2_present, DSMRdata.power_delivered_l2.int_val(),
               DSMRdata.power_returned_l2_present, DSMRdata.power_returned_l2.int_val())) {
    meterState.derived.calculatedCurrentL2Present = derivedCurrentFromPower(
      derivedAbsPowerSumW(DSMRdata.power_delivered_l2.int_val(), DSMRdata.power_returned_l2.int_val()),
      DSMRdata.voltage_l2.val(),
      meterState.derived.calculatedCurrentL2mA);
  }

  if (DSMRdata.voltage_l3_present &&
             derivedPhasePowerPresent(
               DSMRdata.power_delivered_l3_present, DSMRdata.power_delivered_l3.int_val(),
               DSMRdata.power_returned_l3_present, DSMRdata.power_returned_l3.int_val())) {
    meterState.derived.calculatedCurrentL3Present = derivedCurrentFromPower(
      derivedAbsPowerSumW(DSMRdata.power_delivered_l3.int_val(), DSMRdata.power_returned_l3.int_val()),
      DSMRdata.voltage_l3.val(),
      meterState.derived.calculatedCurrentL3mA);
  }
}
