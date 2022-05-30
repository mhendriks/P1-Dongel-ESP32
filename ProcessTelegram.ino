/* 
***************************************************************************  
**  Program  : processTelegram, part of DSMRloggerAPI
**  Version  : v4.2.1
**
**  Copyright (c) 2021 Willem Aandewiel / Martijn Hendriks
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/
//==================================================================================
void processTelegram(){
  DebugTf("Telegram[%d]=>DSMRdata.timestamp[%s]\r\n", telegramCount, DSMRdata.timestamp.c_str());  
                                                    
  strcpy(newTimestamp, DSMRdata.timestamp.c_str()); 

  newT = epoch(newTimestamp, strlen(newTimestamp), true); // update system time
  actT = epoch(actTimestamp, strlen(actTimestamp), false);
  
  // Skip first 2 telegrams .. just to settle down a bit ;-)
  
  if ((int32_t)(telegramCount - telegramErrors) < 2) return;
  
  strCopy(actTimestamp, sizeof(actTimestamp), newTimestamp);  // maar nog NIET actT!!!
  DebugTf("actHour[%02d] -- newHour[%02d]\r\n", hour(actT), hour(newT));
  
  // has the hour changed (or the day or month)  
  // in production testing on hour only would
  // suffice, but in testing I need all three
  if (     (hour(actT) != hour(newT)  ) ) 
//       ||   (day(actT) != day(newT)   ) 
//       || (month(actT) != month(newT) ) )
  {
    writeRingFiles();
  }

  if ( DUE(publishMQTTtimer) ) sendMQTTData();

} // processTelegram()


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
***************************************************************************ƒ/
