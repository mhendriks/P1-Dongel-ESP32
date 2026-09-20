#include <dsmr3.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>

using TestData = ParsedData<
  energy_delivered_total, energy_returned_total,
  power_delivered, power_returned,
  power_delivered_l1, power_delivered_l2, power_delivered_l3,
  power_returned_l1, power_returned_l2, power_returned_l3,
  voltage_l1, voltage_l2, voltage_l3,
  current_l1, current_l2, current_l3
>;

static int failures = 0;

#define CHECK_EQ(actual, expected) do { \
  const uint32_t actualValue = (uint32_t)(actual); \
  const uint32_t expectedValue = (uint32_t)(expected); \
  if (actualValue != expectedValue) { \
    fprintf(stderr, "%s:%d expected %u, got %u\n", __FILE__, __LINE__, \
            (unsigned)expectedValue, (unsigned)actualValue); \
    ++failures; \
  } \
} while (0)

#define CHECK_NEAR(actual, expected, tolerance) do { \
  const float actualValue = (float)(actual); \
  const float expectedValue = (float)(expected); \
  if (fabsf(actualValue - expectedValue) > (tolerance)) { \
    fprintf(stderr, "%s:%d expected %.3f, got %.3f\n", __FILE__, __LINE__, \
            (double)expectedValue, (double)actualValue); \
    ++failures; \
  } \
} while (0)

int main() {
  const char telegram[] =
      "/TEST\r\n"
      "1-0:1.7.0(0000.851*kW)\r\n"
      "1-0:2.7.0(0000.000*kW)\r\n"
      "1-0:21.7.0(0000.354*kW)\r\n"
      "1-0:41.7.0(0000.166*kW)\r\n"
      "1-0:61.7.0(0000.331*kW)\r\n"
      "1-0:22.7.0(0000.000*kW)\r\n"
      "1-0:42.7.0(0000.000*kW)\r\n"
      "1-0:62.7.0(0000.000*kW)\r\n"
      "1-0:32.7.0(228.8*V)\r\n"
      "1-0:52.7.0(232.7*V)\r\n"
      "1-0:72.7.0(229.2*V)\r\n"
      "1-0:31.7.0(00.93*A)\r\n"
      "1-0:51.7.0(02.69*A)\r\n"
      "1-0:71.7.0(05.07*A)\r\n"
      "!";

  TestData data = {};
  ParseResult<void> result = P1Parser::parse(&data, telegram, sizeof(telegram) - 1);
  if (result.err) {
    fprintf(stderr, "telegram parse failed\n");
    return 1;
  }

  // FixedValue::int_val() is exactly the UINT32 mapping's scale-1000
  // representation: kW * 1000 = W.
  CHECK_EQ(data.power_delivered.int_val(), 851);
  CHECK_EQ(data.power_returned.int_val(), 0);
  CHECK_EQ(data.power_delivered_l1.int_val(), 354);
  CHECK_EQ(data.power_delivered_l2.int_val(), 166);
  CHECK_EQ(data.power_delivered_l3.int_val(), 331);
  CHECK_EQ(data.power_returned_l1.int_val(), 0);
  CHECK_EQ(data.power_returned_l2.int_val(), 0);
  CHECK_EQ(data.power_returned_l3.int_val(), 0);

  // SDM630 assumes PF=1 where the P1 telegram has no apparent/reactive power.
  CHECK_NEAR(fabsf(data.power_delivered_l1.val() - data.power_returned_l1.val()) * 1000.0f, 354.0f, 0.01f);  // reg 18
  CHECK_NEAR(fabsf(data.power_delivered_l2.val() - data.power_returned_l2.val()) * 1000.0f, 166.0f, 0.01f);  // reg 20
  CHECK_NEAR(fabsf(data.power_delivered_l3.val() - data.power_returned_l3.val()) * 1000.0f, 331.0f, 0.01f);  // reg 22
  CHECK_NEAR(fabsf(data.power_delivered.val() - data.power_returned.val()) * 1000.0f, 851.0f, 0.01f);        // reg 56

  const float totalCurrent = data.current_l1.val() + data.current_l2.val() + data.current_l3.val();
  CHECK_NEAR(totalCurrent, 8.690f, 0.002f);  // reg 48: same sources as registers 6, 8 and 10

  // Expected encoded 32-bit payloads for the STD UINT32 register table.
  // eModbus emits uint32 values MSW first, so each value below becomes
  // {0x0000, value} for this telegram.
  const uint32_t reg14TotalImport = data.power_delivered.int_val();
  const uint32_t reg16TotalExport = data.power_returned.int_val();
  const int32_t reg38NetL1 = (int32_t)data.power_delivered_l1.int_val() -
                             (int32_t)data.power_returned_l1.int_val();
  const int32_t reg40NetL2 = (int32_t)data.power_delivered_l2.int_val() -
                             (int32_t)data.power_returned_l2.int_val();
  const int32_t reg42NetL3 = (int32_t)data.power_delivered_l3.int_val() -
                             (int32_t)data.power_returned_l3.int_val();
  const int32_t reg48NetTotal = (int32_t)data.power_delivered.int_val() -
                                (int32_t)data.power_returned.int_val();

  CHECK_EQ(reg14TotalImport, 851);
  CHECK_EQ(reg16TotalExport, 0);
  CHECK_EQ(reg38NetL1, 354);
  CHECK_EQ(reg40NetL2, 166);
  CHECK_EQ(reg42NetL3, 331);
  CHECK_EQ(reg48NetTotal, 851);
  CHECK_EQ(data.power_delivered_l1.int_val(), 354);  // register 50
  CHECK_EQ(data.power_delivered_l2.int_val(), 166);  // register 52
  CHECK_EQ(data.power_delivered_l3.int_val(), 331);  // register 54
  CHECK_EQ(data.power_returned_l1.int_val(), 0);     // register 56
  CHECK_EQ(data.power_returned_l2.int_val(), 0);     // register 58
  CHECK_EQ(data.power_returned_l3.int_val(), 0);     // register 60

  const char swedishTelegram[] =
      "/ADN9 6534\r\n"
      "1-0:1.8.0(00000001.012*kWh)\r\n"
      "1-0:2.8.0(00000001.001*kWh)\r\n"
      "!";

  TestData swedishData = {};
  result = P1Parser::parse(&swedishData, swedishTelegram,
                           sizeof(swedishTelegram) - 1);
  if (result.err) {
    fprintf(stderr, "Swedish telegram parse failed\n");
    return 1;
  }

  CHECK_EQ(swedishData.energy_delivered_total.int_val(), 1012);  // register 72
  CHECK_EQ(swedishData.energy_returned_total.int_val(), 1001);   // register 74
  CHECK_EQ(swedishData.energy_delivered_total.int_val() +
           swedishData.energy_returned_total.int_val(), 2013);   // register 342
  return failures == 0 ? 0 : 1;
}
