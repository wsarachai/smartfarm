#ifndef WEB_SERVER_H
#define WEB_SERVER_H
// Relay-control HTTP server. Contract is identical to the ESP32 pump-zone so this
// node is interchangeable from the Node hub / dashboard's point of view:
//
//   POST /api/v1/relay   body {"state":"on"|"off"}  -> switch pump
//   GET  /api/v1/relay                              -> current state
//   both reply {"relay_status":"ON"|"OFF"[, "remaining_ms":N][, "safety_off":true]}
//
// No auth — WPA2 on the AP is the gate (same as pump-zone).

void web_server_begin(void);  // register routes + start listening (call once, after WiFi is up)
void web_server_loop(void);   // call frequently from loop() to service clients

#endif // WEB_SERVER_H
