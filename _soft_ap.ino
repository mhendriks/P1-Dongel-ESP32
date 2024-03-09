#ifdef AP_ONLY

void startAP(){
  Debugln("startAP");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(settingHostname, bAuthPW);

  IPAddress IP = WiFi.softAPIP();
  Debug("AP IP address: ");
  Debuglnln(IP);
}

#endif
