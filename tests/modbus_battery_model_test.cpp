#include <assert.h>
#include <math.h>
#include "../EnergyModel.h"
#include "../ModbusBatteryDecoding.h"

int main() {
  // Signed active power: positive means charging, negative means discharging.
  assert(decodeModbusS16(0x04D2) == 1234);
  assert(decodeModbusS16(0xFB2E) == -1234);
  assert(scaleModbusValue(decodeModbusS16(0xFB2E), 1.0f) == -1234.0f);
  assert(fabsf(scaleModbusValue(742, 0.1f) - 74.2f) < 0.001f);

  // Multiword fields explicitly retain their profile-declared word order.
  assert(decodeModbusU32(0x1234, 0x5678, false) == 0x12345678UL);
  assert(decodeModbusU32(0x1234, 0x5678, true) == 0x56781234UL);

  assert(isModbusInvalid(0xFFFF, 0xFFFF, true));
  assert(!isModbusInvalid(0, 0xFFFF, true));

  EnergyMeasurement measured {42.0f, "W", 1000, 842, true};
  assert(batteryMeasurementQuality(measured, 1999, 1000) == EnergyMeasurementQuality::GOOD);
  assert(batteryMeasurementQuality(measured, 2001, 1000) == EnergyMeasurementQuality::STALE);
  measured.available = false;
  assert(batteryMeasurementQuality(measured, 2001, 1000) == EnergyMeasurementQuality::UNAVAILABLE);
}
