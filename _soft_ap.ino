#ifdef AP_ONLY

void startAP(){
  Debugln("startAP");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(_DEFAULT_HOSTNAME, bAuthPW);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
}

#endif
