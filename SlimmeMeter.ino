/*
***************************************************************************  
**  Program  : handleSlimmeMeter - part of DSMRloggerAPI
**  Version  : v4.2.1
**
**  Copyright (c) 2021 Willem Aandewiel / Martijn Hendriks
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/                           

void SetupSMRport(){
  Serial1.end();
  delay(100); //give it some time
  DebugT(F("P1 serial set to ")); 
  if(bPre40){
    Serial1.begin(9600, SERIAL_7E1, RXP1, TXP1, true); //p1 serial input
    slimmeMeter.doChecksum(false);
    Debugln(F("9600 baud / 7E1"));  
  } else {
    Serial1.begin(115200, SERIAL_8N1, RXP1, TXP1, true); //p1 serial input
    slimmeMeter.doChecksum(true);
    Debugln(F("115200 baud / 8N1"));
  }
  delay(100); //give it some time
}

struct showValues {
  template<typename Item>
  void apply(Item &i) {
    if (i.present()) 
    {
      TelnetStream.print(F("showValues: "));
      TelnetStream.print(Item::name);
      TelnetStream.print(F(": "));
      TelnetStream.print(i.val());
      TelnetStream.println(Item::unit());
    //} else 
    //{
    //  TelnetStream.print(F("<no value>"));
    }
  }
};

//==================================================================================
void handleSlimmemeter()
{
  //DebugTf("showRaw (%s)\r\n", showRaw ?"true":"false");
#ifdef STUB
  return; //escape when stub is active
#endif
    slimmeMeter.loop();
    if (slimmeMeter.available()) {
      ToggleLED(LED_ON);
      CapTelegram = slimmeMeter.raw(); //capture last telegram
      if (showRaw) {
        //-- process telegrams in raw mode
        Debugf("Telegram Raw (%d)\n/%s!%x\n", slimmeMeter.raw().length(), slimmeMeter.raw().c_str(), slimmeMeter.GetCRC() ); 
        showRaw = false; //only 1 reading
      } else processSlimmemeter();
      ToggleLED(LED_OFF);
    } //available
} // handleSlimmemeter()

//==================================================================================
void processSlimmemeter()
{
    telegramCount++;
    
    // Voorbeeld: [21:00:11][   9880/  8960] loop        ( 997): read telegram [28] => [140307210001S]
    if (!bHideP1Log) {
      Debugln(F("\r\nP [Time----][FreeHeap/mBlck][Function----(line):"));
      DebugTf("telegramCount=[%d] telegramErrors=[%d] bufferlength=[%d]\r\n", telegramCount, telegramErrors,slimmeMeter.raw().length());
    }
        
    DSMRdata = {};
    String    DSMRerror;
        
    if (slimmeMeter.parse(&DSMRdata, &DSMRerror))   // Parse succesful, print result
    {
      if (DSMRdata.identification_present)
      {
        //--- this is a hack! The identification can have a backslash in it
        //--- that will ruin javascript processing :-(
        for(int i=0; i<DSMRdata.identification.length(); i++)
        {
          if (DSMRdata.identification[i] == '\\') DSMRdata.identification[i] = '=';
          yield();
        }
      }

//      DebugT("CRC:");Debugln(slimmeMeter.GetCRC(), HEX);
//      TelnetStream.print("CRC:");TelnetStream.println(slimmeMeter.GetCRC(), HEX);
      CRCTelegram = slimmeMeter.GetCRC();

      if (DSMRdata.p1_version_be_present)
      {
        DSMRdata.p1_version = DSMRdata.p1_version_be;
        DSMRdata.p1_version_be_present  = false;
        DSMRdata.p1_version_present     = true;
        DSMR_NL = false;
      }

      modifySmFaseInfo();
#ifndef USE_NTP_TIME
  if (!DSMRdata.timestamp_present) { 
#endif
        if (Verbose2) DebugTln(F("NTP Time set"));
      if ( getLocalTime(&tm) ) {
          DSTactive = tm.tm_isdst;
          sprintf(cMsg, "%02d%02d%02d%02d%02d%02d%s\0\0", (tm.tm_year -100 ), tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,DSTactive?"S":"W");
      } else {
        strCopy(cMsg, sizeof(cMsg), actTimestamp);
        LogFile("timestamp = old time",true);
      }
        DSMRdata.timestamp         = cMsg;
        DSMRdata.timestamp_present = true;

#ifndef USE_NTP_TIME
  } //!timestamp present
#endif  
      //-- handle mbus delivered values
      gasDelivered = modifyMbusDelivered();
      
      processTelegram();
      if (Verbose2) DSMRdata.applyEach(showValues());
    } 
    else // Parser error, print error
    {
      telegramErrors++;
      DebugTf("Parse error\r\n%s\r\n\r\n", DSMRerror.c_str());
      slimmeMeter.clear(); //on errors clear buffer
    }
  
} // handleSlimmeMeter()

//==================================================================================
void processTelegram(){
//  DebugTf("Telegram[%d]=>DSMRdata.timestamp[%s]\r\n", telegramCount, DSMRdata.timestamp.c_str());  
                                                    
//  strcpy(newTimestamp, DSMRdata.timestamp.c_str()); 

  newT = epoch(DSMRdata.timestamp.c_str(), DSMRdata.timestamp.length(), true); // update system time
//  actT = epoch(actTimestamp, strlen(actTimestamp), false); //niet nodig ... is al gezet initieel en in vorige processing

      DebugTf("actHour[%02d] -- newHour[%02d]\r\n", hour(actT), hour(newT));  

  // has the hour changed
  if (     ( hour(actT) != hour(newT) ) || P1Status.FirstUse )  {
//    DebugTf("actHour[%02d] -- newHour[%02d]\r\n", hour(actT), hour(newT));  
    writeRingFiles();
  }

  if ( DUE(publishMQTTtimer) ) sendMQTTDataEV();

  if ( bRawPort ) {
    ws_raw.print("/" + CapTelegram + "!" ); //print telegram to dongle port
    ws_raw.println(CRCTelegram, HEX);
  }

  //update actual time
  strCopy(actTimestamp, sizeof(actTimestamp), DSMRdata.timestamp.c_str()); 
  actT = newT; //epoch(actTimestamp, strlen(actTimestamp), true);   // update system time
  
} // processTelegram()

//==================================================================================
void modifySmFaseInfo()
{
  if (!settingSmHasFaseInfo)
  {
        if (DSMRdata.power_delivered_present && !DSMRdata.power_delivered_l1_present)
        {
          DSMRdata.power_delivered_l1 = DSMRdata.power_delivered;
          DSMRdata.power_delivered_l1_present = true;
          DSMRdata.power_delivered_l2_present = true;
          DSMRdata.power_delivered_l3_present = true;
        }
        if (DSMRdata.power_returned_present && !DSMRdata.power_returned_l1_present)
        {
          DSMRdata.power_returned_l1 = DSMRdata.power_returned;
          DSMRdata.power_returned_l1_present = true;
          DSMRdata.power_returned_l2_present = true;
          DSMRdata.power_returned_l3_present = true;
        }
  } // No Fase Info
  
} //  modifySmFaseInfo()

//==================================================================================
float modifyMbusDelivered()
{
  float tmpGasDelivered = 0;
  
  if ( DSMRdata.mbus1_device_type == MBUS_TYPE )  { //gasmeter
    if (DSMRdata.mbus1_delivered_ntc_present) 
        DSMRdata.mbus1_delivered = DSMRdata.mbus1_delivered_ntc;
  else if (DSMRdata.mbus1_delivered_dbl_present) 
        DSMRdata.mbus1_delivered = DSMRdata.mbus1_delivered_dbl;
  DSMRdata.mbus1_delivered_present     = true;
  DSMRdata.mbus1_delivered_ntc_present = false;
  DSMRdata.mbus1_delivered_dbl_present = false;
//  if (settingMbus1Type > 0) DebugTf("mbus1_delivered [%.3f]\r\n", (float)DSMRdata.mbus1_delivered);
//  if ( (settingMbus1Type == 3) && (DSMRdata.mbus1_device_type == 3) )
  
    tmpGasDelivered = (float)(DSMRdata.mbus1_delivered * 1.0);
    gasDeliveredTimestamp = DSMRdata.mbus1_delivered.timestamp;
//    DebugTf("gasDelivered .. [%.3f]\r\n", tmpGasDelivered);
    mbusGas = 1;
  } else if ( DSMRdata.mbus2_device_type == MBUS_TYPE ){ //gasmeter
    if (DSMRdata.mbus2_delivered_ntc_present) DSMRdata.mbus2_delivered = DSMRdata.mbus2_delivered_ntc;
    else if (DSMRdata.mbus2_delivered_dbl_present) DSMRdata.mbus2_delivered = DSMRdata.mbus2_delivered_dbl;
    DSMRdata.mbus2_delivered_present     = true;
    DSMRdata.mbus2_delivered_ntc_present = false;
    DSMRdata.mbus2_delivered_dbl_present = false;
  //  if (settingMbus2Type > 0) DebugTf("mbus2_delivered [%.3f]\r\n", (float)DSMRdata.mbus2_delivered);
  //  if ( (settingMbus2Type == 3) && (DSMRdata.mbus2_device_type == 3) )
      tmpGasDelivered = (float)(DSMRdata.mbus2_delivered * 1.0);
      gasDeliveredTimestamp = DSMRdata.mbus2_delivered.timestamp;
  //    DebugTf("gasDelivered .. [%.3f]\r\n", tmpGasDelivered);
    mbusGas = 2;
  } else  if ( (DSMRdata.mbus3_device_type == MBUS_TYPE) ){ //gasmeter
    if (DSMRdata.mbus3_delivered_ntc_present) DSMRdata.mbus3_delivered = DSMRdata.mbus3_delivered_ntc;
    else if (DSMRdata.mbus3_delivered_dbl_present) DSMRdata.mbus3_delivered = DSMRdata.mbus3_delivered_dbl;
    DSMRdata.mbus3_delivered_present     = true;
    DSMRdata.mbus3_delivered_ntc_present = false;
    DSMRdata.mbus3_delivered_dbl_present = false;
  //  if (settingMbus3Type > 0) DebugTf("mbus3_delivered [%.3f]\r\n", (float)DSMRdata.mbus3_delivered);
  //  if ( (settingMbus3Type == 3) && (DSMRdata.mbus3_device_type == 3) )
      tmpGasDelivered = (float)(DSMRdata.mbus3_delivered * 1.0);
      gasDeliveredTimestamp = DSMRdata.mbus3_delivered.timestamp;
  //    DebugTf("gasDelivered .. [%.3f]\r\n", tmpGasDelivered);
    mbusGas = 3;
  } else if ( (DSMRdata.mbus4_device_type == MBUS_TYPE) ){ //gasmeter
    if (DSMRdata.mbus4_delivered_ntc_present) DSMRdata.mbus4_delivered = DSMRdata.mbus4_delivered_ntc;
    else if (DSMRdata.mbus4_delivered_dbl_present) DSMRdata.mbus4_delivered = DSMRdata.mbus4_delivered_dbl;
    DSMRdata.mbus4_delivered_present     = true;
    DSMRdata.mbus4_delivered_ntc_present = false;
    DSMRdata.mbus4_delivered_dbl_present = false;
  //  if (settingMbus4Type > 0) DebugTf("mbus4_delivered [%.3f]\r\n", (float)DSMRdata.mbus4_delivered);
  //  if ( (settingMbus4Type == 3) && (DSMRdata.mbus4_device_type == 3) )
      tmpGasDelivered = (float)(DSMRdata.mbus4_delivered * 1.0);
      gasDeliveredTimestamp = DSMRdata.mbus4_delivered.timestamp;
  //    DebugTf("gasDelivered .. [%.3f]\r\n", tmpGasDelivered);
    mbusGas = 4;
  }

  return tmpGasDelivered;
    
} //  modifyMbusDelivered()


/***************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the
* following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
* 
***************************************************************************/
