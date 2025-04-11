#ifdef MBUS

#define MBUS_DEV_ID       1
#define MBUS_PORT       502
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

// Struct om registerwaarden op te halen
struct ModbusMapping {
    ModbusDataType type;
    std::function<uint32_t()> valueGetter;
};

/***

DEFAULT MAPPING

ALL : UINT32 - 4321
Unavailable value : MBUS_VAL_UNAVAILABLE

0 energy meter sample timestamp epoch UTC | dword
2 energy_delivered_tariff1 | dword
4 energy_delivered_tariff2 | dword
6 energy_returned_tariff1 | dword
8 energy_returned_tariff2 | dword
10 energy_delivered_total | dword
12 energy_returned_total | dword
14 power_delivered | dword
16 power_returned | dword
18 U1 (mV) | dword
20 U2 (mV) | dword
22 U3 (mV) | dword
24 I1 (mA) | dword (todo indication delivered / injected)
26 I2 (mA) | dword (todo indication delivered / injected)
28 I3 (mA) | dword (todo indication delivered / injected)
30 Gas Timestamp epoch UTC | dword
32 Gas delivered | dword

*/

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

// Modbus mapping SDM630 = 1, see https://www.eastroneurope.com/images/uploads/products/protocol/SDM630_MODBUS_Protocol.pdf

union {
  float    f;
  uint32_t u;
  int32_t  i;
} map_temp;

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

    { 0xC570, { ModbusDataType::UINT32, []() { return (DSMRdata.power_delivered_l1_present ? (-DSMRdata.power_returned_l1.int_val() + DSMRdata.power_delivered_l1.int_val()) * 10 :MBUS_VAL_UNAVAILABLE); } }},
    { 0xC572, { ModbusDataType::UINT32, []() { return (DSMRdata.power_delivered_l2_present ? (-DSMRdata.power_returned_l2.int_val() + DSMRdata.power_delivered_l2.int_val()) * 10 :MBUS_VAL_UNAVAILABLE); } }},
    { 0xC574, { ModbusDataType::UINT32, []() { return (DSMRdata.power_delivered_l3_present ? (-DSMRdata.power_returned_l3.int_val() + DSMRdata.power_delivered_l3.int_val()) * 10 :MBUS_VAL_UNAVAILABLE); } }},
};

// Modbus mapping CHINT DTSU666
std::map<uint16_t, ModbusMapping> mapping_dtsu666 = {
    {0x0,   {ModbusDataType::INT16, []() { return 204; }}},
//     {0x1,   {ModbusDataType::INT16, []() { return 701; }}},
//     {0x2,   {ModbusDataType::INT16, []() { return 0; }}},
//     {0x3,   {ModbusDataType::INT16, []() { return 0; }}},
//     {0x5,   {ModbusDataType::INT16, []() { return 0; }}},
//     {0x6,   {ModbusDataType::INT16, []() { return 1; }}},
//     {0x7,   {ModbusDataType::INT16, []() { return 10; }}},
//     {0xA, {ModbusDataType::INT16, []() { return 0; }}},
//     {0xC, {ModbusDataType::INT16, []() { return 0; }}},
//     {0x2C, {ModbusDataType::INT16, []() { return 3; }}},
//     {0x2D, {ModbusDataType::INT16, []() { 
//         // if (Baudrate == 1200) return 0;
//         // if (Baudrate == 2400) return 1;
//         // if (Baudrate == 4800) return 2;
//         return 3;
//     }}},
//     {0x2E, {ModbusDataType::INT16, []() { return MBUS_DEV_ID; }}},
//     {0x2012, {ModbusDataType::FLOAT, []() { return DSMRdata.power_delivered_l1_present ? 
//         -10.0 * DSMRdata.power_returned_l1 + 10.0 * DSMRdata.power_delivered_l1 +
//         -10.0 * DSMRdata.power_returned_l2 + 10.0 * DSMRdata.power_delivered_l2 +
//         -10.0 * DSMRdata.power_returned_l3 + 10.0 * DSMRdata.power_delivered_l3 : 0.0;
//     }}},
//     {0x201A, {ModbusDataType::FLOAT, mapping_dtsu666[0x2012].valueGetter}},  // Alias voor 0x2012
//     {0x2014, {ModbusDataType::FLOAT, []() { return DSMRdata.power_delivered_l1_present ? (DSMRdata.power_returned_l1 + DSMRdata.power_delivered_l1) * (DSMRdata.power_returned_l1 ? -10.0 : 10.0) : 0.0; }}},
//     {0x201C, {ModbusDataType::FLOAT, mapping_dtsu666[0x2014].valueGetter}},  // Alias voor 0x2014
//     {0x2016, {ModbusDataType::FLOAT, []() { return DSMRdata.power_delivered_l2_present ? (DSMRdata.power_returned_l2 + DSMRdata.power_delivered_l2) * (DSMRdata.power_returned_l2 ? -10.0 : 10.0) : 0.0; }}},
//     {0x201E, {ModbusDataType::FLOAT, mapping_dtsu666[0x2016].valueGetter}},  // Alias voor 0x2016
//     {0x2018, {ModbusDataType::FLOAT, []() { return DSMRdata.power_delivered_l3_present ? (DSMRdata.power_returned_l3 + DSMRdata.power_delivered_l3) * (DSMRdata.power_returned_l3 ? -10.0 : 10.0) : 0.0; }}},
//     {0x2020, {ModbusDataType::FLOAT, mapping_dtsu666[0x2018].valueGetter}},  // Alias voor 0x2018
};

// uint16_t getMaxKey(const std::map<uint16_t, ModbusMapping>& mapping) {
//     if (mapping.empty()) return 0; // Of een andere passende foutwaarde
//     return mapping.rbegin()->first; // Laatste sleutel in gesorteerde map
// }

// Pointer to the active mapping
std::map<uint16_t, ModbusMapping>* selectedMapping = &mapping_default;  // Standaard mapping
uint16_t MaxReg[4] = {44, 206, 24, 0xC574+2};

// Change active mapping
void setModbusMapping(int mappingChoice) {
    SelMap = mappingChoice;
    switch (mappingChoice) {
        case 0: selectedMapping = &mapping_default; break;
        case 1: selectedMapping = &mapping_sdm630; break;
        case 2: selectedMapping = &mapping_dtsu666; break;
        case 3: selectedMapping = &mapping_alfen_socomec; break;
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
  MBserver.start(MBUS_PORT, MBUS_CLIENTS, MBUS_TIMEOUT);
}

#endif //MBUS

#ifdef MB_RTU

// #define MBUS_RTU_DEV_ID       1
#define MBUS_RTU_BAUD         9600
#define MBUS_RTU_SERIAL       SERIAL_8E1
#define MBUS_RTU_TIMEOUT      2000

#include "ModbusServerRTU.h"
#include "DSMRloggerAPI.h"

// ModbusServerRTU MBserverRTU( MBUS_RTU_TIMEOUT, RTSPIN );
ModbusServerRTU* MBserverRTU;

void SetupMB_RTU(){
#ifndef ULTRA  
  if ( Module != _MOD_RS485 ) {
    DebugTln("Setup Modbus RTU TERMINATED");
    return;
  }
  #define RTU_SERIAL Serial
#else 
  #define RTU_SERIAL Serial2
#endif
  DebugTln("Setup Modbus RTU");
  RTU_SERIAL.end();
  RTUutils::prepareHardwareSerial(RTU_SERIAL);
  RTU_SERIAL.begin( MBUS_RTU_BAUD, MBUS_RTU_SERIAL, mb_rx, mb_tx );

  MBserverRTU = new ModbusServerRTU(MBUS_RTU_TIMEOUT, mb_rts);

  MBserverRTU->registerWorker(MBUS_DEV_ID, READ_HOLD_REGISTER, &MBusHandleRequest);//FC03
  MBserverRTU->registerWorker(MBUS_DEV_ID, READ_INPUT_REGISTER, &MBusHandleRequest);//FC04
  MBserverRTU->begin(RTU_SERIAL);
}
#else
void SetupMB_RTU(){}
#endif
