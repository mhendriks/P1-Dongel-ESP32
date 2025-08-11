#include <WebSocketsClient.h>  // Links2004
#include "DSMRloggerAPI.h"  // voor bestaande helpers/vars

bool g_streamEnabled = false;
WebSocketsClient proxyWS;
WiFiClientSecure secureClient;

// static String g_token;          // bij “simpel starten”: per boot random
static String g_pwd;            // bestaand configuratie-wachtwoord (of vaste string)
static bool   g_wsConnected = false;

static String sha256(const String& s); // fwd decl (implementeer onderaan)

// static String generateToken() {
//   uint32_t r = esp_random();
//   char buf[9]; sprintf(buf, "%08X", r);
//   return String(buf);
// }
// ISRG Root X1 - geldig tot 4 juni 2035
static const char ISRG_Root_X1[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgISA8nU+4EyX6QY9IBhzp0d+6pAMA0GCSqGSIb3DQEBCwUA
MEoxCzAJBgNVBAYTAlVTMRMwEQYDVQQKEwpMZXQncyBFbmNyeXB0MR8wHQYDVQQD
ExZJU1JHIFJvb3QgWDEgQ2VydGlmaWNhdGUwHhcNMTUwNjA0MTEwNDM4WhcNMzUw
NjA0MTEwNDM4WjBKMQswCQYDVQQGEwJVUzETMBEGA1UEChMKTGV0J3MgRW5jcnlw
dDEfMB0GA1UEAxMWSVNSRyBSb290IFgxIENlcnRpZmljYXRlMIICIjANBgkqhkiG
9w0BAQEFAAOCAg8AMIICCgKCAgEA7Fuhd0MDYyI27YFVrH6z4+O/FvT2p3xjCOqy
RmDkkZ/ldc6MjiP2uVtoNcTz4k+3li+BI9sz6Dn/Eg0U4wf/NK2xV2b5ov/n9gYy
aGni4xHnKKwR3Ujvvg4rRB8YlfDYNV2rD0t2mS9EtJ/M75Gjsi/W5SRFJ3pcIW7l
UUl1IXmknlKl+4aGehXbbPfLykNqibA8omcQxFweufKdTgd6f38F2c2JZjw7fNQw
gWjGDEOWAwF/fUNUJvGriS9WjrcHirLKNChZfqlYJfNYIBfFjI7HSwxwl9zgEVVk
C4hd6V4WjNU6pU3FI6P2C3lTya/U7U1sC1GbmNwx3kHxjk32yZUY8CcX+4w+IURo
YTdYF48du9+GaO2tqvwymzjmzGQ/t+h6+c8cN39/V6yR0lZ3KQf1MvL5YEd8J3bP
C1IwW4uTPbe3V5dEJjIrksHMxj2Zl+H2zkZ4k9m6SD8DHqAWBCmXznpP1Otrj4eO
r7me3Z1+RZBgwR1vGkuGS6O7F/3uOE9Nnkl90lSBpUABXKtSH3QPmD93FWrS6DKW
7uwP5G3zfTbLw5gtdG6rFBbAh+URQPyFPfQPFDqhV8WSojK3kbZ9nLsTbNQO1cb4
lyh3oSNH1N53gpd8Qqusz/kD3AKGf3JHjZp+RzeLMQ2JZ06YqAKG2uHbR7bWc1yb
I46LdgC58f8CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC
AYYwHQYDVR0OBBYEFBQusxe3WFbLrlAJQOYfr52LFMLGMA0GCSqGSIb3DQEBCwUA
A4IBAQAKtGXm0sjZK1J7MdX2UAD1+I0+cfS4xV2aGGb9IHG7T8e3gRjD+IaWypv0
fZbmYquq6s3bXcTRpT67YH9xUZ22cmbrHH6ktqdc+GkL5d+p8KEF7gf/ujQ5FHPF
bRjd9iOiRU+pr+R57I56HuBktjRvG2Hs+u9mV1Cdz64hnAc/gbc62B7qP4Ec3GU1
7Gfz1xFRYPNlTFCvNKPGLELDjJgiRmQeE3JGZ7m4b9WEMoZJLl02xVlwZVn5IZAn
4gykY2b8vmBxR3LkY3HrXtE/NaYdxL5cJAuWIttoZon1U1zc2nAGNzvP2kXoe6zu
xJJdRJ6rFRF2Anz0M2M7sBUFOoXr
-----END CERTIFICATE-----
)EOF";

void proxyWsSendHello() {
  DynamicJsonDocument doc(256);
  doc["type"]     = "hello";
  doc["id"]       = String(macID);      // je hebt al MAC helpers in Network.ino
  doc["token"]    = String(_getChipId()).c_str();
  doc["pwd_hash"] = sha256(g_pwd);
  String out; serializeJson(doc, out);
  proxyWS.sendTXT(out);
}

void proxyWsOnMessage(char* payload, size_t length) {
  // Eenvoudige router voor RPC’s vanuit de server/browser
  StaticJsonDocument<512> in;

  Debugf("WSOnMessage payload: %s\n", payload);
  
  DeserializationError err = deserializeJson(in, payload, length);
  if (err) return;

  const char* type = in["type"] | "";
  if (strcmp(type, "ping") == 0) {
    proxyWS.sendTXT("{\"type\":\"pong\"}");
    return;
  }

  // Voorbeeld: status opvragen
  if (strcmp(type, "get") == 0 && strcmp(in["what"]|"", "status") == 0) {
    DynamicJsonDocument resp(1024);
    resp["type"] = "status";
    // Vul met bestaande statusvelden uit Status.ino / Time.ino / P1.ino
    resp["ip"]      = IP_Address();
    resp["rssi"]    = WiFi.RSSI();
    resp["version"] = _VERSION_ONLY;
    // ... voeg toe wat jij nuttig vindt ...
    String out; serializeJson(resp, out);
    proxyWS.sendTXT(out);
    return;
  }

  // Voorbeeld: live stream aan/uit (server vraagt of jij telegrammen gaat pushen)
  if (strcmp(type, "stream") == 0) {
    bool enable = in["enable"] | false;
    // Zet een globale vlag die je in postTelegram() gebruikt om door te sturen
    g_streamEnabled = enable;
    proxyWS.sendTXT("{\"type\":\"ack\",\"cmd\":\"stream\"}");
    return;
  }

  // Voeg hier later "set" acties/commando’s toe (b.v. relais schakelen, config opslaan, enz.)
}

// void proxyWsEvent(WStype_t type, uint8_t * payload, size_t length) {
//   switch (type) {
//     case WStype_CONNECTED:
//       g_wsConnected = true;
//       proxyWsSendHello();
//       break;
//     case WStype_DISCONNECTED:
//       g_wsConnected = false;
//       break;
//     case WStype_TEXT:
//       proxyWsOnMessage((char*)payload, length);
//       break;
//     default: break;
//   }
// }

void startProxyWS() {
  // INIT secrets “simpel”: token per boot; pwd uit je bestaande config/UI
  // if (g_token.isEmpty()) g_token = _getChipId();

  if (g_pwd.isEmpty())   g_pwd   = "test123"; // of laad uit je bestaande config

  Debugf("WS token: %s en pw: %s\n", String(_getChipId()).c_str(), g_pwd);

  // Synology reverse proxy terminate TLS → ESP32 praat WSS
  // proxyWS.setInsecure();
  proxyWS.setCACert(ISRG_Root_X1);
  proxyWS.beginSSL("proxy.smart-stuff.nl", 443, "/ws");
  proxyWS.setReconnectInterval(5000);

  proxyWS.onEvent([](WStype_t type, uint8_t* payload, size_t length){
    switch (type) {
      case WStype_CONNECTED:
        g_wsConnected = true;
        proxyWsSendHello();
        break;
      case WStype_DISCONNECTED:
        g_wsConnected = false;
        break;
      case WStype_TEXT:
        proxyWsOnMessage((char*)payload, length);
        break;
      default: break;
    }
  });
}

void handleProxyWSLoop() {
  proxyWS.loop();
}

// heel eenvoudige SHA-256 (optioneel kun je ESP-IDF mbedTLS gebruiken)
#include <mbedtls/sha256.h>
static String sha256(const String& s){
  uint8_t out[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0); // 0 = SHA-256, 1 = SHA-224
  mbedtls_sha256_update(&ctx, (const unsigned char*)s.c_str(), s.length());
  mbedtls_sha256_finish(&ctx, out);
  mbedtls_sha256_free(&ctx);

  char hex[65];
  for (int i = 0; i < 32; i++) sprintf(hex + i*2, "%02x", out[i]);
  hex[64] = 0;
  return String(hex);
}
