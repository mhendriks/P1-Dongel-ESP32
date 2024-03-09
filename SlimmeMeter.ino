/*
***************************************************************************  
**  Program  : handleSlimmeMeter - part of DSMRloggerAPI
**  Version  : v4.2.1
**
**  Copyright (c) 2023 Martijn Hendriks
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
//==================================================================================
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
    }
  }
};

// PRO_H20_2 pinout
#define P1_LED     16
#define O1_DTR_IO  15
#define TXO1       21
volatile bool dtr1         = false;
bool Out1Avail    = false;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

void SetDTR(bool val){
   portENTER_CRITICAL_ISR(&mux);
   dtr1 = val;
   portEXIT_CRITICAL_ISR(&mux);  
}

//==================================================================================
void IRAM_ATTR dtr_out_int() {
  SetDTR(true);
//  Debugln("int dtr OUTPUT");
}

//==================================================================================
void SetupP1Out(){
//  if ( P1Status.dev_type == PRO_H20_2 ) {
  //setup ports
  pinMode(O1_DTR_IO, INPUT);
  pinMode(P1_LED, OUTPUT);
  
  //hello world lights
  digitalWrite(P1_LED, HIGH); //inverse
  delay(500);
  digitalWrite(P1_LED, LOW); //inverse

    //detect DTR changes
  attachInterrupt( O1_DTR_IO , dtr_out_int, RISING);
      
  //initial host dtr when p1 device is connected before power up the bridge
  if ( digitalRead(O1_DTR_IO) == HIGH ) SetDTR(true);

  Serial.begin(115200, SERIAL_8N1, -1, TXO1, false);

  Debugln("SetupP1Out executed");
//  }
}
//==================================================================================
void P1OutBridge(){
  if ( dtr1 && Out1Avail ) {

    digitalWrite(P1_LED, HIGH);
    Serial.println(CapTelegram);
    Serial.flush();
    Out1Avail = false; 
    if ( digitalRead(O1_DTR_IO) == LOW ) SetDTR(false);
    digitalWrite(P1_LED, LOW);
  }
} 

//==================================================================================
void handleSlimmemeter()
{
  //DebugTf("showRaw (%s)\r\n", showRaw ?"true":"false");
#ifdef STUB
  _handleSTUB();
  return; //escape when stub is active
#endif
    slimmeMeter.loop();
    if (slimmeMeter.available()) {
      ToggleLED(LED_ON);
      CapTelegram = "/" + slimmeMeter.raw() + "!" + slimmeMeter.GetCRC_str() + "\r\n"; //capture last telegram
      Out1Avail = true;
      if (showRaw) {
        //-- process telegrams in raw mode
        Debugf("Telegram Raw (%d)\n%s\n", slimmeMeter.raw().length(), CapTelegram.c_str() ); 
        showRaw = false; //only 1 reading
      } else processSlimmemeter();
      ToggleLED(LED_OFF);
    } //available
} // handleSlimmemeter()

//==================================================================================
void SMCheckOnce(){
  DebugTln(F("first time check"));
  if (DSMRdata.identification_present) {
    //--- this is a hack! The identification can have a backslash in it
    //--- that will ruin javascript processing :-(
    for(int i=0; i<DSMRdata.identification.length(); i++)
    {
      if (DSMRdata.identification[i] == '\\') DSMRdata.identification[i] = '=';
      yield();
    }
    smID = DSMRdata.identification;
  } // check id 

  if (DSMRdata.p1_version_be_present) {
    DSMRdata.p1_version = DSMRdata.p1_version_be;
    DSMRdata.p1_version_be_present  = false;
    DSMRdata.p1_version_present     = true;
    DSMR_NL = false;
  } //p1_version_be_present
  
  mbusWater = MbusTypeAvailable(7);  
  if (mbusWater) WtrMtr = true;
  mbusGas = MbusTypeAvailable(3);  
  DebugTf("mbusWater: %d\r\n",mbusWater);
  DebugTf("mbusGas: %d\r\n",mbusGas);
}
//==================================================================================
void processSlimmemeter() {
    telegramCount++;
    
    // Voorbeeld: [21:00:11][   9880/  8960] loop        ( 997): read telegram [28] => [140307210001S]
    if (!bHideP1Log) {
      Debugln(F("\r\nP [Time----][FreeHeap/mBlck][Function----(line):"));
      DebugTf("telegramCount=[%d] telegramErrors=[%d] bufferlength=[%d]\r\n", telegramCount, telegramErrors,slimmeMeter.raw().length());
    }
        
#ifndef STUB
    DSMRdata = {};
    String    DSMRerror;
        
    if (slimmeMeter.parse(&DSMRdata, &DSMRerror))   // Parse succesful
    {
#endif      
      if ( (telegramCount - telegramErrors) == 1) SMCheckOnce(); //only the first succesfull telegram
      else {
        //use the keys from the initial check; saves processing power
        DSMRdata.identification = smID;
        if ( !DSMR_NL ){
          DSMRdata.p1_version = DSMRdata.p1_version_be;
          DSMRdata.p1_version_be_present  = false;
          DSMRdata.p1_version_present     = true;
        } 
      }
        
      modifySmFaseInfo();
#ifdef HEATLINK
//        DSMRdata.timestamp         = cMsg;
        DSMRdata.timestamp = DSMRdata.mbus1_delivered.timestamp;
        DSMRdata.timestamp_present = true;
#else
  if (!DSMRdata.timestamp_present) { 
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
  } //!timestamp present
#endif //HEATLINK

      //-- handle mbus delivered values
      if (mbusWater) {
        waterDelivered = MbusDelivered(mbusWater);
        waterDeliveredTimestamp = mbusDeliveredTimestamp;
      }
#ifdef HEATLINK
      gasDelivered = MbusDelivered(1); //always 1ste
      gasDeliveredTimestamp = mbusDeliveredTimestamp;
#else
      if (mbusGas) {
        gasDelivered = MbusDelivered(mbusGas);
        gasDeliveredTimestamp = mbusDeliveredTimestamp;
      }
#endif
        
      processTelegram();
      if (Verbose2) DSMRdata.applyEach(showValues());
#ifndef STUB
    } 
    else // Parser error, print error
    {
      telegramErrors++;
      DebugTf("Parse error\r\n%s\r\n\r\n", DSMRerror.c_str());
      DebugTf("Telegram\r\n%s\r\n\r\n", CapTelegram.c_str());
      slimmeMeter.clear(); //on errors clear buffer
    }
#endif    
  
} // handleSlimmeMeter()

//==================================================================================
void processTelegram(){
//  DebugTf("Telegram[%d]=>DSMRdata.timestamp[%s]\r\n", telegramCount, DSMRdata.timestamp.c_str());                                                
//  strcpy(newTimestamp, DSMRdata.timestamp.c_str()); 
//!!! DO NOT REFER TO CPU CONSUMING FUNCTIONS ... THIS PROCES SHOULD NOT BLOCK OR TAKES LOTS OF CPU TIME

  newT = epoch(DSMRdata.timestamp.c_str(), DSMRdata.timestamp.length(), true); // update system time
  DebugTf("actHour[%02d] -- newHour[%02d]\r\n", hour(actT), hour(newT));  

  // has the hour changed write ringfiles
  if ( ( hour(actT) != hour(newT) ) || P1Status.FirstUse ) writeRingFiles(); //bWriteFiles = true; //handled in main flow
  
  //handle mqtt
#ifndef MQTT_DISABLE
  if ( DUE(publishMQTTtimer) || settingMQTTinterval == 1) bSendMQTT = true; //handled in main flow
#endif  
  // handle rawport
  if ( bRawPort ) ws_raw.println( CapTelegram ); //print telegram to dongle port
  
  ProcessMaxVoltage();

  //update actual time
  strCopy(actTimestamp, sizeof(actTimestamp), DSMRdata.timestamp.c_str()); 
  actT = newT;
  
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
byte MbusTypeAvailable(byte type){

  // return first type which is available
  if      ( DSMRdata.mbus1_device_type == type ) return 1;
  else if ( DSMRdata.mbus2_device_type == type ) return 2;
  else if ( DSMRdata.mbus3_device_type == type ) return 3;
  else if ( DSMRdata.mbus4_device_type == type ) return 4;
  return 0;
}

//==================================================================================
float MbusDelivered(byte mbusPort){
  switch ( mbusPort ){
    case 1: 
            if (DSMRdata.mbus1_delivered_ntc_present) DSMRdata.mbus1_delivered = DSMRdata.mbus1_delivered_ntc;
            else if (DSMRdata.mbus1_delivered_dbl_present) DSMRdata.mbus1_delivered = DSMRdata.mbus1_delivered_dbl;
            DSMRdata.mbus1_delivered_present     = true;
            DSMRdata.mbus1_delivered_ntc_present = false;
            DSMRdata.mbus1_delivered_dbl_present = false;
            mbusDeliveredTimestamp = DSMRdata.mbus1_delivered.timestamp;   
            return (float)(DSMRdata.mbus1_delivered * 1.0);
            break;
    case 2: 
            if (DSMRdata.mbus2_delivered_ntc_present) DSMRdata.mbus2_delivered = DSMRdata.mbus2_delivered_ntc;
            else if (DSMRdata.mbus2_delivered_dbl_present) DSMRdata.mbus2_delivered = DSMRdata.mbus2_delivered_dbl;
            DSMRdata.mbus2_delivered_present     = true;
            DSMRdata.mbus2_delivered_ntc_present = false;
            DSMRdata.mbus2_delivered_dbl_present = false;
            mbusDeliveredTimestamp = DSMRdata.mbus2_delivered.timestamp;   
            return (float)(DSMRdata.mbus2_delivered * 1.0);
            break;      
    case 3: 
            if (DSMRdata.mbus3_delivered_ntc_present) DSMRdata.mbus3_delivered = DSMRdata.mbus3_delivered_ntc;
            else if (DSMRdata.mbus3_delivered_dbl_present) DSMRdata.mbus3_delivered = DSMRdata.mbus3_delivered_dbl;
            DSMRdata.mbus3_delivered_present     = true;
            DSMRdata.mbus3_delivered_ntc_present = false;
            DSMRdata.mbus3_delivered_dbl_present = false;
            mbusDeliveredTimestamp = DSMRdata.mbus3_delivered.timestamp;   
            return (float)(DSMRdata.mbus3_delivered * 1.0);
            break;    
    case 4: 
            if (DSMRdata.mbus4_delivered_ntc_present) DSMRdata.mbus4_delivered = DSMRdata.mbus4_delivered_ntc;
            else if (DSMRdata.mbus4_delivered_dbl_present) DSMRdata.mbus4_delivered = DSMRdata.mbus4_delivered_dbl;
            DSMRdata.mbus4_delivered_present     = true;
            DSMRdata.mbus4_delivered_ntc_present = false;
            DSMRdata.mbus4_delivered_dbl_present = false;
            mbusDeliveredTimestamp = DSMRdata.mbus4_delivered.timestamp;   
            return (float)(DSMRdata.mbus4_delivered * 1.0);
            break;
    default:
            DebugTln(F("default"));
            return 0;
            break;
  }
}

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
