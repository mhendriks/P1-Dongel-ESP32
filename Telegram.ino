/* 
***************************************************************************  
**  Program  : DSMRloggerAPI
**  Version  : v4.0.0
**
**  Copyright (c) 2022 Martijn Hendriks 
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

#ifdef USE_TELEGRAM

#include <WiFiClientSecure.h> 

char bottoken[50] =""; 
char chatid[15] =""; 
char host[30] =""; 
char fingerprint[100] ="";  //sha256 of first (leaf) certificate in the tree

const PROGMEM char *msg[] {"DSMR|overload: 25 Ampere", "DSMR|Hoogste Stroom dagverbruik  sinds 3 dagen"}; 

WiFiClientSecure https;
byte cnt = 0;
//=======================================================================

void ReadBotSettings(){
  StaticJsonDocument<380> doc;  
  if (!FSmounted) return;
  File file = LittleFS.open("/BotSettings.json", "r");
  if (!file) {
    DebugTln("read(): No /BotSettings.json found");
    return;
  }
  
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    DebugTln(F("read():Failed to read BotSettings.json file"));
    return;
  }
  file.close();

  strcpy(host, doc["host"]);
  strcpy(fingerprint, doc["fingerprint"]);
  strcpy(chatid, doc["chatid"]);
  strcpy(bottoken, doc["bottoken"]);
  
//  Debug("host: "); Debugln(host);
//  Debug("fingerprint:");Debugln(fingerprint);
//  Debug("chatid:" );Debugln(chatid);
//  Debug("bottoken: ");Debugln(bottoken);
}
      
void Bot_setup() {
  DebugTln("Bot Setup");
  
  ReadBotSettings();

  DebugTln("Bot setup Complete");
} //end bot_setup

//=======================================================================

void Bot_loop() {
  if (*host =='\0' || *fingerprint =='\0' || *chatid =='\0' || *bottoken =='\0' ) return; //no empty strings
  if (cnt >= 2) return;
  cnt++;
  https.setInsecure();
  DebugT("connecting to "); Debugln(host);
  if (!https.connect(host, 443)) {
    DebugTln("connection failed code");
    delay(500);
    return;
  }
  
  if (https.verify(fingerprint, host)) {
    Debugln("certificate OK!");
    https.print(String("GET https://api.telegram.org/bot") + bottoken + "/sendMessage?chat_id=" + chatid + "&text="+ msg[cnt-1] + cnt + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: ESP32BOT\r\n" +
               "Connection: close\r\n\r\n");
  
    while (https.connected()) {
      String line = https.readStringUntil('\n');
      if (line == "\r") {
        Debugln("headers received" );
        break;
      }
    }
    
    String line = https.readStringUntil('\n');
    if (line.startsWith("{\"ok\":true")) Debugln("Message successfull send!");
    else Debugln("failed");
   
  } //if verify
  else Debugln("certificate doesn't match");
 
  https.stop();
  delay(500);
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
