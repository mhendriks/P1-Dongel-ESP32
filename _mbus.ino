#ifdef MBUS

/***

0 energy meter sample timestamp epoch | dword
2 energy_delivered_tariff1 | dword
4 energy_delivered_tariff2 | dword
6 energy_returned_tariff1 | dword
8 energy_returned_tariff2 | dword
10 energy_delivered_total | dword
12 energy_returned_total | dword
14 power_delivered | dword
16 power_returned | dword
18 gas_delivered | dword
20 gas_timestamp epoch

*/

#define MBUS_DEV_ID 1
#define MBUS_PORT 502
#define MBUS_CLIENTS 4
#define MBUS_TIMEOUT 10000

// Set up a Modbus server
ModbusServerWiFi MBserver;

const uint8_t MY_SERVER(MBUS_DEV_ID);

// Worker function for serverID=1, function code 0x03 or 0x04
ModbusMessage MBusHandleRequest(ModbusMessage request) {
  uint16_t addr = 0;        // Start address to read
  uint16_t wrds = 0;        // Number of words to read
  ModbusMessage response;

  // Get addr and words from data array. Values are MSB-first, get() will convert to binary
  request.get(2, addr);
  request.get(4, wrds);
  DebugT(F("MBUS - addr : "));Debugln(addr);
  DebugT(F("MBUS - wrds : "));Debugln(wrds);
  
  // address valid?
  if (addr > 21) {
    // No. Return error response
    response.setError(request.getServerID(), request.getFunctionCode(), ILLEGAL_DATA_ADDRESS);
    return response;
  }

  // Number of words valid?
  if (!wrds || (addr + wrds) > 20) {
    // No. Return error response
    response.setError(request.getServerID(), request.getFunctionCode(), ILLEGAL_DATA_ADDRESS);
    return response;
  }

  // Prepare response
  response.add(request.getServerID(), request.getFunctionCode(), (uint8_t)(wrds * 2));

  switch( addr ) {
    case 0: 
//            u.ivalue = (uint32_t) actT;
//            response.add(u.words[0]);
//            response.add(u.words[1]);
            response.add((uint32_t) actT);
            DebugT(F("MBUS: value 0: "));Debugln((uint32_t) actT);
            break;
    case 2: //energy_delivered_tariff1
            if ( !DSMRdata.energy_delivered_tariff1_present ) response.add((uint32_t)0xFFFFFFFF);
            else response.add((uint32_t)DSMRdata.energy_delivered_tariff1);
            DebugT(F("MBUS: value 2: "));Debugln((uint32_t)DSMRdata.energy_delivered_tariff1);
            break;           
    case 4: //energy_delivered_tariff2
            if ( !DSMRdata.energy_delivered_tariff2_present ) response.add((uint32_t)0xFFFFFFFF);
            else response.add((uint32_t)DSMRdata.energy_delivered_tariff2);
            DebugT(F("MBUS: value 4: "));Debugln((uint32_t)DSMRdata.energy_delivered_tariff2);
            break;
    case 6: //energy_returned_tariff1
            if ( !DSMRdata.energy_returned_tariff1_present ) response.add((uint32_t)0xFFFFFFFF);
            else response.add((uint32_t)DSMRdata.energy_returned_tariff1);
            break;
    case 8: //energy_returned_tariff2
            if ( !DSMRdata.energy_returned_tariff2_present ) response.add((uint32_t)0xFFFFFFFF);
            else response.add((uint32_t)DSMRdata.energy_returned_tariff2);
            break;
    case 10: //energy_delivered_total
            if ( !DSMRdata.energy_delivered_total_present ) response.add((uint32_t)0xFFFFFFFF);
            else response.add((uint32_t)DSMRdata.energy_delivered_total);
            break;
    case 12: //energy_returned_total
            if ( !DSMRdata.energy_returned_total_present ) response.add((uint32_t)0xFFFFFFFF);
            else response.add((uint32_t)DSMRdata.energy_returned_total);
            break;
    default: //wrong value
            DebugT(F("MBUS: WRONG VALUE -- addr: "));Debugln(addr);
            break;
  }

//  // Loop over all words to be sent
//  for (uint16_t i = 0; i < wrds; i++) {
//    // Add word MSB-first to response buffer
//    response.add(memo[addr + i]);
//  }

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

void HandleMBus(){
  
}

#endif //MBUS
