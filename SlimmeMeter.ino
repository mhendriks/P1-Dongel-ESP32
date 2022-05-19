/*
***************************************************************************  
**  Program  : handleSlimmeMeter - part of DSMRloggerAPI
**  Version  : v4.0.0
**
**  Copyright (c) 2021 Willem Aandewiel / Martijn Hendriks
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/  
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

#if !defined(HAS_NO_SLIMMEMETER)
//==================================================================================
void handleSlimmemeter()
{

#ifndef HAS_NO_SLIMMEMETER
  return;
#endif
  //DebugTf("showRaw (%s)\r\n", showRaw ?"true":"false");
    if (slimmeMeter.available()) {
      if (LEDenabled) digitalWrite(LED, !digitalRead(LED)); //toggle LED when telegram available
      TelegramRaw = slimmeMeter.raw(); //gelijk opslaan want async update
      if ( showRaw || JsonRaw ) {
        //-- process telegrams in raw mode
        Debugf("Telegram Raw (%d)\n%s\n" , TelegramRaw.length(),TelegramRaw.c_str()); 
        if (JsonRaw) sendJsonBuffer(TelegramRaw.c_str());
        showRaw = false; //only 1 reading
        JsonRaw = false;
      } 
      else processSlimmemeter();
//      BlinkLED(); //laat succesvolle leesactie zien
      if (LEDenabled) digitalWrite(LED, !digitalRead(LED)); //toggle LED when telegram available
    } //available
} // handleSlimmemeter()

//==================================================================================
void processSlimmemeter()
{
  slimmeMeter.loop();
    telegramCount++;
    
    // Voorbeeld: [21:00:11][   9880/  8960] loop        ( 997): read telegram [28] => [140307210001S]
    Debugln(F("\r\n[Time----][FreeHeap/mBlck][Function----(line):"));
    DebugTf("telegramCount=[%d] telegramErrors=[%d] bufferlength=[%d]\r\n", telegramCount, telegramErrors,slimmeMeter.raw().length());

        
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
          if (DSMRdata.p1_version_be_present)
      {
        DSMRdata.p1_version = DSMRdata.p1_version_be;
        DSMRdata.p1_version_be_present  = false;
        DSMRdata.p1_version_present     = true;
        DSMR_NL = false;
      }

      modifySmFaseInfo();

#ifdef USE_NTP_TIME
      if (!DSMRdata.timestamp_present)                        //USE_NTP
      {                                                       //USE_NTP
        sprintf(cMsg, "%02d%02d%02d%02d%02d%02dW\0\0"         //USE_NTP
                        , (year() - 2000), month(), day()     //USE_NTP
                        , hour(), minute(), second());        //USE_NTP
        DSMRdata.timestamp         = cMsg;                    //USE_NTP
        DSMRdata.timestamp_present = true;                    //USE_NTP
      }                                                       //USE_NTP
#endif
      //-- handle mbus delivered values
      gasDelivered = modifyMbusDelivered();
      
      processTelegram();
      if (Verbose2) DSMRdata.applyEach(showValues());

    } 
    else                  // Parser error, print error
    {
      telegramErrors++;
      DebugTf("Parse error\r\n%s\r\n\r\n", DSMRerror.c_str());
      slimmeMeter.clear(); //on errors clear buffer
    }
  
} // handleSlimmeMeter()

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
  
  if ( DSMRdata.mbus1_device_type == 3 )  { //gasmeter
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
  }
  
  if ( DSMRdata.mbus2_device_type == 3 ){ //gasmeter
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
  }

  if ( (DSMRdata.mbus3_device_type == 3) ){ //gasmeter
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
  }

  if ( (DSMRdata.mbus4_device_type == 3) ){ //gasmeter
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

#endif

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
