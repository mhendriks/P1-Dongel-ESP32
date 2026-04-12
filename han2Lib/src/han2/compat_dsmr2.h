#ifndef HAN2_INCLUDE_COMPAT_DSMR2_H
#define HAN2_INCLUDE_COMPAT_DSMR2_H

#include "fields2.h"

namespace han {
namespace compat {

template<typename TDsmrData>
inline void setFixedValue(decltype(TDsmrData::power_delivered)& target, bool& present, float value) {
  target._value = (int32_t)lroundf(value * 1000.0f);
  present = true;
}

template<typename TDsmrData>
inline void mapToDsmr(TDsmrData& out, const HanState& state, const String& fallbackTimestamp, const String& fallbackId = "HAN reader") {
  const HanData& effective = state.effective;

  out = {};

  if (effective.timestamp.present()) {
    out.timestamp = effective.timestamp.val();
    out.timestamp_present = true;
  } else if (fallbackTimestamp.length() > 0) {
    out.timestamp = fallbackTimestamp;
    out.timestamp_present = true;
  }

  out.identification = effective.identification.present() ? effective.identification.val() : fallbackId;
  out.identification_present = out.identification.length() > 0;

  out.p1_version = "50";
  out.p1_version_present = true;

  if (effective.equipment_id.present()) {
    out.equipment_id = effective.equipment_id.val();
    out.equipment_id_present = effective.equipment_id.val().length() > 0;
  }

  if (effective.energy_delivered_total_kwh.present()) {
    setFixedValue<TDsmrData>(out.energy_delivered_tariff1, out.energy_delivered_tariff1_present, effective.energy_delivered_total_kwh.val());
    setFixedValue<TDsmrData>(out.energy_delivered_tariff2, out.energy_delivered_tariff2_present, 0.0f);
  }
  if (effective.energy_returned_total_kwh.present()) {
    setFixedValue<TDsmrData>(out.energy_returned_tariff1, out.energy_returned_tariff1_present, effective.energy_returned_total_kwh.val());
    setFixedValue<TDsmrData>(out.energy_returned_tariff2, out.energy_returned_tariff2_present, 0.0f);
  }

  if (effective.power_delivered_kw.present()) setFixedValue<TDsmrData>(out.power_delivered, out.power_delivered_present, effective.power_delivered_kw.val());
  if (effective.power_returned_kw.present()) setFixedValue<TDsmrData>(out.power_returned, out.power_returned_present, effective.power_returned_kw.val());

  if (effective.voltage_l1_v.present()) setFixedValue<TDsmrData>(out.voltage_l1, out.voltage_l1_present, effective.voltage_l1_v.val());
  if (effective.voltage_l2_v.present()) setFixedValue<TDsmrData>(out.voltage_l2, out.voltage_l2_present, effective.voltage_l2_v.val());
  if (effective.voltage_l3_v.present()) setFixedValue<TDsmrData>(out.voltage_l3, out.voltage_l3_present, effective.voltage_l3_v.val());
  if (effective.current_l1_a.present()) setFixedValue<TDsmrData>(out.current_l1, out.current_l1_present, effective.current_l1_a.val());
  if (effective.current_l2_a.present()) setFixedValue<TDsmrData>(out.current_l2, out.current_l2_present, effective.current_l2_a.val());
  if (effective.current_l3_a.present()) setFixedValue<TDsmrData>(out.current_l3, out.current_l3_present, effective.current_l3_a.val());
}

}  // namespace compat
}  // namespace han

#endif
