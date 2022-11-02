#ifdef STUB

const char T1[] =
"/ISK5\2M550T-1012\r\n"
"\r\n"
"1-3:0.2.8(50)\r\n"
"0-0:1.0.0(200923170321S)\r\n"
"0-0:96.1.1(4530303434303037323531373036363138)\r\n"
"1-0:1.8.1(002331.028*kWh)\r\n"
"1-0:1.8.2(002603.951*kWh)\r\n"
"1-0:2.8.1(000000.000*kWh)\r\n"
"1-0:2.8.2(000000.000*kWh)\r\n"
"0-0:96.14.0(0002)\r\n"
"1-0:1.7.0(00.232*kW)\r\n"
"1-0:2.7.0(00.000*kW)\r\n"
"0-0:96.7.21(00005)\r\n"
"0-0:96.7.9(00002)\r\n"
"1-0:99.97.0()\r\n"
"1-0:32.32.0(00004)\r\n"
"1-0:52.32.0(00006)\r\n"
"1-0:72.32.0(00003)\r\n"
"1-0:32.36.0(00001)\r\n"
"1-0:52.36.0(00001)\r\n"
"1-0:72.36.0(00001)\r\n"
"0-0:96.13.0()\r\n"
"1-0:32.7.0(228.0*V)\r\n"
"1-0:52.7.0(227.9*V)\r\n"
"1-0:72.7.0(232.7*V)\r\n"
"1-0:31.7.0(000*A)\r\n"
"1-0:51.7.0(000*A)\r\n"
"1-0:71.7.0(000*A)\r\n"
"1-0:21.7.0(00.041*kW)\r\n"
"1-0:41.7.0(00.149*kW)\r\n"
"1-0:61.7.0(00.040*kW)\r\n"
"1-0:22.7.0(00.000*kW)\r\n"
"1-0:42.7.0(00.000*kW)\r\n"
"1-0:62.7.0(00.000*kW)\r\n"
"0-1:24.1.0(003)\r\n"
"0-1:96.1.0(4730303339303031383330303339323138)\r\n"
"0-1:24.2.1(200923170001S)(02681.397*m3)\r\n"
"!8d36";

void handleSTUB(){
  DebugFlush();
  telegramCount++;
  DSMRdata = {};
  ParseResult<void> res;
  res = P1Parser::parse(&DSMRdata, T1, lengthof(T1), false, true);

  if (res.err)
  {
    // Parsing error, show it
    Debugln(res.fullError(T1, T1 + lengthof(T1)));
  }
  else if (!DSMRdata.all_present())
  {
    if (Verbose2) DebugTln("DSMR: Some fields are missing");
  }
  // Succesfully parsed, now process the data!
  DebugTln("Processing telegram ..");
  // Succesfully parsed, now process the data!
  if (!DSMRdata.timestamp_present)
  {
    sprintf(cMsg, "%02d%02d%02d%02d%02d%02dW\0\0"
            , (year(now()) - 2000), month(now()), day(now())
            , hour(now()), minute(now()), second(now()));
    DSMRdata.timestamp         = cMsg;
    DSMRdata.timestamp_present = true;
  }

  DSMRdata.identification ="TEST DATA";
  
  gasDelivered = modifyMbusDelivered();
  modifySmFaseInfo();

  //simulate time 
  // one +hour = 3600 sec
//  DSMRdata.timestamp = buildDateTimeString( DSMRdata.timestamp, sizeof(DSMRdata.timestamp)).c_str() );
  time_t t;
  t = epoch(DSMRdata.timestamp.c_str(), 12, false) + ( 3600 * telegramCount ) ;
  strcpy(cMsg, DSMRdata.timestamp.c_str());
  epochToTimestamp(t,cMsg , 12);
  DSMRdata.timestamp = cMsg + String("S");
  processTelegram();

  //Debugf("==>> act date/time [%s] is [%s]\r\n\n", actTimestamp, );
  
}

//==================================================================================================
/*
int16_t decodeTelegram(int len)
{
  //need to check for start
  int startChar = FindCharInArrayRev((unsigned char *)telegramLine, '/', len);
  int endChar   = FindCharInArrayRev((unsigned char *)telegramLine, '!', len);

  bool validCRCFound = false;
  if(startChar>=0)
  {
    //start found. Reset CRC calculation
    currentCRC=CRC16(0x0000, (unsigned char *) telegramLine+startChar, len-startChar);

  }
  else if(endChar>=0)
  {
    //add to crc calc
    currentCRC=CRC16(currentCRC, (unsigned char *)telegramLine+endChar, 1);

  }
  else
  {
    currentCRC=CRC16(currentCRC, (unsigned char *)telegramLine, len);
  }

  //return validCRCFound;
  return currentCRC;

} // decodeTelegram()


//==================================================================================================
unsigned int CRC16(unsigned int crc, unsigned char *buf, int len)
{
  for (int pos = 0; pos < len; pos++)
  {
    crc ^= (unsigned int)buf[pos];    // XOR byte into least sig. byte of crc

    for (int i = 8; i != 0; i--)      // Loop over each bit
    {
      if ((crc & 0x0001) != 0)        // If the LSB is set
      {
        crc >>= 1;                    // Shift right and XOR 0xA001
        crc ^= 0xA001;
      }
      else                            // Else LSB is not set
        crc >>= 1;                    // Just shift right
    }
  }

  return crc;
}
*/

#endif
