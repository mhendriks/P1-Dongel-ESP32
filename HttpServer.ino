/*
***************************************************************************
**  Program  : HttpServer
**  Purpose  : service the local synchronous WebServer
***************************************************************************
*/

void handleHttpServerClient() {
  if (factoryResetPending || !localHttpEnabled()) return;
  httpServer.handleClient();
}
