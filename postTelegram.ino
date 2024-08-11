void PostTelegram() {

#ifdef POST_TELEGRAM
  if ( !strlen(pt_end_point) ) return;
  if ( ( (esp_log_timestamp()/1000) - TelegramLastPost) < pt_interval ) return;
  
  TelegramLastPost = esp_log_timestamp()/1000;
  
  HTTPClient http;
  http.begin(wifiClient, pt_end_point, pt_port);
  
  http.addHeader("Content-Type", "text/plain");
  http.addHeader("Accept", "*/*");
  
  int httpResponseCode = http.POST( CapTelegram );
  DebugT(F("HTTP Response code: "));Debugln(httpResponseCode);
  
  http.end();  
#endif
  
}
