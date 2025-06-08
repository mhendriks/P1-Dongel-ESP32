#ifdef MBUS

#define MBUS_DEV_ID       1
// #define MBUS_PORT       502
#define MBUS_CLIENTS      4
#define MBUS_TIMEOUT  10000
#define MBUS_VAL_UNAVAILABLE 0xFFFFFFFF

float calculateLineVoltage(float V1, float V2) {
  return sqrt(3) * (V1 + V2) / 2.0;
}

// Set up a Modbus server
ModbusServerWiFi MBserver;
const uint8_t MY_SERVER(MBUS_DEV_ID);

// Modbus data types
enum class ModbusDataType { UINT32, INT16, FLOAT };

union {
  float    f;
  uint32_t u;
  int32_t  i;
} map_temp;

// Struct om registerwaarden op te halen
struct ModbusMapping {
    ModbusDataType type;
    std::function<uint32_t()> valueGetter;
};

// Modbus mapping default = 0
std::map<uint16_t, ModbusMapping> mapping_default = {
    { 0,  { ModbusDataType::UINT32, []() { return (uint32_t)(actT - (actTimestamp[12] == 'S' ? 7200 : 3600)); } }},
    { 2,  { ModbusDataType::UINT32, []() { return (DSMRdata.energy_delivered_tariff1_present && !bUseEtotals) ? (uint32_t)DSMRdata.energy_delivered_tariff1.int_val() : MBUS_VAL_UNAVAILABLE; } }},
    { 4,  { ModbusDataType::UINT32, []() { return (DSMRdata.energy_delivered_tariff2_present && !bUseEtotals) ? (uint32_t)DSMRdata.energy_delivered_tariff2.int_val() : MBUS_VAL_UNAVAILABLE; } }},
    { 6,  { ModbusDataType::UINT32, []() { return (DSMRdata.energy_returned_tariff1_present && !bUseEtotals) ? (uint32_t)DSMRdata.energy_returned_tariff1.int_val() : MBUS_VAL_UNAVAILABLE; } }},
    { 8,  { ModbusDataType::UINT32, []() { return (DSMRdata.energy_returned_tariff2_present && !bUseEtotals) ? (uint32_t)DSMRdata.energy_returned_tariff2.int_val() : MBUS_VAL_UNAVAILABLE; } }},
    { 10, { ModbusDataType::UINT32, []() { return (DSMRdata.energy_delivered_total_present) ? (uint32_t)DSMRdata.energy_delivered_total.int_val() : MBUS_VAL_UNAVAILABLE; } }},
    { 12, { ModbusDataType::UINT32, []() { return (DSMRdata.energy_returned_total_present) ? (uint32_t)DSMRdata.energy_returned_total.int_val() : MBUS_VAL_UNAVAILABLE; } }},
    { 14, { ModbusDataType::UINT32, []() { return (DSMRdata.power_delivered_present) ? (uint32_t)DSMRdata.power_delivered.int_val() : MBUS_VAL_UNAVAILABLE; } }},
    { 16, { ModbusDataType::UINT32, []() { return (DSMRdata.power_returned_present) ? (uint32_t)DSMRdata.power_returned.int_val() : MBUS_VAL_UNAVAILABLE; } }},
    { 18, { ModbusDataType::UINT32, []() { return (DSMRdata.voltage_l1_present) ? (uint32_t)DSMRdata.voltage_l1.int_val() : MBUS_VAL_UNAVAILABLE; } }},
    { 20, { ModbusDataType::UINT32, []() { return (DSMRdata.voltage_l2_present) ? (uint32_t)DSMRdata.voltage_l2.int_val() : MBUS_VAL_UNAVAILABLE; } }},
    { 22, { ModbusDataType::UINT32, []() { return (DSMRdata.voltage_l3_present) ? (uint32_t)DSMRdata.voltage_l3.int_val() : MBUS_VAL_UNAVAILABLE; } }},
    { 24, { ModbusDataType::UINT32, []() { return (DSMRdata.current_l1_present) ? (uint32_t)DSMRdata.current_l1.int_val() : MBUS_VAL_UNAVAILABLE; } }},
    { 26, { ModbusDataType::UINT32, []() { return (DSMRdata.current_l2_present) ? (uint32_t)DSMRdata.current_l2.int_val() : MBUS_VAL_UNAVAILABLE; } }},
    { 28, { ModbusDataType::UINT32, []() { return (DSMRdata.current_l3_present) ? (uint32_t)DSMRdata.current_l3.int_val() : MBUS_VAL_UNAVAILABLE; } }},
    { 30, { ModbusDataType::UINT32, []() { return (mbusGas) ? (uint32_t)(epoch(gasDeliveredTimestamp.c_str(), 10, false) - (actTimestamp[12] == 'S' ? 7200 : 3600)) : MBUS_VAL_UNAVAILABLE; } }},
    { 32, { ModbusDataType::UINT32, []() { return (mbusGas) ? (uint32_t)(gasDelivered * 1000) : MBUS_VAL_UNAVAILABLE; } }},
    { 34, { ModbusDataType::UINT32, []() { return (DSMRdata.electricity_tariff_present) ? (uint32_t)atoi(DSMRdata.electricity_tariff.c_str()) : MBUS_VAL_UNAVAILABLE; } }},
    { 36, { ModbusDataType::UINT32, []() { return (DSMRdata.peak_pwr_last_q_present) ? (uint32_t)DSMRdata.peak_pwr_last_q.int_val() : MBUS_VAL_UNAVAILABLE; } }},
    { 38, { ModbusDataType::UINT32, []() { return (int32_t)(DSMRdata.power_delivered_l1_present) ? (int32_t)DSMRdata.power_delivered_l1.int_val() - (int32_t)DSMRdata.power_returned_l1.int_val() : MBUS_VAL_UNAVAILABLE; } }},
    { 40, { ModbusDataType::UINT32, []() { return (int32_t)(DSMRdata.power_delivered_l2_present) ? (int32_t)DSMRdata.power_delivered_l2.int_val() - (int32_t)DSMRdata.power_returned_l2.int_val() : MBUS_VAL_UNAVAILABLE; } }},
    { 42, { ModbusDataType::UINT32, []() { return (int32_t)(DSMRdata.power_delivered_l3_present) ? (int32_t)DSMRdata.power_delivered_l3.int_val() - (int32_t)DSMRdata.power_returned_l3.int_val() : MBUS_VAL_UNAVAILABLE; } }},
};


std::map<uint16_t, ModbusMapping> mapping_mx3xx = {
    {32774,  {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.voltage_l1_present ? DSMRdata.voltage_l1.val() : MBUS_VAL_UNAVAILABLE; return map_temp.u; }}},
    {32776,  {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.voltage_l1_present ? DSMRdata.voltage_l2.val() : MBUS_VAL_UNAVAILABLE; return map_temp.u; }}},
    {32778,  {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.voltage_l1_present ? DSMRdata.voltage_l3.val() : MBUS_VAL_UNAVAILABLE; return map_temp.u; }}},
    {32782,  {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.current_l1_present ? DSMRdata.current_l1.val() : MBUS_VAL_UNAVAILABLE; return map_temp.u; }}},
    {32784,  {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.current_l2_present ? DSMRdata.current_l2.val() : MBUS_VAL_UNAVAILABLE; return map_temp.u; }}},
    {32786,  {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.current_l3_present ? DSMRdata.current_l3.val() : MBUS_VAL_UNAVAILABLE; return map_temp.u; }}},
    {32798,  {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.power_delivered_l1_present ? (-DSMRdata.power_returned_l1.int_val() + DSMRdata.power_delivered_l1.int_val()) :MBUS_VAL_UNAVAILABLE; return map_temp.u;}}},
    {32800,  {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.power_delivered_l2_present ? (-DSMRdata.power_returned_l2.int_val() + DSMRdata.power_delivered_l2.int_val()) :MBUS_VAL_UNAVAILABLE; return map_temp.u;}}},
    {32802,  {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.power_delivered_l3_present ? (-DSMRdata.power_returned_l3.int_val() + DSMRdata.power_delivered_l3.int_val()) :MBUS_VAL_UNAVAILABLE; return map_temp.u;}}},
    {32804,  {ModbusDataType::FLOAT, []() { return mapping_mx3xx[32798].valueGetter(); }}}, // Alias P1
    {32806,  {ModbusDataType::FLOAT, []() { return mapping_mx3xx[32800].valueGetter(); }}}, // Alias P2
    {32808,  {ModbusDataType::FLOAT, []() { return mapping_mx3xx[32802].valueGetter(); }}}, // Alias P3
    {32790,  {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.power_delivered_present ? (-DSMRdata.power_returned.int_val() + DSMRdata.power_delivered.int_val()) :MBUS_VAL_UNAVAILABLE; return map_temp.u; }}},
    {32792,  {ModbusDataType::FLOAT, []() { return mapping_mx3xx[32790].valueGetter(); }}}, // Alias voor 52
    // {62,  {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.power_returned > 0 ? -1.0 : 1.0; return map_temp.u; }}},
    {32816,  {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.power_returned_l1 > 0 ? -1.0 : 1.0; return map_temp.u; }}},
    {32818,  {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.power_returned_l2_present ? (DSMRdata.power_returned_l2 > 0 ? -1.0 : 1.0) :MBUS_VAL_UNAVAILABLE; return map_temp.u; }}},
    {32820,  {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.power_returned_l3_present ? (DSMRdata.power_returned_l3 > 0 ? -1.0 : 1.0) :MBUS_VAL_UNAVAILABLE; return map_temp.u; }}},
    {32780,  {ModbusDataType::FLOAT, []() { map_temp.f = 50.0; return map_temp.u; }}},
    {33024,  {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.energy_delivered_tariff1_present ? DSMRdata.energy_delivered_tariff2 + DSMRdata.energy_delivered_tariff1 :MBUS_VAL_UNAVAILABLE; return map_temp.u; }}},
    {33030,  {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.energy_returned_tariff1_present ? DSMRdata.energy_returned_tariff2 + DSMRdata.energy_returned_tariff1 :MBUS_VAL_UNAVAILABLE; return map_temp.u; }}},
    // {200, {ModbusDataType::FLOAT, []() { map_temp.f = (DSMRdata.voltage_l1_present && DSMRdata.voltage_l2_present) ? calculateLineVoltage(DSMRdata.voltage_l1, DSMRdata.voltage_l2) :MBUS_VAL_UNAVAILABLE; return map_temp.u; }}},
    // {202, {ModbusDataType::FLOAT, []() { map_temp.f = (DSMRdata.voltage_l2_present && DSMRdata.voltage_l3_present) ? calculateLineVoltage(DSMRdata.voltage_l2, DSMRdata.voltage_l3) :MBUS_VAL_UNAVAILABLE; return map_temp.u; }}},
    // {204, {ModbusDataType::FLOAT, []() { map_temp.f = (DSMRdata.voltage_l3_present && DSMRdata.voltage_l1_present) ? calculateLineVoltage(DSMRdata.voltage_l3, DSMRdata.voltage_l1) :MBUS_VAL_UNAVAILABLE; return map_temp.u; }}}
};

// Modbus mapping SDM630 = 1, see https://www.eastroneurope.com/images/uploads/products/protocol/SDM630_MODBUS_Protocol.pdf
std::map<uint16_t, ModbusMapping> mapping_sdm630 = {
    {0,   {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.voltage_l1_present ? DSMRdata.voltage_l1.val() : MBUS_VAL_UNAVAILABLE; return map_temp.u; }}},
    {2,   {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.voltage_l1_present ? DSMRdata.voltage_l2.val() : MBUS_VAL_UNAVAILABLE; return map_temp.u; }}},
    {4,   {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.voltage_l1_present ? DSMRdata.voltage_l3.val() : MBUS_VAL_UNAVAILABLE; return map_temp.u; }}},
    {6,   {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.current_l1_present ? DSMRdata.current_l1.val() : MBUS_VAL_UNAVAILABLE; return map_temp.u; }}},
    {8,   {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.current_l2_present ? DSMRdata.current_l2.val() : MBUS_VAL_UNAVAILABLE; return map_temp.u; }}},
    {10,  {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.current_l3_present ? DSMRdata.current_l3.val() : MBUS_VAL_UNAVAILABLE; return map_temp.u; }}},
    {12,  {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.power_delivered_l1_present ? (-DSMRdata.power_returned_l1.val() + DSMRdata.power_delivered_l1.val()) * 1000.0 :MBUS_VAL_UNAVAILABLE; return map_temp.u;}}},
    {14,  {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.power_delivered_l2_present ? (-DSMRdata.power_returned_l2.val() + DSMRdata.power_delivered_l2.val()) * 1000.0 :MBUS_VAL_UNAVAILABLE; return map_temp.u;}}},
    {16,  {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.power_delivered_l3_present ? (-DSMRdata.power_returned_l3.val() + DSMRdata.power_delivered_l3.val()) * 1000.0 :MBUS_VAL_UNAVAILABLE; return map_temp.u;}}},
    {24,  {ModbusDataType::FLOAT, []() { return mapping_sdm630[12].valueGetter(); }}}, // Alias P1
    {26,  {ModbusDataType::FLOAT, []() { return mapping_sdm630[14].valueGetter(); }}}, // Alias P2
    {28,  {ModbusDataType::FLOAT, []() { return mapping_sdm630[16].valueGetter(); }}}, // Alias P3
    {52,  {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.power_delivered_present ? (-DSMRdata.power_returned.val() + DSMRdata.power_delivered.val()) * 1000.0 :MBUS_VAL_UNAVAILABLE; return map_temp.u; }}},
    {60,  {ModbusDataType::FLOAT, []() { return mapping_sdm630[52].valueGetter(); }}}, // Alias voor 52
    {62,  {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.power_returned > 0 ? -1.0 : 1.0; return map_temp.u; }}},
    {30,  {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.power_returned_l1 > 0 ? -1.0 : 1.0; return map_temp.u; }}},
    {32,  {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.power_returned_l2_present ? (DSMRdata.power_returned_l2 > 0 ? -1.0 : 1.0) :MBUS_VAL_UNAVAILABLE; return map_temp.u; }}},
    {34,  {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.power_returned_l3_present ? (DSMRdata.power_returned_l3 > 0 ? -1.0 : 1.0) :MBUS_VAL_UNAVAILABLE; return map_temp.u; }}},
    {70,  {ModbusDataType::FLOAT, []() { map_temp.f = 50.0; return map_temp.u; }}},
    {72,  {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.energy_delivered_tariff1_present ? DSMRdata.energy_delivered_tariff2 + DSMRdata.energy_delivered_tariff1 :MBUS_VAL_UNAVAILABLE; return map_temp.u; }}},
    {74,  {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.energy_returned_tariff1_present ? DSMRdata.energy_returned_tariff2 + DSMRdata.energy_returned_tariff1 :MBUS_VAL_UNAVAILABLE; return map_temp.u; }}},
    {200, {ModbusDataType::FLOAT, []() { map_temp.f = (DSMRdata.voltage_l1_present && DSMRdata.voltage_l2_present) ? calculateLineVoltage(DSMRdata.voltage_l1, DSMRdata.voltage_l2) :MBUS_VAL_UNAVAILABLE; return map_temp.u; }}},
    {202, {ModbusDataType::FLOAT, []() { map_temp.f = (DSMRdata.voltage_l2_present && DSMRdata.voltage_l3_present) ? calculateLineVoltage(DSMRdata.voltage_l2, DSMRdata.voltage_l3) :MBUS_VAL_UNAVAILABLE; return map_temp.u; }}},
    {204, {ModbusDataType::FLOAT, []() { map_temp.f = (DSMRdata.voltage_l3_present && DSMRdata.voltage_l1_present) ? calculateLineVoltage(DSMRdata.voltage_l3, DSMRdata.voltage_l1) :MBUS_VAL_UNAVAILABLE; return map_temp.u; }}}
};

// https://www.socomec.it/sites/default/files/2021-05/COUNTIS-E27-MODBUS_COMMUNICATION-TABLE_2017-08_CMT_EN.html
std::map<uint16_t, ModbusMapping> mapping_alfen_socomec = {
    { 0xC560, { ModbusDataType::UINT32, []() { return (DSMRdata.current_l1_present ? (uint32_t)DSMRdata.current_l1.int_val() : MBUS_VAL_UNAVAILABLE); } }},
    { 0xC562, { ModbusDataType::UINT32, []() { return (DSMRdata.current_l2_present ? (uint32_t)DSMRdata.current_l2.int_val() : MBUS_VAL_UNAVAILABLE); } }},
    { 0xC564, { ModbusDataType::UINT32, []() { return (DSMRdata.current_l3_present ? (uint32_t)DSMRdata.current_l3.int_val() : MBUS_VAL_UNAVAILABLE); } }},

    { 0xC566, { ModbusDataType::UINT32, []() { return (DSMRdata.current_l1_present ? (uint32_t)DSMRdata.current_l1.int_val()+DSMRdata.current_l2.int_val()+DSMRdata.current_l3.int_val() : MBUS_VAL_UNAVAILABLE); } }},

    { 0xC570 /* 50544 */, { ModbusDataType::UINT32, []() { return (DSMRdata.power_delivered_l1_present ? (int32_t)(( DSMRdata.power_delivered_l1.val() - DSMRdata.power_returned_l1.val() ) * 100) : MBUS_VAL_UNAVAILABLE); } }},
    { 0xC572 /* 50546 */, { ModbusDataType::UINT32, []() { return (DSMRdata.power_delivered_l2_present ? (int32_t)(( DSMRdata.power_delivered_l2.val() - DSMRdata.power_returned_l2.val() ) * 100) : MBUS_VAL_UNAVAILABLE); } }},
    { 0xC574 /* 50548 */, { ModbusDataType::UINT32, []() { return (DSMRdata.power_delivered_l3_present ? (int32_t)(( DSMRdata.power_delivered_l3.val() - DSMRdata.power_returned_l3.val() ) * 100) : MBUS_VAL_UNAVAILABLE); } }}
};

std::map<uint16_t, ModbusMapping> mapping_abb_b21 = {
    {0x5B00,   {ModbusDataType::UINT32, []() { return ( DSMRdata.voltage_l1_present ? (uint32_t)(DSMRdata.voltage_l1 * 10) : MBUS_VAL_UNAVAILABLE);  }}},
    {0x5B02,   {ModbusDataType::UINT32, []() { return ( DSMRdata.voltage_l1_present ? (uint32_t)(DSMRdata.voltage_l2 * 10): MBUS_VAL_UNAVAILABLE);  }}},
    {0x5B04,   {ModbusDataType::UINT32, []() { return ( DSMRdata.voltage_l1_present ? (uint32_t)(DSMRdata.voltage_l3 * 10) : MBUS_VAL_UNAVAILABLE);  }}},

    { 0x5B0C, { ModbusDataType::UINT32, []() { return (DSMRdata.current_l1_present ? (uint32_t)DSMRdata.current_l1.int_val()/10 : MBUS_VAL_UNAVAILABLE); } }},
    { 0x5B0E, { ModbusDataType::UINT32, []() { return (DSMRdata.current_l2_present ? (uint32_t)DSMRdata.current_l2.int_val()/10 : MBUS_VAL_UNAVAILABLE); } }},
    { 0x5B10, { ModbusDataType::UINT32, []() { return (DSMRdata.current_l3_present ? (uint32_t)DSMRdata.current_l3.int_val()/10 : MBUS_VAL_UNAVAILABLE); } }},

    { 0x5B14,        { ModbusDataType::UINT32, []() { return (DSMRdata.power_delivered_present ? (int32_t)(( DSMRdata.power_delivered.val() - DSMRdata.power_returned.val() ) * 100) : MBUS_VAL_UNAVAILABLE); } }},
    { 0x5B16 /*  */, { ModbusDataType::UINT32, []() { return (DSMRdata.power_delivered_l1_present ? (int32_t)(( DSMRdata.power_delivered_l1.val() - DSMRdata.power_returned_l1.val() ) * 100) : MBUS_VAL_UNAVAILABLE); } }},
    { 0x5B18 /*  */, { ModbusDataType::UINT32, []() { return (DSMRdata.power_delivered_l2_present ? (int32_t)(( DSMRdata.power_delivered_l2.val() - DSMRdata.power_returned_l2.val() ) * 100) : MBUS_VAL_UNAVAILABLE); } }},
    { 0x5B1A /*  */, { ModbusDataType::UINT32, []() { return (DSMRdata.power_delivered_l3_present ? (int32_t)(( DSMRdata.power_delivered_l3.val() - DSMRdata.power_returned_l3.val() ) * 100) : MBUS_VAL_UNAVAILABLE); } }}
};

// Modbus mapping CHINT DTSU666
std::map<uint16_t, ModbusMapping> mapping_dtsu666 = {
  //U
    {0x2006,   {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.voltage_l1_present ? DSMRdata.voltage_l1.val() * 10.0: MBUS_VAL_UNAVAILABLE; return map_temp.u; }}},
    {0x2008,   {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.voltage_l1_present ? DSMRdata.voltage_l2.val() * 10.0: MBUS_VAL_UNAVAILABLE; return map_temp.u; }}},
    {0x200A,   {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.voltage_l1_present ? DSMRdata.voltage_l3.val() * 10.0: MBUS_VAL_UNAVAILABLE; return map_temp.u; }}},
  //I
    {0x200C,   {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.current_l1_present ? DSMRdata.current_l1.int_val() : MBUS_VAL_UNAVAILABLE; return map_temp.u; }}},
    {0x200E,   {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.current_l2_present ? DSMRdata.current_l2.int_val() : MBUS_VAL_UNAVAILABLE; return map_temp.u; }}},
    {0x2010,   {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.current_l3_present ? DSMRdata.current_l3.int_val() : MBUS_VAL_UNAVAILABLE; return map_temp.u; }}},

  //P
    {0x2012,  {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.power_delivered_present ? (-DSMRdata.power_returned.val() + DSMRdata.power_delivered.val()) * 100.0 :MBUS_VAL_UNAVAILABLE; return map_temp.u;}}},
    {0x2014,  {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.power_delivered_l1_present ? (-DSMRdata.power_returned_l1.val() + DSMRdata.power_delivered_l1.val()) * 100.0 :MBUS_VAL_UNAVAILABLE; return map_temp.u;}}},
    {0x2016,  {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.power_delivered_l2_present ? (-DSMRdata.power_returned_l2.val() + DSMRdata.power_delivered_l2.val()) * 100.0 :MBUS_VAL_UNAVAILABLE; return map_temp.u;}}},
    {0x2018,  {ModbusDataType::FLOAT, []() { map_temp.f = DSMRdata.power_delivered_l3_present ? (-DSMRdata.power_returned_l3.val() + DSMRdata.power_delivered_l3.val()) * 100.0 :MBUS_VAL_UNAVAILABLE; return map_temp.u;}}} 
};

// uint16_t getMaxKey(const std::map<uint16_t, ModbusMapping>& mapping) {
//     if (mapping.empty()) return 0; // Of een andere passende foutwaarde
//     return mapping.rbegin()->first; // Laatste sleutel in gesorteerde map
// }

// Pointer to the active mapping
std::map<uint16_t, ModbusMapping>* selectedMapping = &mapping_default;  // Standaard mapping
uint16_t MaxReg[7] = { 44, 206, 24, 0xC574+2, 100, 0x5B1A+2, 33030+2 };


// Change active mapping
void setModbusMapping(int mappingChoice) {
    SelMap = mappingChoice;
    switch (mappingChoice) {
        case 0: selectedMapping = &mapping_default; break;
        case 1: selectedMapping = &mapping_sdm630; break;
        case 2: selectedMapping = &mapping_dtsu666; break;
        case 3: selectedMapping = &mapping_alfen_socomec; break;
        // case 4: selectedMapping = &mapping_em330; break;
        case 5: selectedMapping = &mapping_abb_b21; break;
        case 6: selectedMapping = &mapping_mx3xx; break;
        default: selectedMapping = &mapping_default; break; // Fallback naar default
    }
}

//handle modbus request
ModbusMessage MBusHandleRequest(ModbusMessage request) {
    uint16_t address;
    uint16_t words;
    digitalWrite(LED, LOW);

    ModbusMessage response;
    request.get(2, address);
    request.get(4, words);

    Debugf("addr [0x%X] words [0x%02X] DevID [0x%02X] FC [0x%02X]\n", address, words, request.getServerID(), request.getFunctionCode());

    if ( (address + words) > MaxReg[SelMap] ){
        response.setError(request.getServerID(), request.getFunctionCode(), ILLEGAL_DATA_ADDRESS);
        return response;
    }

    response.add(request.getServerID(), request.getFunctionCode(), (uint8_t)(words * 2));

    uint16_t currentAddr = address;
    union uMbusData {
      float    f;
      uint32_t u;
      int32_t  i;
      uint8_t  b[4];
    } val;

    while (currentAddr < (address + words)) {
        
        ModbusDataType type = ModbusDataType::UINT32;
        if (selectedMapping->find(currentAddr) != selectedMapping->end()) {
            type = (*selectedMapping)[currentAddr].type;
            if ( type == ModbusDataType::UINT32 ){
              val.u = (*selectedMapping)[currentAddr].valueGetter();
            } else if ( type == ModbusDataType::FLOAT ){
              val.u = (float)(*selectedMapping)[currentAddr].valueGetter();
            }     
        } else {
            Debugf("MBUS WRONG VALUE -- addr: %d\n", currentAddr);
            val.u = MBUS_VAL_UNAVAILABLE;
        }

#ifdef DEBUG
        Debugf("\n--MODBUS uint32 : %u\n", val.u);
        Debugf("--MODBUS int32  : %i\n", val.i);
        Debugf("--MODBUS HEX    : %08X\n", val.u);
        Debugf("--MODBUS FLOAT  : %f\n",   val.f);
        Debugf("--MODBUS Bytes  : [%d] [%d]\n", val.b[1], val.b[0]);
#endif
        switch (type) {
            case ModbusDataType::FLOAT: 
            case ModbusDataType::UINT32: {
                response.add(val.u);
                currentAddr += 2;
                break;
            }
            case ModbusDataType::INT16: {
                response.add((int16_t)(val.b[0]));
                currentAddr += 1;
                break;
            }
        }
    }

    digitalWrite(LED, HIGH);
    return response;
}

void mbusSetup(){

  MBserver.registerWorker(MY_SERVER, READ_HOLD_REGISTER, &MBusHandleRequest);//FC03
  MBserver.registerWorker(MY_SERVER, READ_INPUT_REGISTER, &MBusHandleRequest);//FC04
  MBserver.start(mb_config.port, MBUS_CLIENTS, MBUS_TIMEOUT);
}

#endif //MBUS

#ifdef MB_RTU

// #define MBUS_RTU_BAUD         9600
// #define MBUS_RTU_SERIAL       SERIAL_8E1
#define MBUS_RTU_TIMEOUT      2000

#include "ModbusServerRTU.h"
#include "DSMRloggerAPI.h"

ModbusServerRTU* MBserverRTU;

void SetupMB_RTU(){

  // Debugf("mb_rx : %d \n",mb_rx);
  // Debugf("mb_tx : %d \n",mb_tx);
  // Debugf("mb_rts: %d \n",mb_rts);

  if ( mb_rx == -1 ) {
    DebugTln("Setup Modbus RTU TERMINATED");
    return;
  }
  DebugTln("Setup Modbus RTU");
  RTU_SERIAL.end();
  RTUutils::prepareHardwareSerial(RTU_SERIAL);
  RTU_SERIAL.begin( mb_config.baud , mb_config.parity , mb_rx, mb_tx );

  MBserverRTU = new ModbusServerRTU(MBUS_RTU_TIMEOUT, mb_rts);

  MBserverRTU->registerWorker(mb_config.id , READ_HOLD_REGISTER, &MBusHandleRequest);//FC03
  MBserverRTU->registerWorker(mb_config.id, READ_INPUT_REGISTER, &MBusHandleRequest);//FC04
  MBserverRTU->begin(RTU_SERIAL);
}
#else
void SetupMB_RTU(){}
#endif
