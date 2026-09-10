#include <dsmr3.h>

#include <stdint.h>
#include <stdio.h>

using TestData = ParsedData<
  power_delivered, power_returned,
  power_delivered_l1, power_delivered_l2, power_delivered_l3,
  power_returned_l1, power_returned_l2, power_returned_l3
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
  return failures == 0 ? 0 : 1;
}
