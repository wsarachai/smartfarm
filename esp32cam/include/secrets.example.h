#pragma once
// -----------------------------------------------------------------------------
// TEMPLATE — copy this file to `secrets.h` (same folder) and fill in real values.
// `secrets.h` is git-ignored so your credentials never get committed.
//
//     cp include/secrets.example.h include/secrets.h
//
// -----------------------------------------------------------------------------

// --- WiFi ---
#define WIFI_SSID "YourNetworkName"
#define WIFI_PASS "YourNetworkPassword"

// --- Addressing ---
// The firmware uses DHCP; ap-server assigns the address. To pin the camera to a
// fixed address, add a MAC->IP reservation for this board in ap-server's web UI
// (http://192.168.0.1/edit) — do NOT hard-code a static IP here.

// --- mDNS hostname --- reachable as http://<hostname>.local/ on supporting OSes.
#define MDNS_HOSTNAME "esp32cam"

// --- OTA --- password required to push firmware over WiFi.
// MUST match `upload_flags = --auth=...` for the esp32cam_ota env in platformio.ini.
#define OTA_PASSWORD "change-me-ota-pass"

// --- Periodic snapshot push --- POST a JPEG to PUSH_URL every PUSH_INTERVAL_MS.
// Set PUSH_ENABLED to 0 to disable. Coexists with the on-demand /capture endpoint.
#define PUSH_ENABLED     0
#define PUSH_URL         "http://192.168.1.20:3000/ingest"
#define PUSH_INTERVAL_MS 10000

// --- Rolling SD-card save --- also write each pushed frame to the microSD card,
// deleting the oldest files when free space drops below SD_MIN_FREE_KB so the
// card never fills. Uses 1-bit SD mode (keeps GPIO4 free for the flash LED).
#define SD_SAVE_ENABLED  0
#define SD_DIR           "/frames"
#define SD_MIN_FREE_KB   51200        // keep at least 50 MB free
