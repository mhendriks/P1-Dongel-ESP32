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

volatile bool dtr1         = false;
bool          Out1Avail    = false;
uint32_t      SerialLastChecked = 0;

// portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

//P1 reader task
void fP1Reader( void * pvParameters ){
  DebugTln(F("Enable slimme meter..."));
  SetupP1In();
#ifndef HAN_READER
  SetupP1Out();
#endif
  esp_task_wdt_add(nullptr);
  smartMeter.enable(false);
// #ifdef ULTRA
//   // digitalWrite(17, LOW); //default on
// #endif   
  while(true) {
    PrintHWMark(0);
#ifndef HAN_READER
    CheckP1Signal();
    handleSlimmemeter();
    P1OutBridge();
#else
    handleHanReader();
#endif
    esp_task_wdt_reset();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
  LogFile("P1 reader: unexpected task exit", true);
  vTaskDelete(NULL);
}

void StartP1Task(){
    if( xTaskCreatePinnedToCore( fP1Reader, "p1-reader", 1024*8, NULL, 10, &tP1Reader, /*core*/ 0 ) == pdPASS ) 

    DebugTln(F("Task tP1Reader succesfully created"));
}

//===========================================================================================
void CheckP1Signal(){
#ifdef VIRTUAL_P1
  // Virtual P1 input is selected via network stream; skip physical P1 auto baud/signal probing.
  if (strlen(virtual_p1_ip) > 0) return;
#endif
  if ( telegramCount - telegramErrors >= 1 ) return; //only checking at start
  if ( ( esp_log_timestamp() - SerialLastChecked) > 11500 ) {
    bPre40 = !bPre40;
    SetupP1In();
    Debugln(F("P1 Serial Port settings changed"));
  }
}

void SetupP1In(){
  Serial1.end();
  vTaskDelay(50 / portTICK_PERIOD_MS);
  DebugT(F("P1 serial set to ")); 
  Serial1.begin(bPre40 ? 9600 : 115200,
                bPre40 ? SERIAL_7E1 : SERIAL_8N1,
                RxP1, TxO1);

  Debugf("%u baud\n", (unsigned)(bPre40 ? 9600 : 115200));
  Serial1.setRxInvert(true); //only Rx  
  smartMeter.doChecksum(!bPre40);

  // Drain: clear the last bytes / ISR-race
  while (Serial1.available()) {(void)Serial1.read();vTaskDelay(1);}
  smartMeter.clearAll(); //on errors clear buffer
  telegramCount = 0;
  telegramErrors = 0;
  
  SerialLastChecked = esp_log_timestamp();
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

static void applyParsedSmartMeterData(MyData& DSMRdataNew, bool isHan) {
  bP1offline = false;
  P1error_cnt_sequence = 0;
  DSMRdata = DSMRdataNew;
  last_telegram_t = millis();

#ifdef HAN_READER
  if (isHan) {
    gasDelivered = 0.0f;
    gasDeliveredTimestamp = "";
    waterDelivered = 0.0f;
    waterDeliveredTimestamp = "";
    mbusGas = 0;
    mbusWater = 0;
    WtrMtr = false;
    bUseEtotals = false;
    bWarmteLink = false;
  }
#endif

  if ((telegramCount - telegramErrors) == 1) {
    SMCheckOnce();
  } else {
    DSMRdata.identification = smID;
    if (DSMRdata.p1_version_be_present) {
      DSMRdata.p1_version = DSMRdata.p1_version_be;
      DSMRdata.p1_version_be_present  = false;
      DSMRdata.p1_version_present     = true;
    }
  }

  if (bUseEtotals) {
    DSMRdata.energy_delivered_tariff1_present = true;
    DSMRdata.energy_delivered_tariff1 = DSMRdata.energy_delivered_total;
    DSMRdata.energy_returned_tariff1_present = true;
    DSMRdata.energy_returned_tariff1 = DSMRdata.energy_returned_total;
  }

  modifySmFaseInfo();
  if (bWarmteLink) {
    DSMRdata.timestamp = DSMRdata.mbus1_delivered.timestamp;
    DSMRdata.timestamp_present = true;
    gasDelivered = MbusDelivered(1);
    gasDeliveredTimestamp = mbusDeliveredTimestamp;
  } else  {
    if (!DSMRdata.timestamp_present && !skipNetwork ) {
      if (Verbose2) DebugTln(F("NTP Time set"));
      if (getLocalTime(&tm)) {
        DSTactive = tm.tm_isdst;
        sprintf(cMsg, "%02d%02d%02d%02d%02d%02d%s\0\0",
                (tm.tm_year -100 ), tm.tm_mon + 1, tm.tm_mday,
                tm.tm_hour, tm.tm_min, tm.tm_sec, DSTactive ? "S" : "W");
      } else {
        strCopy(cMsg, sizeof(cMsg), actTimestamp);
        LogFile("timestamp = old time", true);
      }
      DSMRdata.timestamp         = cMsg;
      DSMRdata.timestamp_present = true;
    }

    if (mbusWater) {
      waterDelivered = MbusDelivered(mbusWater);
      waterDeliveredTimestamp = mbusDeliveredTimestamp;
    }
    if (mbusGas) {
      gasDelivered = MbusDelivered(mbusGas);
      gasDeliveredTimestamp = mbusDeliveredTimestamp;
    }
  }

  processTelegram();
  if (Verbose2) DSMRdata.applyEach(showValues());
}

static void handleParsedMeter(P1Reader& meter, bool isHan) {
  if (!meter.available()) return;

  ToggleLED(LED_ON);
  CapTelegram = meter.CompleteRaw();
  if (!isHan) Out1Avail = true;
  if (bRawPort) ws_raw.println(CapTelegram);

#ifdef VIRTUAL_P1
  if (!isHan) virtSetLastData();
#endif

  if (showRaw) {
    Debugf("Telegram Raw (%d)\n%s\n", meter.raw().length(), CapTelegram.c_str());
    showRaw = false;
    meter.clear();
    ToggleLED(LED_OFF);
    return;
  }

  telegramCount++;
  if (!bHideP1Log) {
    DebugTf("telegramCount=[%d] telegramErrors=[%d] bufferlength=[%d]\r\n",
            telegramCount, telegramErrors, meter.raw().length());
  }
  MyData DSMRdataNew = {};
  String DSMRerror;

  if (meter.parse(&DSMRdataNew, &DSMRerror)) {
    applyParsedSmartMeterData(DSMRdataNew, isHan);
  } else {
    telegramErrors++;
    if (P1error_cnt_sequence++ > 3) bP1offline = true;
    DebugTf("%sParse error\r\n%s\r\n\r\n", isHan ? "HAN " : "", DSMRerror.c_str());
    DebugTf("Telegram\r\n%s\r\n\r\n", CapTelegram.c_str());
    meter.clear();
  }

  ToggleLED(LED_OFF);
}

#ifdef HAN_READER
static void handleParsedMeter(SmartMeterHandle& meter, bool isHan) {
  if (!meter.available()) return;

  ToggleLED(LED_ON);
  CapTelegram = meter.CompleteRaw();
  if (!isHan) Out1Avail = true;
  if (bRawPort) ws_raw.println(CapTelegram);

  if (!isHan) {
#ifdef VIRTUAL_P1
    virtSetLastData();
#endif
  }

  if (showRaw) {
    Debugf("Telegram Raw (%d)\n%s\n", meter.raw().length(), CapTelegram.c_str());
    showRaw = false;
    meter.clear();
    ToggleLED(LED_OFF);
    return;
  }

  telegramCount++;
  if (!bHideP1Log) {
    DebugTf("telegramCount=[%d] telegramErrors=[%d] bufferlength=[%d]\r\n",
            telegramCount, telegramErrors, meter.raw().length());
  }
  MyData DSMRdataNew = {};
  String DSMRerror;

  if (meter.parse(&DSMRdataNew, &DSMRerror)) {
    applyParsedSmartMeterData(DSMRdataNew, isHan);
  } else {
    telegramErrors++;
    if (P1error_cnt_sequence++ > 3) bP1offline = true;
    DebugTf("%sParse error\r\n%s\r\n\r\n", isHan ? "HAN " : "", DSMRerror.c_str());
    DebugTf("Telegram\r\n%s\r\n\r\n", CapTelegram.c_str());
    meter.clear();
  }

  ToggleLED(LED_OFF);
}

static void handleParsedMeter(han::HanReader& meter, bool isHan) {
  if (!meter.available()) return;

  ToggleLED(LED_ON);
  CapTelegram = meter.CompleteRaw();
  if (!isHan) Out1Avail = true;
  if (bRawPort) ws_raw.println(CapTelegram);

  if (showRaw) {
    Debugf("Telegram Raw (%d)\n%s\n", meter.raw().length(), CapTelegram.c_str());
    showRaw = false;
    meter.clear();
    ToggleLED(LED_OFF);
    return;
  }

  telegramCount++;
  if (!bHideP1Log) {
    DebugTf("telegramCount=[%d] telegramErrors=[%d] bufferlength=[%d]\r\n",
            telegramCount, telegramErrors, meter.raw().length());
  }
  MyData DSMRdataNew = {};
  String DSMRerror;

  if (meter.parse(&DSMRdataNew, &DSMRerror)) {
    applyParsedSmartMeterData(DSMRdataNew, isHan);
  } else {
    telegramErrors++;
    if (P1error_cnt_sequence++ > 3) bP1offline = true;
    DebugTf("%sParse error\r\n%s\r\n\r\n", isHan ? "HAN " : "", DSMRerror.c_str());
    DebugTf("Telegram\r\n%s\r\n\r\n", CapTelegram.c_str());
    meter.clear();
  }

  ToggleLED(LED_OFF);
}
#endif

void SetDTR(bool val){
  //  portENTER_CRITICAL_ISR(&mux);
   dtr1 = val;
  //  portEXIT_CRITICAL_ISR(&mux);  
}

//==================================================================================
void IRAM_ATTR dtr_out_int() {
  SetDTR(true);
//  Debugln("int dtr OUTPUT");
}

//==================================================================================
void SetupP1Out(){
  if ( !P1Out ) return; 
  //setup ports
  pinMode(DTR_out, INPUT);
  if ( LED_out != -1 ) {
    pinMode(LED_out, OUTPUT);
  
    //hello world lights
    digitalWrite(LED_out, HIGH); //inverse
    delay(500);
    digitalWrite(LED_out, LOW); //inverse
  }
    //detect DTR changes
  attachInterrupt( DTR_out , dtr_out_int, RISING);
      
  //initial host dtr when p1 device is connected before power up the bridge
  if ( digitalRead(DTR_out) == HIGH ) SetDTR(true);

  // Serial.begin(115200, SERIAL_8N1, -1, TXO1, false);

  Debugln("SetupP1Out executed");
 
}
//==================================================================================
void P1OutBridge(){
  if ( P1Out && dtr1 && Out1Avail ) {

    if ( LED_out >= 0 ) digitalWrite(LED_out, HIGH);
    Serial1.println(CapTelegram);
    // Serial1.flush();
    Out1Avail = false; 
    if ( digitalRead(DTR_out) == LOW ) SetDTR(false);
    if ( LED_out >= 0 ) digitalWrite(LED_out, LOW);
  }
} 

//==================================================================================

#ifdef VIRTUAL_P1
bool bVirt_connected = false;
WiFiClient vp1_client;
time_t vp1_last_connect = 0;
time_t vp1_last_data = 0;

void virtSetLastData(){
   vp1_last_data = millis();
}

void handleVirtualP1(){
  if ( skipNetwork ) return;
  if (strlen(virtual_p1_ip) == 0) {
    if (vp1_client.connected()) {
      bVirt_connected = false;
      vp1_client.stop();
      smartMeter.ChangeStream(&Serial1);
    } else return;
  }

 if ( millis() - vp1_last_data > 20000) vp1_client.stop();
  
  if( ! vp1_client.connected() ){
    if ( (millis() - vp1_last_connect) < 5000 && (millis() > 5000) ) return;
    vp1_last_connect = millis();
    DebugTln(F("Virtual P1 DISCONNECTED"));
    bVirt_connected = false;
    if (!vp1_client.connect(virtual_p1_ip, 82)) {
      DebugTln(F("Virtual P1 to host failed"));
    } else {
      DebugTln(F("Virtual P1 connected"));
      vp1_last_data = millis();
      bVirt_connected = true;
      smartMeter.ChangeStream(&vp1_client);
    }
  }
}
#else
void handleVirtualP1(){}
#endif  

//==================================================================================
#ifdef HAN_READER
void handleHanReader() {
  smartMeter.setSource(SmartMeterSource::HAN);
#if defined(HAN_TESTDATA) || defined(HAN_TESTDATA_RAW)
  hanTestProfile.step(hanMeter, millis(),
#ifdef HAN_TESTDATA_DYNAMIC
                      true
#else
                      false
#endif
  );
#else
  smartMeter.loop();
#endif
  handleParsedMeter(smartMeter, true);
  if (millis() - last_telegram_t > 35000) bP1offline = true;
}
#endif

//==================================================================================
void handleSlimmemeter()
{
  #ifdef HAN_READER
  smartMeter.setSource(SmartMeterSource::DSMR);
  #endif
  smartMeter.loop();
  handleParsedMeter(smartMeter, false);
  if (millis() - last_telegram_t > (bV5meter ? 3500 : 35000)) bP1offline = true;
} // handleSlimmemeter()

//==================================================================================
void SMCheckOnce(){
  DebugTln(F("first time check"));
  DebugFlush();
  if (DSMRdata.identification_present) {
    //--- this is a hack! The identification can have a backslash in it
    //--- that will ruin javascript processing :-(
    for(int i=0; i<DSMRdata.identification.length(); i++)
    {
      if (DSMRdata.identification[i] == '\\') DSMRdata.identification[i] = '=';
    }
    smID = DSMRdata.identification;
  } // check id 

  if ( DSMRdata.energy_delivered_total_present && !DSMRdata.energy_delivered_tariff1_present ) bUseEtotals = true;
  if (DSMRdata.p1_version_be_present) {
    DSMRdata.p1_version = DSMRdata.p1_version_be;
    DSMRdata.p1_version_be_present  = false;
    DSMRdata.p1_version_present     = true;
    // DSMR_NL = false;
  } //p1_version_be_present
  if ( ! DSMRdata.energy_delivered_tariff1_present && !DSMRdata.energy_delivered_total_present ) bWarmteLink = true;
  mbusWater = MbusTypeAvailable(7);  
  if (mbusWater) WtrMtr = true;
  else if ( WtrMtr ) {
      waterDeliveredTimestamp = DSMRdata.timestamp;
      waterDelivered = P1Status.wtr_m3 + (P1Status.wtr_l / 1000.0);
  }
  mbusGas = MbusTypeAvailable(3);  
  DebugTf("mbusWater: %d\r\n",mbusWater);
  DebugTf("mbusGas: %d\r\n",mbusGas);
  ResetStats();
  bV5meter = DSMRdata.voltage_l1_present || DSMRdata.voltage_l2_present || DSMRdata.voltage_l3_present;
  DebugTf("bV5meter: %d\r\n",bV5meter);
  DebugTf("bUseEtotals: %d\r\n",bUseEtotals);
  DebugTf("bWarmteLink: %d\r\n",bWarmteLink);
}
//called every hour
void UpdateYesterday( ){
#ifdef DEBUG
  Debug("newT/(uint32_t)(3600*24): ");Debugln(newT/(uint32_t)(3600*24));
  Debug("dataYesterday.lastUpdDay: ");Debugln( dataYesterday.lastUpdDay);
#endif  
  if ( dataYesterday.lastUpdDay == (newT/(uint32_t)(3600*24)) ) return; //do nothing
  //data yesterday should be changed
  DebugTln("UpdateUsage data");
  dataYesterday.t1    = (uint32_t) (DSMRdata.energy_delivered_tariff1 * 1000);
  dataYesterday.t2    = (uint32_t) (DSMRdata.energy_delivered_tariff2 * 1000);
  dataYesterday.t1r   = (uint32_t) (DSMRdata.energy_returned_tariff1 * 1000);
  dataYesterday.t2r   = (uint32_t) (DSMRdata.energy_returned_tariff2 * 1000);
  dataYesterday.gas   = (uint32_t) (gasDelivered * 1000);
  dataYesterday.water = (uint32_t) (waterDelivered * 1000);
  dataYesterday.lastUpdDay = newT/(3600*24);
}

//==================================================================================
void processTelegram(){
//  DebugTf("Telegram[%d]=>DSMRdata.timestamp[%s]\r\n", telegramCount, DSMRdata.timestamp.c_str());                                                
//  strcpy(newTimestamp, DSMRdata.timestamp.c_str()); 
//!!! DO NOT REFER TO CPU CONSUMING FUNCTIONS ... THIS PROCES SHOULD NOT BLOCK OR TAKES LOTS OF CPU TIME

  newT = epoch(DSMRdata.timestamp.c_str(), DSMRdata.timestamp.length(), true); // update system time
  
  //cal current more accurate; only works with SMR 5 meters
  if ( try_calc_i ) {
    if ( DSMRdata.voltage_l1_present && DSMRdata.voltage_l1 && DSMRdata.power_delivered_l1.int_val() ){
      DSMRdata.current_l1._value = (uint32_t)((DSMRdata.power_delivered_l1.int_val() + DSMRdata.power_returned_l1.int_val())/DSMRdata.voltage_l1*1000);
      DSMRdata.current_l1_present = true;
    }
    if ( DSMRdata.voltage_l2_present && DSMRdata.voltage_l2 && DSMRdata.power_delivered_l2.int_val() ){
      DSMRdata.current_l2._value = (uint32_t)((DSMRdata.power_delivered_l2.int_val() + DSMRdata.power_returned_l2.int_val())/DSMRdata.voltage_l2*1000);
      DSMRdata.current_l2_present = true;
    }
    if ( DSMRdata.voltage_l3_present && DSMRdata.voltage_l3 && DSMRdata.power_delivered_l3.int_val()){
      DSMRdata.current_l3._value = (uint32_t)((DSMRdata.power_delivered_l3.int_val() + DSMRdata.power_returned_l3.int_val())/DSMRdata.voltage_l3*1000);
      DSMRdata.current_l3_present = true;
    }
  }//try calc i
  // has the hour changed write ringfiles
#ifdef DEBUG
  DebugTf("actMin[%02d] -- newMin[%02d]\r\n", minute(actT), minute(newT));  
  if ( ( minute(actT) != minute(newT) ) || P1Status.FirstUse || !dataYesterday.lastUpdDay ) {
    writeRingFiles(); //bWriteFiles = true; //handled in main flow
    UpdateYesterday();
  }
#else
  DebugTf("actHour[%02d] -- newHour[%02d]\r\n", hour(actT), hour(newT));  
  if ( ( hour(actT) != hour(newT) ) || P1Status.FirstUse || !dataYesterday.lastUpdDay) {
    writeRingFiles(); //bWriteFiles = true; //handled in main flow
    if ( hour(actT) == 1 && hour(newT) == 3 ){
      uint8_t slot = CalcSlot(RINGHOURS, false); //based on actT
      slot = ( slot + 1 ) % RingFiles[RINGHOURS].slots; //increase slot 1h to next slot = 2h
      writeRingFileAtSlot(RINGHOURS, slot, nullptr, true); //todo winter -> summer time issue actT = 1h so next slot should be written too
      DebugTln(F("DST gap fix"));
    }
    UpdateYesterday();
    //check dag overgang
    if ( currentDay != (newT/(uint32_t)(3600*24)) ) {
      ResetStats();
      currentDay = newT/(3600*24);
    }
  }
#endif
  
  if ( !skipNetwork ) {
	#ifndef MQTT_DISABLE
	  if ( bMQTTenabled && (DUE(publishMQTTtimer) || settingMQTTinterval == 1 || telegramCount == 1)) bSendMQTT = true; //handled in main flow
	#endif  

	  ProcessStats();
	  NetSwitchStateMngr();
  }

  #ifdef ESPNOW  
		// if ( telegramCount % 3 == 1 ) SendActualData();
		P2PSendActualData();
	#endif
  //update actual time
  strCopy(actTimestamp, sizeof(actTimestamp), DSMRdata.timestamp.c_str()); 
  actT = newT;
  
  // PostHomey();
  #ifdef POST_POWERCH
    if ( (bV5meter && telegramCount % 3 == 0 ) || !bV5meter ) bNewTelegramWebhook = true; //every 3 secs (v5) or new telegram (v2/4)
  #endif
  #ifdef POST_MEENT
    bNewTelegramWebhook = true; // interval handling is done in PostWebhook()
  #endif
  #ifdef UDP_BCAST
    New_P1_UDP = true;
  #endif 
} // processTelegram()

//==================================================================================
void modifySmFaseInfo()
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
  
} //  modifySmFaseInfo()

extern unsigned long startTijdL1;
extern unsigned long startTijdL2;
extern unsigned long startTijdL3;
extern bool overspanningActiefL1;
extern bool overspanningActiefL2;
extern bool overspanningActiefL3;

void ResetOvervoltageStats() {
  P1Stats.TU1over = 0;
  P1Stats.TU2over = 0;
  P1Stats.TU3over = 0;
  startTijdL1 = 0;
  startTijdL2 = 0;
  startTijdL3 = 0;
  overspanningActiefL1 = false;
  overspanningActiefL2 = false;
  overspanningActiefL3 = false;
}

void ResetStats(){
  P1Stats.I1piek  = 0;
  P1Stats.I2piek  = 0;
  P1Stats.I3piek  = 0;
  P1Stats.U1piek  = 0;
  P1Stats.U2piek  = 0;
  P1Stats.U3piek  = 0;
  P1Stats.U1min   = 0xFFFFFFFF;
  P1Stats.U2min   = 0xFFFFFFFF;
  P1Stats.U3min   = 0xFFFFFFFF;
  P1Stats.P1max   = 0;
  P1Stats.P2max   = 0;
  P1Stats.P3max   = 0;
  P1Stats.P1min   = 0x7FFFFFFF;
  P1Stats.P2min   = 0x7FFFFFFF;
  P1Stats.P3min   = 0x7FFFFFFF;  
  P1Stats.Psluip  = 0xFFFFFFFF;
  ResetOvervoltageStats();
  P1Stats.StartTime = epoch(DSMRdata.timestamp.c_str(), DSMRdata.timestamp.length(), false);
}

unsigned long startTijdL1 = 0;
unsigned long startTijdL2 = 0;
unsigned long startTijdL3 = 0;

bool overspanningActiefL1 = false;
bool overspanningActiefL2 = false;
bool overspanningActiefL3 = false;


void controleerOverspanning(int spanning, uint32_t &overspanningTotaal, unsigned long &startTijd, bool &overspanning) {
  if (spanning >= settingOvervoltageThreshold) {
    if (!overspanning) {
      startTijd = millis();
      overspanning = true;
    }
  } else {
    if (overspanning) {
      overspanningTotaal += (millis() - startTijd) / 1000;
      overspanning = false;
    }
  }
}

uint32_t actueleOverspanningSeconden(uint32_t overspanningTotaal, unsigned long startTijd, bool overspanning) {
  if (!overspanning || startTijd == 0) return overspanningTotaal;
  return overspanningTotaal + ((millis() - startTijd) / 1000);
}

void ProcessStats(){
  if ( DSMRdata.current_l1_present && DSMRdata.current_l1.int_val() > P1Stats.I1piek ) P1Stats.I1piek = DSMRdata.current_l1.int_val();
  if ( DSMRdata.current_l2_present && DSMRdata.current_l2.int_val() > P1Stats.I2piek ) P1Stats.I2piek = DSMRdata.current_l2.int_val();
  if ( DSMRdata.current_l3_present && DSMRdata.current_l3.int_val() > P1Stats.I3piek ) P1Stats.I3piek = DSMRdata.current_l3.int_val();
  
  if ( DSMRdata.power_delivered_l1_present) {
    if ( DSMRdata.power_delivered_l1.int_val() > P1Stats.P1max ) P1Stats.P1max = DSMRdata.power_delivered_l1.int_val();
    else if ( (int32_t)(DSMRdata.power_delivered_l1.int_val() - DSMRdata.power_returned_l1.int_val()) < P1Stats.P1min ) P1Stats.P1min = (int32_t)(DSMRdata.power_delivered_l1.int_val() - DSMRdata.power_returned_l1.int_val());
  } 

  if ( DSMRdata.power_delivered_l2_present ) {
      if ( DSMRdata.power_delivered_l2.int_val() > P1Stats.P2max ) P1Stats.P2max = DSMRdata.power_delivered_l2.int_val();
      else if ( (int32_t)(DSMRdata.power_delivered_l2.int_val() - DSMRdata.power_returned_l2.int_val()) < P1Stats.P2min ) P1Stats.P2min = (int32_t)(DSMRdata.power_delivered_l2.int_val() - DSMRdata.power_returned_l2.int_val());
  }
  
  if ( DSMRdata.power_delivered_l3_present ){
    if ( DSMRdata.power_delivered_l3.int_val() > P1Stats.P3max ) P1Stats.P3max = DSMRdata.power_delivered_l3.int_val();
    else if ( (int32_t)(DSMRdata.power_delivered_l3.int_val() - DSMRdata.power_returned_l3.int_val()) < P1Stats.P3min ) P1Stats.P3min = (int32_t)(DSMRdata.power_delivered_l3.int_val() - DSMRdata.power_returned_l3.int_val());
  } 

  if ( DSMRdata.voltage_l1_present ) {
    if ( DSMRdata.voltage_l1.int_val() > P1Stats.U1piek ) P1Stats.U1piek = DSMRdata.voltage_l1.int_val();
    else if ( DSMRdata.voltage_l1.int_val() < P1Stats.U1min ) P1Stats.U1min = DSMRdata.voltage_l1.int_val(); 
    controleerOverspanning(DSMRdata.voltage_l1, P1Stats.TU1over, startTijdL1, overspanningActiefL1);
  }
  if ( DSMRdata.voltage_l2_present ) {
    if ( DSMRdata.voltage_l2.int_val() > P1Stats.U2piek ) P1Stats.U2piek = DSMRdata.voltage_l2.int_val(); 
    else if ( DSMRdata.voltage_l2.int_val() < P1Stats.U2min ) P1Stats.U2min = DSMRdata.voltage_l2.int_val(); 
    controleerOverspanning(DSMRdata.voltage_l2, P1Stats.TU2over, startTijdL2, overspanningActiefL2);
  }
  if ( DSMRdata.voltage_l3_present ) {
    if ( DSMRdata.voltage_l3.int_val() > P1Stats.U3piek ) P1Stats.U3piek = DSMRdata.voltage_l3.int_val();
    else if ( DSMRdata.voltage_l3.int_val() < P1Stats.U3min ) P1Stats.U3min = DSMRdata.voltage_l3.int_val(); 
    controleerOverspanning(DSMRdata.voltage_l3, P1Stats.TU3over, startTijdL3, overspanningActiefL3);
  }  
  if ( (hour() < 5) && (DSMRdata.power_delivered.int_val() < P1Stats.Psluip) && DSMRdata.power_delivered.int_val() ) P1Stats.Psluip = DSMRdata.power_delivered.int_val();
  // overspannning bijhouden per etmaal; seconden tellen boven de ingestelde grens per fase
}

//==================================================================================
byte MbusTypeAvailable(byte type){

  // return first type which is available
  if      ( DSMRdata.mbus4_device_type == type ) return 4;
  else if ( DSMRdata.mbus3_device_type == type ) return 3;
  else if ( DSMRdata.mbus2_device_type == type ) return 2;
  else if ( DSMRdata.mbus1_device_type == type ) return 1;
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
