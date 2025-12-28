/* 
***************************************************************************  
**  Program  : helperStuff, part of DSMRloggerAPI
**
**  Copyright (c) 2021 Willem Aandewiel / Martijn Hendriks
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/
#ifndef _HELPER
#define _HELPER

#include "rom/rtc.h"
#include "esp_efuse.h"
#include "esp_efuse_table.h"

// ------------------ ENUMS & CONSTANTS ------------------ //
enum HWtype { UNDETECTED, P1P, NRGD, P1E, P1EP, P1UM, P1U, NRGM, P1S, P1UX2 };
const char* const HWTypeNames[]  = { "N/A", "P1P", "NRGD", "P1E", "P1EP", "P1UM", "P1U", "NRGM", "P1S", "P1UX2" };
const char* const ModTypeNames[] = { "N/A", "IO+", "H2O", "RS485" };

// ------------------ GLOBAL VARIABLES ------------------ //
HWtype HardwareType = UNDETECTED;
uint16_t HardwareVersion = 0; 
int8_t   rgbled_io = RGBLED_PIN;

static inline const dev_conf& DEVCONF() { return device_config[(int)HardwareType]; }
#define _getChipId() (uint64_t)ESP.getEfuseMac()

static const char* const resetReasonsIDF[] = {
    [0]  = "0 - ESP_RST_UNKNOWN - Cannot be determined",
    [1]  = "1 - ESP_RST_POWERON - Power-on reset",
    [2]  = "2 - ESP_RST_EXT - External pin reset",
    [3]  = "3 - ESP_RST_SW - Software reset (esp_restart)",
    [4]  = "4 - ESP_RST_PANIC - Exception/Panic",
    [5]  = "5 - ESP_RST_INT_WDT - Interrupt watchdog",
    [6]  = "6 - ESP_RST_TASK_WDT - Task watchdog",
    [7]  = "7 - ESP_RST_WDT - Other watchdog",
    [8]  = "8 - ESP_RST_DEEPSLEEP - Wake from deep sleep",
    [9]  = "9 - ESP_RST_BROWNOUT - Brownout",
    [10] = "10 - ESP_RST_SDIO - SDIO reset",
    [11] = "11 - ESP_RST_USB - USB peripheral reset",
    [12] = "12 - ESP_RST_JTAG - JTAG reset",
    [13] = "13 - ESP_RST_EFUSE - eFuse error",
    [14] = "14 - ESP_RST_PWR_GLITCH - Power glitch detected",
    [15] = "15 - ESP_RST_CPU_LOCKUP - CPU lock-up"
};

/* WD timers 
- idle = off
- 10 sec on tasks
*/
void SetupWDT(){
  esp_task_wdt_deinit();
  esp_task_wdt_config_t cfg = {
    .timeout_ms = 22000, //in 15sec default 
    // .idle_core_mask = (1<<0) | (1<<1), //S3 watch idle core 0 & 1
    // .idle_core_mask = (1<<0), //C3 watch idle core 0 
    .idle_core_mask = 0, //idle core watch dog OFF
#ifdef DEBUG
    .trigger_panic = true
#else
    .trigger_panic = false
#endif
  };
  esp_task_wdt_init(&cfg);
  esp_task_wdt_add(NULL);
}

// ------------------ HARDWARE DETECTIE ------------------ //
static inline const char* hwTypeNameSafe(uint16_t t) {
  if (t < (sizeof(HWTypeNames) / sizeof(HWTypeNames[0])) && HWTypeNames[t]) return HWTypeNames[t];
  return "UNKNOWN";
}

//detect hardware type if burned in efuse
void DetectHW() {
  uint32_t raw = 0;
  esp_err_t err = esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA, &raw, 32);
  if (err != ESP_OK) {
    HardwareType    = UNDETECTED;
    HardwareVersion = 0;
    Debugf("!Error reading eFuse: %s\n", esp_err_to_name(err));
    return;
  }

  // HardwareType    = (uint16_t)((raw >> 16) & 0xFFFF);
  HardwareType    = (HWtype)((raw >> 16) & 0xFFFF);
  HardwareVersion = (uint16_t)(raw & 0xFFFF);

  Debugf("HW type   : %s (%u)\n", hwTypeNameSafe(HardwareType), HardwareType);
  Debugf("HW version: %u\n", HardwareVersion);
}

enum ModuleType : int8_t { MOD_UNKNOWN=-1, MOD_NONE=0, MOD_IOP=1, MOD_H20=2, MOD_RS485=3 };

static inline const char* modName(int8_t t){
  switch(t){
    case MOD_NONE: return "NONE";
    case MOD_IOP:  return "IO+";
    case MOD_H20:  return "H20";
    case MOD_RS485:return "RS485";
    default:       return "UNKNOWN";
  }
}

void DetectModule(int8_t slot){
  /* check slot module
  S1 S2
   0  0 -> NONE
   0  1 -> IO+
   1  0 -> H20
   1  1 -> RS485
  */
  if (!active_mod_conf) return;
  if (slot < 0 || slot >= 2) return;

  pinMode(active_mod_conf->io_conf[slot].sense1, INPUT);
  pinMode(active_mod_conf->io_conf[slot].sense2, INPUT);

  int s1 = digitalRead(active_mod_conf->io_conf[slot].sense1) ? 1 : 0;
  int s2 = digitalRead(active_mod_conf->io_conf[slot].sense2) ? 1 : 0;
  modType[slot] = (s1 << 1) | s2;

#ifdef DEBUG  
  Debugf("\n-- MODULE slot %d: S1=%d S2=%d -> type=%s (%d)\n", slot, s1, s2, modName(modType[slot]), modType[slot]);
#endif  
}

//using double module will active only module in slot 2
void ActivateModule( int8_t slot ){
  if ( modType[slot] == -1 ) return;
  switch ( modType[slot] ) {
  case 0: //NO BOARD
          break;
  case 1: //IO+ Do Nothing for now
          break;              
  case 2: //H20
          IOWater = active_mod_conf->io_conf[slot].wtr_s0;
          WtrMtr = true;
          break;              
  case 3: //RS485
          mb_rx  = active_mod_conf->io_conf[slot].mb_rx;
          mb_tx  = active_mod_conf->io_conf[slot].mb_tx;
          mb_rts = active_mod_conf->io_conf[slot].mb_rts;
          break;              
  }
}

// helper: zet pin als output en schrijf waarde, alleen als pin >= 0
static inline void pinWriteIfValid(int8_t pin, uint8_t level) {
  if (pin >= 0) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, level);
  }
}

//detect module and activate + Ultra V1 module assignment
void DetectModule() {
  switch ( HardwareType ){
    case NRGD:
            active_mod_conf = &module_config[0];
            DetectModule(0); ActivateModule(0);
            break;
#ifdef ULTRA
    case P1UX2:
            // if (HardwareVersion >= 101) pinWriteIfValid(dc.eth_rst, HIGH);   // disable W5500 RESET only 1.1+ hardware
            active_mod_conf = &module_config[2];
            DetectModule(0);  ActivateModule(0);
            DetectModule(1);  ActivateModule(1);
            break;
    case P1U:
            active_mod_conf = &module_config[1];
            DetectModule(0);  ActivateModule(0);
            DetectModule(1);  ActivateModule(1);
            break;
    case P1UM:
            // Ultra V1 fallback and P1UM / P1U none efused
            mb_rx   = RXPIN;
            mb_tx   = TXPIN;
            mb_rts  = RTSPIN;
            break;
#endif            
  } //switch
}

// old device mapping = depricated
void DevTypeMapping(){ 

#ifdef ULTRA
  if ( HardwareType == UNDETECTED ) HardwareType = P1UM;
#else
  //none Ultra dongles only
  if ( HardwareType == UNDETECTED ) {
  #ifdef ETHERNET
    #ifdef ETH_P1EP
        HardwareType = P1EP;
    #else
        HardwareType = P1E;
    #endif    
  #else
      #ifdef NRG_DONGLE
        HardwareType = NRGD;
      #else 
        //PRO
        HardwareType = PRO;
      #endif
  #endif 
  }

#endif

  //Hardware type is set... load the config
  const dev_conf& dc = DEVCONF();
  
  rgbled_io = dc.rgb;
  RxP1      = dc.p1_in_rx;
  // DTR?      = dc.p1_in_dtr;
  statusled = dc.led;
  TxO1      = dc.p1_out_tx;
  DTR_out   = dc.p1_out_dtr;
  LED_out   = dc.p1_out_led;
  P1Out     = dc.p1_out_tx;
  UseRGB    = (dc.rgb >= 0);
  IOWater   = dc.water;

  DebugTf(
    "Pins: RGB=%d statusLED=%d Rx=%d Tx=%d DTR=%d LED_out=%d P1Out=%d Water=%d UseRGB=%d\n",
    rgbled_io, statusled, RxP1, TxO1, DTR_out, LED_out, P1Out, IOWater, UseRGB
  );

  // W5500 RESET handling = ENABLE ETH
  if ( HardwareType == P1UX2 && HardwareVersion >= 101 ) pinWriteIfValid(dc.eth_rst, HIGH);   // disable W5500 RESET only 1.1+ hardware

}

void SetConfig(){
  DetectHW();
  DevTypeMapping();
  DetectModule();

  //pin modes
  // pinMode(DTR_IO, OUTPUT);
  if ( statusled !=-1  ) pinMode(statusled, OUTPUT);
  if ( IOWater != -1 ) pinMode(IOWater, INPUT_PULLUP);
  
  //old hardware versions with DTR P1 IN -> forced to read.
  if ( DTR_out >= 0 ) {
    pinMode(DTR_out, OUTPUT); 
    digitalWrite(DTR_out,LOW);
  }
  
  Debugf("---> Config set UseRGB [%s] IOWater [%d]\n\n", UseRGB ? "true" : "false", IOWater);
  
  // sign of life = ON during setup or change config
  SwitchLED( LED_ON, LED_BLUE );
  delay(1300);
  SwitchLED( LED_OFF, LED_BLUE );
}

void FacReset() {
  DebugTln(F("/!\\ Factory reset"));
//  bFacReset = false;
  P1StatusClear();
  LittleFS.remove("/DSMRsettings.json");
  LittleFS.remove("/fixedip.json");
  resetWifi();
}

void ToggleLED(byte mode) {
  if ( UseRGB ) { 
    if ( LEDenabled ) SwitchLED( mode, LED_GREEN ); 
    else { rgbLedWrite(rgbled_io,0,0,0); /*rgb.setPixel(LED_BLACK);*/ }; 
  } 
  else if ( LEDenabled ) digitalWrite(statusled, !digitalRead(statusled)); else digitalWrite(statusled, LED_OFF);
}

void ClearRGB(){
  R_value = 0;
  G_value = 0;
  B_value = 0;
}

void SwitchLED( byte mode, uint32_t color) {
if ( UseRGB ) {
    if ( LEDenabled ) {
      uint32_t value = 0; //off mode
      if ( mode == LED_ON ) value = BRIGHTNESS; 
      if ( color != LED_GREEN ) ClearRGB();
      switch ( color ) {
      case LED_RED:   R_value = value; break;
      case LED_GREEN: G_value = value; break;
      case LED_BLUE:  B_value = value; break;  
      } 
    } else ClearRGB();
    rgbLedWrite(rgbled_io,R_value,G_value,B_value); //sdk 3.0
   } else {
      digitalWrite(statusled, color==LED_RED?LED_OFF:mode);
   }
}

//===========================================================================================

// const char* getResetReason(){
//     return String(String(rtc_get_reset_reason(0)) + resetReasons[ rtc_get_reset_reason(0) > 15? 0 : rtc_get_reset_reason(0) ]).c_str();
//   // esp_reset_reason_t r = esp_reset_reason();
//   // DebugTf("Last Reset reason: %d = %s\n", r, RESET_REASON(r));
//   // return RESET_REASON(r);
// }

const char* getResetReason() {
    static char buf[64];
    esp_reset_reason_t r = esp_reset_reason();
    if (r < 0 || r > 15 || resetReasonsIDF[r] == nullptr) {
        snprintf(buf, sizeof(buf), "Unknown reset reason (%d)", (int)r);
        return buf;
    }
    return resetReasonsIDF[r];
}

//===========================================================================================

void ShutDownHandler(){
  MQTTDisconnect();
  P1StatusWrite();
  P1StatusEnd();
  DebugTln(F("/!\\ SHUTDOWN /!\\"));
  DebugFlush();
}

//===========================================================================================

void P1Reboot(){
    delay(200);
    ESP.restart();
    delay(500);  
}

//===========================================================================================
bool bailout () // to prevent firmware from crashing!
{
  if (ESP.getFreeHeap() > 5500) return false; //do nothing
  
  DebugT(F("Bailout due to low heap --> reboot in 3 seconds")); Debugln(ESP.getFreeHeap());
  LogFile("reboot due to low heap", false);
  P1Reboot();
  return true; // komt hier als het goed is niet
  
}

//===========================================================================================
bool compare(String x, String y) 
{ 
    for (int i = 0; i < min(x.length(), y.length()); i++) { 
      if (x[i] != y[i]) 
      {
        return (bool)(x[i] < y[i]); 
      }
    } 
    return x.length() < y.length(); 
    
} // compare()


//===========================================================================================
boolean isValidIP(IPAddress ip)
{
 /* Works as follows:
   *  example: 
   *  127.0.0.1 
   *   1 => 127||0||0||1 = 128>0 = true 
   *   2 => !(false || false) = true
   *   3 => !(false || false || false || false ) = true
   *   4 => !(true && true && true && true) = false
   *   5 => !(false) = true
   *   true && true & true && false && true = false ==> correct, this is an invalid addres
   *   
   *   0.0.0.0
   *   1 => 0||0||0||0 = 0>0 = false 
   *   2 => !(true || true) = false
   *   3 => !(false || false || false || false) = true
   *   4 => !(true && true && true && tfalse) = true
   *   5 => !(false) = true
   *   false && false && true && true && true = false ==> correct, this is an invalid addres
   *   
   *   192.168.0.1 
   *   1 => 192||168||0||1 =233>0 = true 
   *   2 => !(false || false) = true
   *   3 +> !(false || false || false || false) = true
   *   4 => !(false && false && true && true) = true
   *   5 => !(false) = true
   *   true & true & true && true && true = true ==> correct, this is a valid address
   *   
   *   255.255.255.255
   *   1 => 255||255||255||255 =255>0 = true 
   *   2 => !(false || false ) = true
   *   3 +> !(true || true || true || true) = false
   *   4 => !(false && false && false && false) = true
   *   5 => !(true) = false
   *   true && true && false && true && false = false ==> correct, this is an invalid address
   *   
   *   0.123.12.1       => true && false && true && true && true = false  ==> correct, this is an invalid address 
   *   10.0.0.0         => true && false && true && true && true = false  ==> correct, this is an invalid address 
   *   10.255.0.1       => true && true && false && true && true = false  ==> correct, this is an invalid address 
   *   150.150.255.150  => true && true && false && true && true = false  ==> correct, this is an invalid address 
   *   
   *   123.21.1.99      => true && true && true && true && true = true    ==> correct, this is annvalid address 
   *   1.1.1.1          => true && true && true && true && true = true    ==> correct, this is annvalid address 
   *   
   *   Some references on valid ip addresses: 
   *   - https://www.quora.com/How-do-you-identify-an-invalid-IP-address
   *   
   */
  boolean _isValidIP = false;
  _isValidIP = ((ip[0] || ip[1] || ip[2] || ip[3])>0);                             // if any bits are set, then it is not 0.0.0.0
  _isValidIP &= !((ip[0]==0) || (ip[3]==0));                                       // if either the first or last is a 0, then it is invalid
  _isValidIP &= !((ip[0]==255) || (ip[1]==255) || (ip[2]==255) || (ip[3]==255)) ;  // if any of the octets is 255, then it is invalid  
  _isValidIP &= !(ip[0]==127 && ip[1]==0 && ip[2]==0 && ip[3]==1);                 // if not 127.0.0.0 then it might be valid
  _isValidIP &= !(ip[0]>=224);                                                     // if ip[0] >=224 then reserved space  
  
//  DebugTf( "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
//  if (_isValidIP) 
//    Debugln(F(" = Valid IP")); 
//  else 
//    Debugln(F(" = Invalid IP!"));
    
  return _isValidIP;
  
} //  isValidIP()


//===========================================================================================
bool isNumericp(const char *timeStamp, int8_t len)
{
  for (int i=0; (i<len && i<12);i++)
  {
    if (timeStamp[i] < '0' || timeStamp[i] > '9')
    {
      return false;
    }
  }
  return true;
  
} // isNumericp()


//===========================================================================================
int8_t splitString(String inStrng, char delimiter, String wOut[], uint8_t maxWords) 
{
  int16_t inxS = 0, inxE = 0, wordCount = 0;
  
    inStrng.trim();
    while(inxE < inStrng.length() && wordCount < maxWords) 
    {
      inxE  = inStrng.indexOf(delimiter, inxS);         //finds location of first ,
      wOut[wordCount] = inStrng.substring(inxS, inxE);  //captures first data String
      wOut[wordCount].trim();
      //DebugTf("[%d] => [%c] @[%d] found[%s]\r\n", wordCount, delimiter, inxE, wOut[wordCount].c_str());
      inxS = inxE;
      inxS++;
      wordCount++;
    }
    // zero rest of the words
    for(int i=wordCount; i< maxWords; i++)
    {
      wOut[wordCount][0] = 0;
    }
    // if not whole string processed place rest in last word
    if (inxS < inStrng.length()) 
    {
      wOut[maxWords-1] = inStrng.substring(inxS, inStrng.length());  // store rest of String      
    }

    return wordCount;
    
} // splitString()


uint64_t uptime()  {
  return esp_log_timestamp()/1000;;
}

String upTime()  {
  char    calcUptime[20];
  uint64_t  upTimeSeconds = esp_log_timestamp()/1000;

  snprintf(calcUptime, sizeof(calcUptime), "%d(d)-%02d:%02d(H:m)"
                                          , int((upTimeSeconds / (60 * 60 * 24)) % 365)
                                          , int((upTimeSeconds / (60 * 60)) % 24)
                                          , int((upTimeSeconds / (60)) % 60));

  return calcUptime;

} // upTime()


//===========================================================================================
void strConcat(char *dest, int maxLen, const char *src)
{
  if (strlen(dest) + strlen(src) < maxLen) 
  {
    strcat(dest, src);
  } 
  else
  {
    DebugTf("Combined string > %d chars\r\n", maxLen);
  }
  
} // strConcat()


//===========================================================================================
void strConcat(char *dest, int maxLen, float v, int dec)
{
  static char buff[25];
  if (dec == 0)       sprintf(buff,"%.0f", v);
  else if (dec == 1)  sprintf(buff,"%.1f", v);
  else if (dec == 2)  sprintf(buff,"%.2f", v);
  else if (dec == 3)  sprintf(buff,"%.3f", v);
  else if (dec == 4)  sprintf(buff,"%.4f", v);
  else if (dec == 5)  sprintf(buff,"%.5f", v);
  else                sprintf(buff,"%f",   v);

  if (strlen(dest) + strlen(buff) < maxLen) 
  {
    strcat(dest, buff);
  } 
  else
  {
    DebugTf("Combined string > %d chars\r\n", maxLen);
  }
  
} // strConcat()


//===========================================================================================
void strConcat(char *dest, int maxLen, int32_t v)
{
  static char buff[25];
  sprintf(buff,"%d", v);

  if (strlen(dest) + strlen(buff) < maxLen) 
  {
    strcat(dest, buff);
  } 
  else
  {
    DebugTf("Combined string > %d chars\r\n", maxLen);
  }
  
} // strConcat()



//===========================================================================================
// a 'save' string copy

void strCopy(char *dest, int maxLen, const char *src, uint8_t frm, uint8_t to)
{
  int d=0;
//DebugTf("dest[%s], src[%s] max[%d], frm[%d], to[%d] =>\r\n", dest, src, maxLen, frm, to);
  dest[0] = '\0';
  for (int i=0; i<=frm; i++)
  {
    if (src[i] == 0) return;
  }
  for (int i=frm; (src[i] != 0  && i<=to && d<maxLen); i++)
  {
    dest[d++] = src[i];
  }
  dest[d] = '\0';
    
} // strCopy()

//===========================================================================================
// a 'save' version of strncpy() that does not put a '\0' at
// the end of dest if src >= maxLen!
void strCopy(char *dest, int maxLen, const char *src)
{
  if (maxLen <= 0) return;
  strncpy(dest, src, maxLen - 1);
  dest[maxLen - 1] = '\0';
}

//===========================================================================================
int stricmp(const char *a, const char *b)
{
    for (;; a++, b++) {
        int d = tolower((unsigned char)*a) - tolower((unsigned char)*b);
        if (d != 0 || !*a)
            return d;
    }
    
} // stricmp()


//===========================================================================================
// float formatFloat(float v, int dec)
// {
//   return (String(v, dec).toFloat());

// } //  formatFloat()

//===========================================================================================
float strToFloat(const char *s, int dec)
{
  float r = strtof(s, NULL);
  float m = powf(10.0f, dec);
  return ((int)(r * m)) / m;
}

//=======================================================================        
template<typename Item>
Item& typecastValue(Item& i) 
{
  return i;
}

//=======================================================================        
float typecastValue(TimestampedFixedValue i) 
{
  return strToFloat(String(i).c_str(), 3);
}

//=======================================================================        
float typecastValue(FixedValue i) 
{
  return i;
}

#endif