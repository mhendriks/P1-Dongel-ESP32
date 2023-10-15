#ifdef VOLTAGE_MON

void PrintVarr(){
  char buffer[DATA_RECLEN_V+1];    
  DebugT(F("Current Max Voltage: "));Debugln(MaxVoltage);
  for ( byte i = 0; i < VmaxSlots; i++){
  
  //"%-12.12s,%3.1f,%3.1f,%3.1f,%3d"
   snprintf(buffer, sizeof(buffer), (char*)DATA_FORMAT_V, MaxVarr[i].ts , (uint16_t)MaxVarr[i].L1, (uint16_t)MaxVarr[i].L2, (uint16_t)MaxVarr[i].L3, (uint16_t)MaxVarr[i].MaxV, (char) MaxVarr[i].Over );
   Debugln(buffer);
//   Debug(MaxVarr[i].L1);Debug(",");
//   Debug(MaxVarr[i].L2);Debug(",");
//   Debugln(MaxVarr[i].Over);
  }
}

//-------------------------------------------------

void SaveVoltage(){
  //todo
}

void StoreVoltage(bool over){
  strncpy(MaxVarr[Vcount].ts, actTimestamp, 12);
  MaxVarr[Vcount].L1    = (uint16_t) DSMRdata.voltage_l1 * 10;
  MaxVarr[Vcount].L2    = (uint16_t) DSMRdata.voltage_l2 * 10;
  MaxVarr[Vcount].L3    = (uint16_t) DSMRdata.voltage_l3 * 10;
  MaxVarr[Vcount].MaxV  = MaxVoltage;
  MaxVarr[Vcount].Over  = over?'>':'<';
  
  Vcount++;
  if ( Vcount == VmaxSlots ) Vcount = 0;
} //StoreVoltage

//-------------------------------------------------

void ProcessMaxVoltage(){
  if ( MaxVoltage < 207 || !DSMRdata.voltage_l1_present ) return;
  if ( DSMRdata.voltage_l1 >= MaxVoltage || DSMRdata.voltage_l2 >= MaxVoltage || DSMRdata.voltage_l3 >= MaxVoltage ){
     if ( !bMaxV ) {
        bMaxV = true;
        StoreVoltage(true);
     }
  } else if ( bMaxV ) {
    bMaxV = false;
    StoreVoltage(false);
  }
} //ProcessMaxVoltage
#else
void PrintVarr(){}
void ProcessMaxVoltage(){}
#endif
