// #include <esp/esp_websocket_client.h>

// const char* websocket_host = "127.0.0.1";
// const uint16_t websocket_port = 8000;

// esp_websocket_client_handle_t webSocket;


// void websocketEventHandler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
// {
//     esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
//     switch (event_id)
//     {
//     case WEBSOCKET_EVENT_CONNECTED:
//         DebugTf("WEBSOCKET_EVENT_CONNECTED");
//         break;
//     case WEBSOCKET_EVENT_DISCONNECTED:
//         DebugTf("WEBSOCKET_EVENT_DISCONNECTED");
//         break;
//     case WEBSOCKET_EVENT_DATA:
//         DebugTf("WEBSOCKET_EVENT_DATA");
//         DebugTf("Received opcode=%d", data->op_code);
//         if (data->op_code == 0x08 && data->data_len == 2)
//         {
//             DebugTf("Received closed message with code=%d", 256 * data->data_ptr[0] + data->data_ptr[1]);
//         }
//         else
//         {
//             DebugTf("Received=%.*s", data->data_len, (char *)data->data_ptr);
//         }
//         DebugTf("Total payload length=%d, data_len=%d, current payload offset=%d\r\n", data->payload_len, data->data_len, data->payload_offset);
//         break;
//     case WEBSOCKET_EVENT_ERROR:
//         DebugTf("WEBSOCKET_EVENT_ERROR");
//         break;
//     }
// }

// void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
//   switch(type) {
//     case WStype_DISCONNECTED:
//       Debugln("WS Disconnected");
//       break;
//     case WStype_CONNECTED:
//       Debugln("WS Connected to server");
//       webSocket.sendTXT(macID);  // Identify
//       break;
//     case WStype_TEXT:
//       Debugf("Command: %s\n", payload);
//       // Hier verwerk je commando van server
//       break;
//   }
// }

void setupWS() {
  // webSocket.begin(websocket_host, websocket_port, "/");
  // webSocket.onEvent(webSocketEvent);
  // webSocket.setReconnectInterval(5000);


//   esp_websocket_client_config_t websocket_cfg = {};
//   websocket_cfg.uri = "ws://192.168.160.16:9000/vgtuhd";
//   websocket_cfg.subprotocol = "ocpp2.0.1";
//   webSocket = esp_websocket_client_init(&websocket_cfg);
//   esp_websocket_register_events(webSocket, WEBSOCKET_EVENT_ANY, websocketEventHandler, (void *)webSocket);
//   esp_websocket_client_start(webSocket);

}

void handleWS() {
  // webSocket.loop();
}