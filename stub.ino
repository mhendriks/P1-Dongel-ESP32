#ifdef STUB

// Parse static test telegram 

DECLARE_TIMER_SEC(StubTimer,  2); // try reconnecting cyclus timer

//warmtelink
const char P1_1_[]  = 
"/ISk5\2MT382-1000\r\n"
"\r\n"
"1-3:0.2.8(50)\r\n"
"0-0:1.1.0(230319144324W)\r\n"
"0-1:24.1.0(012)\r\n"
"0-1:96.1.0(303030304B414D32)\r\n"
"0-1:24.2.1(230319144324W)(31.177*m3)\r\n"
"!970c";

//normale 3 fase smr 5
const char P1_1__[] =
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

//BE met gas en water en piek waardes
const char P1_1[] =
"/FLU5\253769484_A\r\n"
"\r\n"
"0-0:96.1.4(50217)\r\n"
"0-0:96.1.1(3153414733313035323039343933)\r\n"
"0-0:1.0.0(230421165328S)\r\n"
"1-0:1.8.1(000056.913*kWh)\r\n"
"1-0:1.8.2(000184.024*kWh)\r\n"
"1-0:2.8.1(000083.503*kWh)\r\n"
"1-0:2.8.2(000015.696*kWh)\r\n"
"0-0:96.14.0(0001)\r\n"
"1-0:1.4.0(00.000*kW)\r\n"
"1-0:1.6.0(230411223000S)(04.509*kW)\r\n"
"0-0:98.1.0(0)(1-0:1.6.0)(1-0:1.6.0)()\r\n"
"1-0:1.7.0(00.000*kW)\r\n"
"1-0:2.7.0(01.739*kW)\r\n"
"1-0:21.7.0(00.188*kW)\r\n"
"1-0:41.7.0(00.000*kW)\r\n"
"1-0:61.7.0(00.000*kW)\r\n"
"1-0:22.7.0(00.000*kW)\r\n"
"1-0:42.7.0(00.000*kW)\r\n"
"1-0:62.7.0(01.928*kW)\r\n"
"1-0:32.7.0(245.3*V)\r\n"
"1-0:52.7.0(000.0*V)\r\n"
"1-0:72.7.0(246.8*V)\r\n"
"1-0:31.7.0(001.00*A)\r\n"
"1-0:51.7.0(007.74*A)\r\n"
"1-0:71.7.0(007.97*A)\r\n"
"0-0:96.3.10(1)\r\n"
"0-0:17.0.0(999.9*kW)\r\n"
"1-0:31.4.0(999*A)\r\n"
"0-0:96.13.0()\r\n"
"0-1:24.1.0(007)\r\n"
"0-1:96.1.1(3853455430303030383337393634)\r\n"
"0-1:24.2.1(230421165102S)(00003.830*m3)\r\n"
"0-2:24.1.0(003)\r\n"
"0-2:96.1.1(37464C4F32313233303135313839)\r\n"
"0-2:24.4.0(1)\r\n"
"0-2:24.2.3(230421164959S)(00107.812*m3)\r\n"
"!438b";

void _handleSTUB(){
  if (DUE(StubTimer)) {
  DebugFlush();
  DSMRdata = {};
  ParseResult<void> res;
  res = P1Parser::parse(&DSMRdata, P1_1, lengthof(P1_1), false, false);

  if (res.err)
  {
    // Parsing error, show it
    Debugln(res.fullError(P1_1, P1_1 + lengthof(P1_1)));
    return;
  }

  DSMRdata.identification ="TEST DATA";
  CapTelegram = P1_1;
  processSlimmemeter();
  
  }
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
