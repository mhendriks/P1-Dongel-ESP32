#ifdef MBUS

/***

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

//TODO : DEV_ID configureren en evt ook de rest

#define MBUS_DEV_ID       1
#define MBUS_PORT       502
#define MBUS_CLIENTS      4
#define MBUS_TIMEOUT  10000
#define MBUS_VAL_UNAVAILABLE 0xFFFFFFFF

// Set up a Modbus server
ModbusServerWiFi MBserver;

const uint8_t MY_SERVER(MBUS_DEV_ID);

// Worker function for serverID=1, function code 0x03 or 0x04
ModbusMessage MBusHandleRequest(ModbusMessage request) {
  uint16_t addr = 0;        // Start address to read
  uint16_t wrds = 0;        // Number of words to read
  uint32_t mbus_val;
  ModbusMessage response;

  // Get addr and words from data array. Values are MSB-first, get() will convert to binary
  request.get(2, addr);
  request.get(4, wrds);
  DebugTf("MBUS - addr : %d | wrds : %d\n",addr, wrds);

  // address + wrds valid?
  if (addr > 35 || !wrds || (addr + wrds) > 34) {
    // No. Return error response
    response.setError(request.getServerID(), request.getFunctionCode(), ILLEGAL_DATA_ADDRESS);
    return response;
  }
  
  // Prepare response
  response.add(request.getServerID(), request.getFunctionCode(), (uint8_t)(wrds * 2));
  uint16_t utc_comp = 3600;
  if ( actTimestamp[12] == 'S') utc_comp = 7200;

  for ( int i = 0; i < wrds; i=i+2 ) {
    switch( addr + i) {
      case 0: //time UTC
              mbus_val = (uint32_t) (actT - utc_comp);
              break;
      case 2: //energy_delivered_tariff1
              if ( !DSMRdata.energy_delivered_tariff1_present ) mbus_val = MBUS_VAL_UNAVAILABLE;
              else mbus_val = (uint32_t)(DSMRdata.energy_delivered_tariff1*1000);
              break;           
      case 4: //energy_delivered_tariff2
              if ( !DSMRdata.energy_delivered_tariff2_present ) mbus_val = MBUS_VAL_UNAVAILABLE;
              else mbus_val = (uint32_t)(DSMRdata.energy_delivered_tariff2*1000);
              break;
      case 6: //energy_returned_tariff1
                if ( !DSMRdata.energy_returned_tariff1_present ) mbus_val = MBUS_VAL_UNAVAILABLE;
                else mbus_val = (uint32_t)(DSMRdata.energy_returned_tariff1*1000);
              break;
      case 8: //energy_returned_tariff2
                if ( !DSMRdata.energy_returned_tariff2_present ) mbus_val = MBUS_VAL_UNAVAILABLE;
                else mbus_val = (uint32_t)(DSMRdata.energy_returned_tariff2*1000);
              break;
      case 10: // 
              mbus_val = MBUS_VAL_UNAVAILABLE;
              break;
      case 12: //
              mbus_val = MBUS_VAL_UNAVAILABLE;
              break;
      case 14: //energy_delivered
                if ( !DSMRdata.power_delivered_present ) mbus_val = MBUS_VAL_UNAVAILABLE;
                else mbus_val = (uint32_t)(DSMRdata.power_delivered*1000);
              break;
      case 16: //energy_returned
                if ( !DSMRdata.power_returned_present ) mbus_val = MBUS_VAL_UNAVAILABLE;
                else mbus_val = (uint32_t)(DSMRdata.power_returned*1000);
              break;
      case 18: //U1 
                 if ( !DSMRdata.voltage_l1_present ) mbus_val = MBUS_VAL_UNAVAILABLE;
                else mbus_val = (uint32_t)(DSMRdata.voltage_l1*1000);
              break;
      case 20: //U2
                if ( !DSMRdata.voltage_l2_present ) mbus_val = MBUS_VAL_UNAVAILABLE;
                else mbus_val = (uint32_t)(DSMRdata.voltage_l2*1000);
              break;                
      case 22: //U3
                if ( !DSMRdata.voltage_l3_present ) mbus_val = MBUS_VAL_UNAVAILABLE;
                else mbus_val = (uint32_t)(DSMRdata.voltage_l3*1000);
              break;
      case 24: //I1 
                if ( !DSMRdata.current_l1_present ) mbus_val = MBUS_VAL_UNAVAILABLE;
                else mbus_val = (uint32_t)(DSMRdata.current_l1*1000);
              break;
      case 26: //I2
                if ( !DSMRdata.current_l2_present ) mbus_val = MBUS_VAL_UNAVAILABLE;
                else mbus_val = (uint32_t)(DSMRdata.current_l2*1000);
              break;                
      case 28: //I3
                if ( !DSMRdata.current_l3_present ) mbus_val = MBUS_VAL_UNAVAILABLE;
                else mbus_val = (uint32_t)(DSMRdata.current_l3*1000);
              break;
      case 30: //gas timestamp   
                if ( !mbusGas ) mbus_val = MBUS_VAL_UNAVAILABLE;
                else mbus_val = (uint32_t)(epoch(gasDeliveredTimestamp.c_str(), 10, false) - utc_comp);
              break;
      case 32: //gas
                if ( !mbusGas ) mbus_val = MBUS_VAL_UNAVAILABLE;
                else mbus_val = (uint32_t)(gasDelivered * 1000);
              break;        
      default: //wrong value
              DebugT(F("MBUS WRONG VALUE -- addr: "));Debugln(addr + i);
              break;
    }
    
    DebugTf("MBUS: value %d : %d\n",i,mbus_val);
    response.add(mbus_val);
  
  } //for i

  // Return the data response
  return response;
}

void mbusSetup(){

  // Register the worker function with the Modbus server
  MBserver.registerWorker(MY_SERVER, READ_HOLD_REGISTER, &MBusHandleRequest);
  
  // Register the worker function again for another FC
  MBserver.registerWorker(MY_SERVER, READ_INPUT_REGISTER, &MBusHandleRequest);
  
  // Start the Modbus TCP server:
  MBserver.start(MBUS_PORT, MBUS_CLIENTS, MBUS_TIMEOUT);
}

#endif //MBUS
