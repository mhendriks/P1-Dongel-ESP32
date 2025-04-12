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
enum HWtype { UNDETECTED, P1P, NRGD, P1E, P1EP, P1UM, P1U, NRGM, P1S };
const char HWTypeNames[][5] = { "N/A", "P1P", "NRGD", "P1E", "P1EP", "P1UM", "P1U", "NRGM", "P1S" };

// ------------------ GLOBAL VARIABLES ------------------ //
uint16_t HardwareType = UNDETECTED; 
uint16_t HardwareVersion = 0; 
byte     rgbled_io = RGBLED_PIN;

#define _getChipId() (uint64_t)ESP.getEfuseMac()

const PROGMEM char *resetReasons[]  { "Unknown", "Vbat power on reset", "2-unknown","Software reset digital core", "Legacy watch dog reset digital core", 
"Deep Sleep reset digital core", "Reset by SLC module, reset digital core","Timer Group0 Watch dog reset digital core","Timer Group1 Watch dog reset digital core",
"RTC Watch dog Reset digital core","Instrusion tested to reset CPU","Time Group reset CPU","Software reset CPU","RTC Watch dog Reset CPU","for APP CPU, reseted by PRO CPU",
"Reset when the vdd voltage is not stable","RTC Watch dog reset digital core and rtc module"};

// ------------------ HARDWARE DETECTIE ------------------ //
//detect hardware type if burned in efuse
void DetectHW() {
    uint32_t waarde = 0;
    if (esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA, &waarde, 32) != ESP_OK) {
        Debugln("!Fout bij uitlezen van eFuse!");
        return;
    }

    HardwareType = (waarde >> 16) & 0xFFFF;
    HardwareVersion = waarde & 0xFFFF;
    
    Debugf("Hardwaretype: %s\n", HWTypeNames[HardwareType]);
    Debugf("Versienummer: %d\n", HardwareVersion);
}

void DetectModule(int8_t slot){
  /* check slot module
  S1 S2
   0  0 -> NONE
   0  1 -> n/a
   1  0 -> H20
   1  1 -> RS485
  */
  
  pinMode(active_mod_conf->io_conf[slot].sense1, INPUT);
  pinMode(active_mod_conf->io_conf[slot].sense2, INPUT);
  modType[slot] = digitalRead(active_mod_conf->io_conf[slot].sense1) * 2 + digitalRead(active_mod_conf->io_conf[slot].sense2);
#ifdef DEBUG  
  Debugf("\n-- MODULE: type = %d\n",modType[slot]);
  switch ( modType[slot] ) {
    case 0: Debugln("-- MODULE: No board\n");break;
    case 1: Debugln("-- MODULE: IO+\n"); break;
    case 2: Debugln("-- MODULE: H20\n"); break;
    case 3: Debugln("-- MODULE: RS485\n"); break;
  }
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

//detect hardware type if burned in efuse
void DetectModule() {
#ifdef NRG_DONGLE
  DetectModule(0); 
  ActivateModule(0);
#endif   

#ifdef ULTRA
  if ( HardwareType == P1U ) {
    rgbled_io =  9; //Ultra V2
    active_mod_conf = &module_config[1];
    DetectModule(0); 
    ActivateModule(0);
    DetectModule(1); 
    ActivateModule(1);
  } else {
    IOWater = IO_WATER; //Ultra V1
    mb_rx   = RXPIN;
    mb_tx   = TXPIN;
    mb_rts  = RTSPIN;
  }
  UseRGB = true; 
  P1Out = true;
  
#endif

}

void SetConfig(){
  DetectHW();
  DetectModule();
#ifndef ULTRA
  switch ( P1Status.dev_type ) {
    case PRO       : UseRGB = false; 
                     IOWater = 5;
                     break;
    case PRO_BRIDGE: UseRGB = true; 
                     IOWater = -1;
                     pinMode(PRT_LED, OUTPUT);
                     digitalWrite(PRT_LED, true); //default on
                     break;
    case PRO_ETH:    UseRGB = true; 
                     IOWater = -1;
                     break;
    case PRO_H20_B:  UseRGB = true; 
                     IOWater = 0;
                     WtrMtr = true;
                     break;
    case PRO_H20_2:  UseRGB = false; 
                     IOWater = 3;
                     RxP1 = 4;
                     TxO1 = 10;
                     P1Out = true;
                     WtrMtr = true;
                     break;    
    case _P1EP:      UseRGB = false; 
                     IOWater = -1;
                     P1Out = true;
                     WtrMtr = false;
                     break;       
    case P1NRG:      UseRGB = false; 
                     P1Out = true;
                     break;                                          
  }
#endif
  //pin modes
  pinMode(DTR_IO, OUTPUT);
  if ( LED !=-1  ) pinMode(LED, OUTPUT);
  if ( IOWater != -1 ) pinMode(IOWater, INPUT_PULLUP);   
  
  Debugf("Config set UseRGB [%s] IOWater [%d]\n", UseRGB ? "true" : "false", IOWater);
  // if ( UseRGB ) rgb.begin();
  
  // sign of life = ON during setup or change config
  SwitchLED( LED_ON, LED_BLUE );
  delay(2000);
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
  } // PRO_BRIDGE
  else if ( LEDenabled ) digitalWrite(LED, !digitalRead(LED)); else digitalWrite(LED, LED_OFF);
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
      digitalWrite(LED, color==LED_RED?LED_OFF:mode);
   }
}


//===========================================================================================

const char* getResetReason(){
    return resetReasons[rtc_get_reset_reason(0)];
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
    delay(500);
    ESP.restart();
    delay(2000);  
}

//===========================================================================================
bool bailout () // to prevent firmware from crashing!
{
  if (ESP.getFreeHeap() > 5500) return false; //do nothing
  
  DebugT(F("Bailout due to low heap --> reboot in 3 seconds")); Debugln(ESP.getFreeHeap());
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
  dest[0] = '\0';
  strcat(dest, src);
    
} // strCopy()


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
  float r =  0.0;
  int   p =  0;
  int   d = -1;
  
  r = strtof(s, NULL);
  p = (int)(r*pow(10, dec));
  r = p / pow(10, dec);
  //DebugTf("[%s][%d] => p[%d] -> r[%f]\r\n", s, dec, p, r);
  return r; 

} //  strToFloat()


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
