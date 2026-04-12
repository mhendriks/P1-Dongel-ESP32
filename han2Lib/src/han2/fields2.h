#ifndef HAN2_INCLUDE_FIELDS_H
#define HAN2_INCLUDE_FIELDS_H

#include <Arduino.h>

namespace han {
namespace fields {

template<typename T>
struct NumericValue {
  T _value = 0;
  bool _present = false;

  bool present() const { return _present; }
  T val() const { return _value; }
  void clear() { _value = 0; _present = false; }
  void set(T value) { _value = value; _present = true; }
  operator T() const { return _value; }
};

struct StringValue {
  String _value;
  bool _present = false;

  bool present() const { return _present; }
  const String& val() const { return _value; }
  void clear() { _value = ""; _present = false; }
  void set(const String& value) { _value = value; _present = true; }
  operator const String&() const { return _value; }
};

using FixedValue = NumericValue<float>;
using UIntValue = NumericValue<uint32_t>;
using IntValue = NumericValue<int32_t>;

}  // namespace fields

enum class HanListType : uint8_t {
  Unknown = 0,
  List1 = 1,
  List2 = 2,
};

struct HanData {
  fields::StringValue timestamp;
  fields::StringValue identification;
  fields::StringValue equipment_id;

  HanListType list_type = HanListType::Unknown;

  fields::FixedValue power_delivered_kw;
  fields::FixedValue power_returned_kw;
  fields::FixedValue reactive_power_delivered_kvar;
  fields::FixedValue reactive_power_returned_kvar;

  fields::FixedValue voltage_l1_v;
  fields::FixedValue voltage_l2_v;
  fields::FixedValue voltage_l3_v;
  fields::FixedValue current_l1_a;
  fields::FixedValue current_l2_a;
  fields::FixedValue current_l3_a;

  fields::FixedValue energy_delivered_total_kwh;
  fields::FixedValue energy_returned_total_kwh;
  fields::FixedValue reactive_energy_delivered_total_kvarh;
  fields::FixedValue reactive_energy_returned_total_kvarh;

  void clear() {
    timestamp.clear();
    identification.clear();
    equipment_id.clear();
    list_type = HanListType::Unknown;
    power_delivered_kw.clear();
    power_returned_kw.clear();
    reactive_power_delivered_kvar.clear();
    reactive_power_returned_kvar.clear();
    voltage_l1_v.clear();
    voltage_l2_v.clear();
    voltage_l3_v.clear();
    current_l1_a.clear();
    current_l2_a.clear();
    current_l3_a.clear();
    energy_delivered_total_kwh.clear();
    energy_returned_total_kwh.clear();
    reactive_energy_delivered_total_kvarh.clear();
    reactive_energy_returned_total_kvarh.clear();
  }
};

struct HanState {
  HanData last_list1;
  HanData last_list2;
  HanData effective;
  bool has_list1 = false;
  bool has_list2 = false;

  void clear() {
    last_list1.clear();
    last_list2.clear();
    effective.clear();
    has_list1 = false;
    has_list2 = false;
  }

  void update(const HanData& data) {
    if (data.list_type == HanListType::List2) {
      last_list2 = data;
      has_list2 = true;
    } else if (data.list_type == HanListType::List1) {
      last_list1 = data;
      has_list1 = true;
    }

    effective = data;

    if (has_list2) {
      if (!effective.timestamp.present() && last_list2.timestamp.present()) effective.timestamp = last_list2.timestamp;
      if (!effective.identification.present() && last_list2.identification.present()) effective.identification = last_list2.identification;
      if (!effective.equipment_id.present() && last_list2.equipment_id.present()) effective.equipment_id = last_list2.equipment_id;
      if (!effective.energy_delivered_total_kwh.present() && last_list2.energy_delivered_total_kwh.present()) effective.energy_delivered_total_kwh = last_list2.energy_delivered_total_kwh;
      if (!effective.energy_returned_total_kwh.present() && last_list2.energy_returned_total_kwh.present()) effective.energy_returned_total_kwh = last_list2.energy_returned_total_kwh;
      if (!effective.reactive_energy_delivered_total_kvarh.present() && last_list2.reactive_energy_delivered_total_kvarh.present()) effective.reactive_energy_delivered_total_kvarh = last_list2.reactive_energy_delivered_total_kvarh;
      if (!effective.reactive_energy_returned_total_kvarh.present() && last_list2.reactive_energy_returned_total_kvarh.present()) effective.reactive_energy_returned_total_kvarh = last_list2.reactive_energy_returned_total_kvarh;
    }
  }
};

}  // namespace han

#endif
