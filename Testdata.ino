#if defined(HAS_NO_SLIMMEMETER)

#define   MAXLINELENGTH     128   // longest normal line is 47 char (+3 for \r\n\0)

enum runStates { SInit, SMonth, SDay, SHour, SNormal };
enum runStates runMode = SNormal;

char        telegramLine[MAXLINELENGTH] = "";
char        telegram[1000] = "";
uint16_t    currentCRC; 
int16_t     calcCRC;
uint32_t    actInterval = 5, nextMinute = 0, nextESPcheck = 0, nextGuiUpdate;
int8_t      State;
int16_t     actSec, actMinute, actHour, actDay, actMonth, actYear, actSpeed;
char        actDSMR[3] = "40", savDSMR[3] = "40";
double      ED_T1=0, ED_T2=0, ER_T1=0, ER_T2=0, V_l1=0, V_l2=0, V_l3=0, C_l1=0, C_l2=0, C_l3=0;
uint8_t     ETariffInd=1;
float       PDelivered, PReturned;
float       IPD_l1, IPD_l2, IPD_l3, IPR_l1, IPR_l2, IPR_l3;
float       GDelivered;
bool        forceBuildRingFiles = false;
int16_t     forceBuildRecs;

#define   TELEGRAM_INTERVAL   5 //seconds
DECLARE_TIMER_SEC(telegramTimer, TELEGRAM_INTERVAL);

//==================================================================================================
void handleTestdata()
{
  time_t nt;
  int16_t slot;
 
  if (!forceBuildRingFiles && ( !DUE( telegramTimer) ) )  return;   // not yet time for new Telegram

//  DebugTf("Time for a new Telegram ..");
  if (forceBuildRingFiles)
  {
    switch(runMode) {
      case SInit:   runMode = SMonth;
                    forceBuildRecs = (_NO_MONTH_SLOTS_ *2) +5;
                    runMode = SMonth;
                    nt = epoch(actTimestamp, sizeof(actTimestamp), true);
                    break;
                    
      case SMonth:  if (forceBuildRecs <= 0)
                    {
                      forceBuildRecs = (_NO_DAY_SLOTS_ *2) +4;
                      nt = epoch(actTimestamp, sizeof(actTimestamp), true);
                      runMode = SDay;
                      break;
                    }    
                    nt = now() + (15 * SECS_PER_DAY);
                    DebugTf("Force build RING file for months -> rec[%2d]\r\n\n", forceBuildRecs);
                    forceBuildRecs--;
                    break;
                    
      case SDay:    if (forceBuildRecs <= 0)
                    {
                      forceBuildRecs = (_NO_HOUR_SLOTS_ * 2) +5;
                      nt = epoch(actTimestamp, sizeof(actTimestamp), true);
                      runMode = SHour;
                      break;
                    }
                    nt = now() + (SECS_PER_DAY / 2);
                    DebugTf("Force build RING file for days -> rec[%2d]\r\n\n", forceBuildRecs);
                    forceBuildRecs--;
                    break;
                    
      case SHour:   if (forceBuildRecs <= 0)
                    {
                      forceBuildRingFiles= false;
                      nt = epoch(actTimestamp, sizeof(actTimestamp), true);
                      DebugTln("Force build RING file back to normal operation\r\n\n");
                      runMode = SNormal;
                      break;
                    }
                    nt = now() + (SECS_PER_HOUR / 2);
                    //epochToTimestamp(nt, newTimestamp, sizeof(newTimestamp));
                    //slot = timestampToHourSlot(newTimestamp, sizeof(newTimestamp));
                    DebugTf("Force build RING file for hours -> rec[%2d]\r\n\n", forceBuildRecs);
                    forceBuildRecs--;
                    break;
                    
      default:      runMode = SNormal;
                    forceBuildRingFiles = false;

    } // switch()
  }
  else  // normal operation mode
  {
    nt = now() + (SECS_PER_HOUR / 2);
  }

  epochToTimestamp(nt, newTimestamp, sizeof(newTimestamp));
  Debugf("==>> new date/time [%s] is [%s]\r\n", newTimestamp, buildDateTimeString(newTimestamp, sizeof(newTimestamp)).c_str());
  
  updateMeterValues(SNormal);
  
  currentCRC = 0;
  memset(telegram,0,sizeof(telegram));
  
  for (int16_t line = 0; line < 38; line++) {
    yield();
    int16_t len = buildTelegram40(line, telegramLine);  // also: prints to DSMRsend
    calcCRC = decodeTelegram(len);
  } 
  snprintf(cMsg, sizeof(cMsg), "!%04X\r\n\r\n", (calcCRC & 0xFFFF));
  if (Verbose2) Debug(cMsg);
  strConcat(telegram, sizeof(telegram), cMsg);

  DebugFlush();
  telegramCount++;

  DSMRdata = {};
  ParseResult<void> res = P1Parser::parse(&DSMRdata, telegram, lengthof(telegram));
  if (res.err) 
  {
    // Parsing error, show it
    Debugln(res.fullError(telegram, telegram + lengthof(telegram)));
  } 
  else if (!DSMRdata.all_present()) 
  {
    if (Verbose2) DebugTln("DSMR: Some fields are missing");
  } 
  // Succesfully parsed, now process the data!

  processTelegram();
  
  Debugf("==>> act date/time [%s] is [%s]\r\n\n", actTimestamp, buildDateTimeString(actTimestamp, sizeof(actTimestamp)).c_str());

} // handleTestdata()


//==================================================================================================
int16_t buildTelegram40(int16_t line, char telegramLine[]) 
{
  int16_t len = 0;

  float val;

  switch (line) {
                                     //XMX5LGBBLB2410065887
    case 0:   sprintf(telegramLine, "/XMX5LGBBLB2410065887\r\n");
              break;
    case 1:   sprintf(telegramLine, "\r\n");    
              break;
    case 2:   sprintf(telegramLine, "1-3:0.2.8(50)\r\n");
              break;
    case 3:   sprintf(telegramLine, "0-0:1.0.0(%12.12sS)\r\n", newTimestamp);
              break;
    case 4:   sprintf(telegramLine, "0-0:96.1.1(4530303336303000000000000000000040)\r\n", val);
              break;
    case 5:   // Energy Delivered
              sprintf(telegramLine, "1-0:1.8.1(%s*kWh)\r\n", Format(ED_T1, 10, 3).c_str());
              break;
    case 6:   sprintf(telegramLine, "1-0:1.8.2(%s*kWh)\r\n", Format(ED_T2, 10, 3).c_str());
              break;
    case 7:   // Energy Returned
              sprintf(telegramLine, "1-0:2.8.1(%s*kWh)\r\n", Format(ER_T1, 10, 3).c_str());
              break;
    case 8:   sprintf(telegramLine, "1-0:2.8.2(%s*kWh)\r\n", Format(ER_T2, 10, 3).c_str());
              break;
    case 9:   // Tariff indicator electricity
              sprintf(telegramLine, "0-0:96.14.0(%04d)\r\n", ETariffInd);
              break;
    case 10:  // Actual electricity power delivered (+P) in 1 Watt resolution
              sprintf(telegramLine, "1-0:1.7.0(%s*kW)\r\n", Format(PDelivered, 6, 2).c_str());
              break;
    case 11:  // Actual electricity power received (-P) in 1 Watt resolution
              sprintf(telegramLine, "1-0:2.7.0(%s*kW)\r\n", Format(PReturned, 6, 2).c_str());
              break;
    case 12:  // Number of power failures in any phase
              sprintf(telegramLine, "0-0:96.7.21(00010)\r\n", val);
              break;
    case 13:  // Number of long power failures in any phase
              sprintf(telegramLine, "0-0:96.7.9(00000)\r\n", val);
              break;
    case 14:  // Power Failure Event Log (long power failures)
              sprintf(telegramLine, "1-0:99.97.0(0)(0-0:96.7.19)\r\n", val);
              break;
    case 15:  // Number of voltage sags in phase L1
              sprintf(telegramLine, "1-0:32.32.0(00002)\r\n", val);
              break;
    case 16:  // Number of voltage sags in phase L2 (polyphase meters only)
              sprintf(telegramLine, "1-0:52.32.0(00003)\r\n", val);
              break;
    case 17:  // Number of voltage sags in phase L3 (polyphase meters only)
              sprintf(telegramLine, "1-0:72.32.0(00003)\r\n", val);
              break;
    case 18:  // Number of voltage swells in phase L1
              sprintf(telegramLine, "1-0:32.36.0(00000)\r\n", val);
              break;
    case 19:  // Number of voltage swells in phase L2
              sprintf(telegramLine, "1-0:52.36.0(00000)\r\n", val);
              break;
    case 20:  // Number of voltage swells in phase L3
              sprintf(telegramLine, "1-0:72.36.0(00000)\r\n", val);
              break;
    case 21:  // Text message max 2048 characters
              sprintf(telegramLine, "0-0:96.13.0()\r\n", val);
              break;
    case 22:  // Instantaneous voltage L1 in 0.1V resolution
              sprintf(telegramLine, "1-0:32.7.0(%03d.0*V)\r\n", (240 + random(-3,3)));
              break;
    case 23:  // Instantaneous voltage L1 in 0.1V resolution
              sprintf(telegramLine, "1-0:52.7.0(%03d.0*V)\r\n", (238 + random(-3,3)));
              break;
    case 24:  // Instantaneous voltage L1 in 0.1V resolution
              sprintf(telegramLine, "1-0:72.7.0(%03d.0*V)\r\n", (236 + random(-3,3)));
              break;
    case 25:  // Instantaneous current L1 in A resolution
              sprintf(telegramLine, "1-0:31.7.0(%03d*A)\r\n", random(0,4));
              break;
    case 26:  // Instantaneous current L2 in A resolution
              sprintf(telegramLine, "1-0:51.7.0(%03d*A)\r\n",  random(0,4));
              break;
    case 27:  // Instantaneous current L3 in A resolution
              sprintf(telegramLine, "1-0:71.7.0(000*A)\r\n", val);
              break;
    case 28:  // Instantaneous active power L1 (+P) in W resolution
              sprintf(telegramLine, "1-0:21.7.0(%s*kW)\r\n", Format(IPD_l1, 6, 3).c_str());
              break;
    case 29:  // Instantaneous active power L2 (+P) in W resolution
              sprintf(telegramLine, "1-0:41.7.0(%s*kW)\r\n", Format(IPD_l2, 6, 3).c_str());
              break;
    case 30:  // Instantaneous active power L3 (+P) in W resolution
              sprintf(telegramLine, "1-0:61.7.0(%s*kW)\r\n", Format(IPD_l3, 6, 3).c_str());
              break;
    case 31:  // Instantaneous active power L1 (-P) in W resolution
              sprintf(telegramLine, "1-0:22.7.0(%s*kW)\r\n", Format(IPR_l1, 6, 3).c_str());
              break;
    case 32:  // Instantaneous active power L2 (-P) in W resolution
              sprintf(telegramLine, "1-0:42.7.0(%s*kW)\r\n", Format(IPR_l2, 6, 3).c_str());
              break;
    case 33:  // Instantaneous active power L3 (-P) in W resolution
              sprintf(telegramLine, "1-0:62.7.0(%s*kW)\r\n", Format(IPR_l3, 6, 3).c_str());
              break;
    case 34:  // Gas Device-Type
              sprintf(telegramLine, "0-1:24.1.0(003)\r\n", val);
              break;
    case 35:  // Equipment identifier (Gas)
              sprintf(telegramLine, "0-1:96.1.0(4730303339303031363532303530323136)\r\n", val);
              break;
    case 36:  // Last 5-minute value (temperature converted), gas delivered to client
              // in m3, including decimal values and capture time (Note: 4.x spec has
              sprintf(telegramLine, "0-1:24.2.1(%02d%02d%02d%02d%02d01S)(%s*m3)\r\n", (year() - 2000), month(), day(), hour(), minute(), 
                                                                            Format(GDelivered, 9, 3).c_str());
              break;
    case 37:  sprintf(telegramLine, "!xxxx\r\n");   
              break;
              
  } // switch(line)

  if (line < 37) {
    if (Verbose2) Debug(telegramLine); 
    strConcat(telegram, sizeof(telegram), telegramLine);
  }

  for(len = 0; len < MAXLINELENGTH, telegramLine[len] != '\0'; len++) {}    
  
  return len;

} // buildTelegram40()


//==================================================================================================
int16_t buildTelegram30(int16_t line, char telegramLine[]) 
{
/*
**  /KMP5 KA6U001585575011                - Telegram begin-marker + manufacturer + serial number
**  
**  0-0:96.1.1(204B413655303031353835353735303131)    -  Serial number in hex
**  1-0:1.8.1(08153.408*kWh)                          -  +T1: Energy input, low tariff (kWh)
**  1-0:1.8.2(05504.779*kWh)                          -  +T2: Energy input, normal tariff (kWh)
**  1-0:2.8.1(00000.000*kWh)                          -  -T3: Energy output, low tariff (kWh)
**  1-0:2.8.2(00000.000*kWh)                          -  -T4: Energy output, normal tariff (kWh)
**  0-0:96.14.0(0002)                                 -  Current tariff (1=low, 2=normal)
**  1-0:1.7.0(0000.30*kW)                             -  Actual power input (kW)
**  1-0:2.7.0(0000.00*kW)                             -  Actual power output (kW)
**  0-0:17.0.0(999*A)                                 -  Max current per phase (999=no max)
**  0-0:96.3.10(1)                                    -  Switch position
**  0-0:96.13.1()                                     -  Message code
**  0-0:96.13.0()                                     -  Message text
**  0-1:24.1.0(3)                                     -  Attached device type identifier
**  0-1:96.1.0(3238313031353431303031333733353131)    -  Serial number of gas meter
**  0-1:24.3.0(190718190000)(08)(60)(1)(0-1:24.2.1)(m3) -  Time of last gas meter update
**  (04295.190)                                       -  Gas meter value (m³)
**  0-1:24.4.0(1)                                     -  Gas valve position
**  !                                                 -  Telegram end-marker
**  
*/
//==================================================================================================
  int16_t len = 0;

  float val;

  switch (line) {
                                //KMP5 KA6U001585575011
    case 0:   sprintf(telegramLine, "/KMP5 KA6U001585575011\r\n");
              break;
    case 1:   sprintf(telegramLine, "\r\n");    
              break;
    case 2:   sprintf(telegramLine, "0-0:96.1.1(4530303336303000000000000000000000)\r\n", val);
              break;
    case 3:   // Energy Delivered
              sprintf(telegramLine, "1-0:1.8.1(%s*kWh)\r\n", Format(ED_T1, 10, 3).c_str());
              break;
    case 4:   sprintf(telegramLine, "1-0:1.8.2(%s*kWh)\r\n", Format(ED_T2, 10, 3).c_str());
              break;
    case 5:   // Energy Returned
              sprintf(telegramLine, "1-0:2.8.1(%s*kWh)\r\n", Format(ER_T1, 10, 3).c_str());
              break;
    case 6:   sprintf(telegramLine, "1-0:2.8.2(%s*kWh)\r\n", Format(ER_T2, 10, 3).c_str());
              break;
    case 7:   // Tariff indicator electricity
              sprintf(telegramLine, "0-0:96.14.0(%04d)\r\n", ETariffInd);
              break;
    case 8:   // Actual electricity power delivered (+P) in 1 Watt resolution
              sprintf(telegramLine, "1-0:1.7.0(%s*kW)\r\n", Format(PDelivered, 6, 2).c_str());
              break;
    case 9:   // Actual electricity power received (-P) in 1 Watt resolution
              sprintf(telegramLine, "1-0:2.7.0(%s*kW)\r\n", Format(PReturned, 6, 2).c_str());
              break;
    case 10:  // Max current per phase (999=no max)
              sprintf(telegramLine, "0-0:17.0.0(016*A)\r\n", val);
              break;
    case 11:  // Switch position (?)
              sprintf(telegramLine, "0-0:96.3.10(1)\r\n", val);
              break;
    case 12:  // Text message code
              sprintf(telegramLine, "0-0:96.13.1()\r\n", val);
              break;
    case 13:  // Text message text
              sprintf(telegramLine, "0-0:96.13.0()\r\n", val);
              break;
    case 14:  // Gas Device-Type
              sprintf(telegramLine, "0-1:24.1.0(3)\r\n", val);
              break;
    case 15:  // Equipment identifier (Gas)
              sprintf(telegramLine, "0-1:96.1.0(4730303339303031363500000000000000)\r\n", val);
              break;
    case 16:  // Last 5-minute value (temperature converted), gas delivered to client
              // in m3, including decimal values and capture time 
              sprintf(telegramLine, "0-1:24.3.0(%02d%02d%02d%02d%02d00)(08)(60)(1)(0-1:24.2.1)(m3)\r\n", (year() - 2000), month(), day(), hour(), minute());
              break;
    case 17:  sprintf(telegramLine, "(%s)\r\n", Format(GDelivered, 9, 3).c_str());
              break;
    case 18:  // Gas valve position
              sprintf(telegramLine, "0-1:24.4.0(1)\r\n", val);
              break;
    case 19:  sprintf(telegramLine, "!\r\n\r\n");     // just for documentation 
              break;
              
  } // switch(line)

  if (line < 19) {
    if (Verbose2) Debug(telegramLine); 
    strConcat(telegram, sizeof(telegram), telegramLine);
  }

  for(len = 0; len < MAXLINELENGTH, telegramLine[len] != '\0'; len++) {}    
  
  return len;

} // buildTelegram30()



//==================================================================================================
void updateMeterValues(uint8_t period) 
{
  float  Factor = 1.098;
  String wsString = "";
  /*
  switch(period) {
    case SMonth:  Factor = 30.0 * 24.0; break;
    case SDay:    Factor = 24.0;        break;
    case SHour:   Factor = 1.0;         break;
    default:      Factor = 1.0;
  }
  **/
  ED_T1      += (float)((random(200,2200) / 3600000.0) * actInterval) * Factor;
  ED_T2      += (float)((random(100,9000) / 3600000.0) * actInterval) * Factor;
  if (hour(actT) >= 4 && hour(actT) <= 20) {
    ER_T1      += (float)((random(0,400)  / 3600000.0) * actInterval) * Factor;
    ER_T2      += (float)((random(0,200)  / 3600000.0) * actInterval) * Factor;
    ETariffInd  = 1;
  } else {
    ETariffInd  = 2;
  }
  GDelivered += (float)(random(2,25) / 10000.0) * Factor;     // Gas Delevered
  V_l1        = (float)(random(220,240) * 1.01);      // Voltages
  V_l2        = (float)(random(220,240) * 1.02);
  V_l3        = (float)(random(220,240) * 1.03);
  C_l1        = (float)(random(1,20) * 1.01);          // Current
  C_l2        = (float)(random(1,15) * 1.02);
  C_l3        = (float)(random(1,10) * 1.03);   
  IPD_l1      = (float)(random(1,1111) * 0.001102);
  IPD_l2      = (float)(random(1,892)  * 0.001015);
  IPD_l3      = (float)(random(1,773)  * 0.001062);
  if (hour(actT) >= 4 && hour(actT) <= 20) {
    IPR_l1    = (float)(random(1,975) * 0.01109);
    IPR_l2    = (float)(random(1,754) * 0.01031);
    IPR_l3    = (float)(random(1,613) * 0.01092);
    
  } else {    // 's-nachts geen opwekking van energy!
    IPR_l1    = 0.0;
    IPR_l2    = 0.0;
    IPR_l3    = 0.0;
  }
  PDelivered  = (float)(IPD_l1 + IPD_l2 + IPD_l3) / 1.0;       // Power Delivered
  PReturned   = (float)(IPR_l1 + IPR_l2 + IPR_l3) / 1.0;       // Power Returned

  if (Verbose2) Debugf("l1[%5d], l2[%5d], l3[%5d] ==> l1+l2+l3[%9.3f]\n"
                                                          , (int)(IPD_l1 * 1000)
                                                          , (int)(IPD_l2 * 1000)
                                                          , (int)(IPD_l3 * 1000)
                                                          , PDelivered);
  
} // updateMeterValues()


//==================================================================================================
String Format(double x, int len, int d) 
{
  String r;
//  r.reserve(len);
  int rl;
  
  r = String(x, d);
//Debugf("Format(%s, %d, %d)\n", r.c_str(), len, d);
  while (r.length() < len) r = "0" + r;
  rl = r.length();
  if (rl > len) {
    return r.substring((rl - len));
  }
  return r;

} // Format()



//==================================================================================================
int FindCharInArrayRev(unsigned char array[], char c, int len) 
{
  for (int16_t i = len - 1; i >= 0; i--) {
    if (array[i] == c) {
      return i;
    }
  }
  return -1;
}


//==================================================================================================
int16_t decodeTelegram(int len) 
{
  //need to check for start
  int startChar = FindCharInArrayRev((unsigned char*)telegramLine, '/', len);
  int endChar   = FindCharInArrayRev((unsigned char*)telegramLine, '!', len);
  
  bool validCRCFound = false;
  if(startChar>=0) {
    //start found. Reset CRC calculation
    currentCRC=CRC16(0x0000,(unsigned char *) telegramLine+startChar, len-startChar);
    
  } else if(endChar>=0) {
    //add to crc calc 
    currentCRC=CRC16(currentCRC,(unsigned char*)telegramLine+endChar, 1);
  
  } else {
    currentCRC=CRC16(currentCRC, (unsigned char*)telegramLine, len);
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

    for (int i = 8; i != 0; i--) {    // Loop over each bit
      if ((crc & 0x0001) != 0) {      // If the LSB is set
        crc >>= 1;                    // Shift right and XOR 0xA001
        crc ^= 0xA001;
      }
      else                            // Else LSB is not set
        crc >>= 1;                    // Just shift right
    }
  }

  return crc;
}

#endif
