// ESP32-CAM IP Camera — v1
// AI-Thinker board, PlatformIO + Arduino. Live MJPEG stream + snapshot, LAN-only.
//
// Boot flow: init camera (SVGA / q12 / fb_count 2 / GRAB_LATEST)
//            -> connect WiFi via DHCP (address assigned by ap-server)
//            -> start mDNS -> start HTTP servers.

#include "esp_camera.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "esp_task_wdt.h"

// Task watchdog: if loop() stops feeding it for this long (an acute hang —
// wedged WiFi stack, blocked capture), the chip resets itself.
#define WDT_TIMEOUT_S 30

#include "secrets.h"
#include "camera_pins.h"

// v2 health telemetry defaults — override in secrets.h. TELEMETRY_URL empty
// disables the health POST, so an older secrets.h still compiles and runs.
#ifndef DEVICE_ID
#define DEVICE_ID "esp32cam"
#endif
#ifndef FW_VERSION
#define FW_VERSION "2.0.0"
#endif
#ifndef TELEMETRY_URL
#define TELEMETRY_URL ""
#endif
#ifndef CONFIG_URL
#define CONFIG_URL ""
#endif

// Runtime state pulled from the hub's /config each cycle (camera-v2). Seeded
// from the compile-time default, then overridden by server config.
static uint32_t g_snapshotIntervalMs  = PUSH_INTERVAL_MS;
static bool     g_enabled             = true;
static uint32_t g_rebootIntervalHours = 24;  // local fallback reboot (0 = off)

#if SD_SAVE_ENABLED
#include "FS.h"
#include "SD_MMC.h"
#endif

// Defined in app_httpd.cpp — starts the control server (port 80) and stream
// server (port 81). Returns true if the control UI (:80) started.
bool startCameraServer();
// Defined in app_httpd.cpp — grab a validated (complete) JPEG frame.
camera_fb_t *grab_validated_frame(int max_tries);

static void initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  // 10 MHz (vs the usual 20) gives the DMA capture path maximum margin against
  // dropped bytes, which show up as random colored horizontal lines. This is an
  // AI feed — FPS is irrelevant, clean frames are everything.
  config.xclk_freq_hz = 10000000;
  config.frame_size   = FRAMESIZE_UXGA;         // 1600x1200 — max detail for AI
  config.pixel_format = PIXFORMAT_JPEG;         // streaming
  config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY; // only complete, in-order frames
  config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;                  // lower = better quality / bigger; 12 is the safe floor at UXGA
  config.fb_count     = 2;                   // double buffer (needs PSRAM)

  if (!psramFound()) {
    // No PSRAM: fall back to a config that fits internal RAM so we still boot.
    Serial.println("[cam] WARNING: PSRAM not found — falling back to VGA/single buffer");
    config.frame_size  = FRAMESIZE_VGA;
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.fb_count    = 1;
    config.grab_mode   = CAMERA_GRAB_WHEN_EMPTY;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[cam] init failed (0x%x) — check ribbon cable & power, then reset\n", err);
    // Nothing works without the sensor; reboot to retry rather than hang half-alive.
    delay(3000);
    ESP.restart();
  }

  // Sensor is up. Report which one and apply a couple of sensible defaults.
  sensor_t *s = esp_camera_sensor_get();
  Serial.printf("[cam] sensor PID: 0x%x (%s)\n", s->id.PID,
                s->id.PID == OV3660_PID ? "OV3660" :
                s->id.PID == OV2640_PID ? "OV2640" : "unknown");

  if (s->id.PID == OV3660_PID) {
    // OV3660 ships slightly washed out & upside-down on this board — correct it.
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
  }
}

static void connectWiFi() {
  // DHCP: let ap-server assign our address (reserved band if the AP has a
  // MAC reservation for us, otherwise a dynamic .100+ lease).
  WiFi.mode(WIFI_STA);
  // Advertise our hostname BEFORE begin() so ap-server's DHCP server captures
  // it (DHCP option 12) and labels us "esp32cam" in its client list.
  WiFi.setHostname(MDNS_HOSTNAME);

  Serial.printf("[wifi] connecting to \"%s\" (DHCP)", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  WiFi.setSleep(false);  // keep the stream responsive

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() - start > 20000) {  // 20s: give up and reboot to retry cleanly
      Serial.println("\n[wifi] timeout — restarting");
      ESP.restart();
    }
  }
  // Reduce radio TX power to shrink the current spikes that stack on top of the
  // camera's capture spikes (a cause of frame corruption on power-marginal boards).
  // Safe here — signal is strong. Raise if the link ever drops.
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  // Associated. Confirm DHCP actually handed us an address — WiFi can report
  // WL_CONNECTED before (or without) an IP if the DHCP server is slow/broken.
  IPAddress ip = WiFi.localIP();
  if (ip == IPAddress(0, 0, 0, 0)) {
    Serial.println("\n[wifi] associated, waiting for DHCP IP from ap-server...");
    uint32_t t = millis();
    while ((ip = WiFi.localIP()) == IPAddress(0, 0, 0, 0)) {
      delay(200);
      if (millis() - t > 8000) {
        Serial.println("[wifi] DHCP FAILED — no IP from ap-server; restarting");
        ESP.restart();
      }
    }
  }

  String mac = WiFi.macAddress();
  Serial.println();
  Serial.println("[wifi] DHCP OK — IP obtained from ap-server:");
  Serial.printf("[wifi]   SSID   : %s  (RSSI %d dBm, TX power reduced)\n",
                WIFI_SSID, WiFi.RSSI());
  Serial.printf("[wifi]   MAC    : %s\n", mac.c_str());
  Serial.printf("[wifi]   IP     : %s\n", ip.toString().c_str());
  Serial.printf("[wifi]   Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
  Serial.printf("[wifi]   Subnet : %s\n", WiFi.subnetMask().toString().c_str());
  Serial.printf("[wifi]   DNS    : %s\n", WiFi.dnsIP().toString().c_str());
  Serial.printf("[wifi]   Tip: reserve MAC %s in ap-server for a fixed IP.\n",
                mac.c_str());
}

static void setupOTA() {
  ArduinoOTA.setHostname(MDNS_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() { Serial.println("\n[ota] update starting"); });
  ArduinoOTA.onEnd([]()   { Serial.println("\n[ota] done — rebooting"); });
  ArduinoOTA.onProgress([](unsigned int p, unsigned int t) {
    Serial.printf("[ota] %u%%\r", (p * 100) / t);
  });
  ArduinoOTA.onError([](ota_error_t e) { Serial.printf("[ota] error %u\n", e); });
  ArduinoOTA.begin();
  Serial.println("[ota] ready for wireless updates");
}

// Arm the task watchdog on the loop task. Armed AFTER boot so the one-time
// camera/WiFi init (which can take many seconds) isn't judged a hang; it guards
// only the steady-state loop, which must call esp_task_wdt_reset() each pass.
static void setupWatchdog() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  // Core v3 already initializes the TWDT (for idle tasks), so reconfigure it.
  esp_task_wdt_config_t cfg = {
    .timeout_ms = WDT_TIMEOUT_S * 1000,
    .idle_core_mask = 0,
    .trigger_panic = true,
  };
  esp_task_wdt_reconfigure(&cfg);
#else
  esp_task_wdt_init(WDT_TIMEOUT_S, true);
#endif
  esp_task_wdt_add(NULL);  // subscribe the loop task (no-op if already added)
  Serial.printf("[wdt] task watchdog armed (%ds)\n", WDT_TIMEOUT_S);
}

#if SD_SAVE_ENABLED
// Rolling storage on the microSD card. Files are named /<dir>/<8-digit>.jpg
// (FAT 8.3-safe). We track the newest index to write and the oldest index to
// delete; when free space runs low we delete from the oldest end. Sequential
// names give a reliable "oldest" without an RTC (file timestamps are unreliable).
static bool     g_sdReady   = false;
static uint32_t g_nextIdx   = 1;  // next filename index to write
static uint32_t g_oldestIdx = 1;  // oldest surviving filename index

static void initSD() {
  // 1-bit mode: uses GPIO2/14/15 only, leaving GPIO4 free for the flash LED.
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("[sd] mount failed — SD saving disabled");
    return;
  }
  if (SD_MMC.cardType() == CARD_NONE) {
    Serial.println("[sd] no card present — SD saving disabled");
    return;
  }
  if (!SD_MMC.exists(SD_DIR)) SD_MMC.mkdir(SD_DIR);

  // Resume the rolling range by scanning existing files for min/max index.
  bool any = false;
  File root = SD_MMC.open(SD_DIR);
  if (root && root.isDirectory()) {
    for (File f = root.openNextFile(); f; f = root.openNextFile()) {
      if (f.isDirectory()) continue;
      String n = String(f.name());
      int slash = n.lastIndexOf('/');
      if (slash >= 0) n = n.substring(slash + 1);
      uint32_t idx = (uint32_t)strtoul(n.c_str(), NULL, 10);
      if (idx == 0) continue;
      if (idx + 1 > g_nextIdx) g_nextIdx = idx + 1;
      if (!any || idx < g_oldestIdx) g_oldestIdx = idx;
      any = true;
    }
  }
  if (!any) { g_nextIdx = 1; g_oldestIdx = 1; }

  g_sdReady = true;
  Serial.printf("[sd] ready: %llu MB total, %llu MB used, resuming at #%u (oldest #%u)\n",
                SD_MMC.totalBytes() / (1024ULL * 1024), SD_MMC.usedBytes() / (1024ULL * 1024),
                g_nextIdx, g_oldestIdx);
}

// Delete oldest files until at least SD_MIN_FREE_KB is free (or nothing's left).
static void sdEnsureFreeSpace() {
  while (g_sdReady && g_oldestIdx < g_nextIdx) {
    uint64_t freeKB = (SD_MMC.totalBytes() - SD_MMC.usedBytes()) / 1024ULL;
    if (freeKB >= (uint64_t)SD_MIN_FREE_KB) return;
    // Advance to the next existing oldest file and remove it.
    bool removed = false;
    while (g_oldestIdx < g_nextIdx) {
      char p[48];
      snprintf(p, sizeof(p), "%s/%08u.jpg", SD_DIR, g_oldestIdx);
      g_oldestIdx++;
      if (SD_MMC.exists(p)) { SD_MMC.remove(p); removed = true; Serial.printf("[sd] rolled off %s\n", p); break; }
    }
    if (!removed) return;  // nothing found to delete
  }
}

static void saveToSD(const uint8_t *buf, size_t len) {
  if (!g_sdReady) return;
  sdEnsureFreeSpace();
  char p[48];
  snprintf(p, sizeof(p), "%s/%08u.jpg", SD_DIR, g_nextIdx);
  File f = SD_MMC.open(p, FILE_WRITE);
  if (!f) { Serial.printf("[sd] open %s failed\n", p); return; }
  size_t w = f.write(buf, len);
  f.close();
  if (w == len) { Serial.printf("[sd] saved %s (%u B)\n", p, (unsigned)w); g_nextIdx++; }
  else          Serial.printf("[sd] short write %s (%u/%u)\n", p, (unsigned)w, (unsigned)len);
}
#endif  // SD_SAVE_ENABLED

#if PUSH_ENABLED
// Capture one validated JPEG, save it to SD (rolling), and POST it (raw body,
// image/jpeg) to PUSH_URL.
static void pushSnapshot() {
  camera_fb_t *fb = grab_validated_frame(5);
  if (!fb) { Serial.println("[push] capture failed — skipping"); return; }

#if SD_SAVE_ENABLED
  saveToSD(fb->buf, fb->len);
#endif

  WiFiClient client;
  HTTPClient http;
  if (http.begin(client, PUSH_URL)) {
    http.addHeader("Content-Type", "image/jpeg");
    int code = http.POST(fb->buf, fb->len);
    if (code > 0) Serial.printf("[push] POST %s -> %d (%u bytes)\n", PUSH_URL, code, fb->len);
    else          Serial.printf("[push] POST failed: %s\n", http.errorToString(code).c_str());
    http.end();
  } else {
    Serial.println("[push] http.begin() failed (bad URL?)");
  }
  esp_camera_fb_return(fb);
}

// v2: report device health to the hub's generic telemetry endpoint so the
// camera shows up as a normal device card (heap/rssi/uptime/fw). Watching heap
// and uptime trend is how the hub catches a degrading board before it locks up.
static void pushTelemetry() {
  if (strlen(TELEMETRY_URL) == 0) return;  // disabled
  char body[256];
  snprintf(body, sizeof(body),
           "{\"device_id\":\"%s\",\"metrics\":{"
           "\"free_heap\":%u,\"rssi\":%ld,\"uptime_s\":%lu,\"fw_version\":\"%s\"}}",
           DEVICE_ID, (unsigned)ESP.getFreeHeap(), (long)WiFi.RSSI(),
           (unsigned long)(millis() / 1000UL), FW_VERSION);

  WiFiClient client;
  HTTPClient http;
  if (http.begin(client, TELEMETRY_URL)) {
    http.addHeader("Content-Type", "application/json");
    int code = http.POST((uint8_t *)body, strlen(body));
    if (code > 0) Serial.printf("[telem] POST %s -> %d\n", TELEMETRY_URL, code);
    else          Serial.printf("[telem] POST failed: %s\n", http.errorToString(code).c_str());
    http.end();
  }
}

static framesize_t framesizeFromStr(const char *s) {
  if (!strcmp(s, "QVGA")) return FRAMESIZE_QVGA;
  if (!strcmp(s, "CIF"))  return FRAMESIZE_CIF;
  if (!strcmp(s, "VGA"))  return FRAMESIZE_VGA;
  if (!strcmp(s, "SVGA")) return FRAMESIZE_SVGA;
  if (!strcmp(s, "XGA"))  return FRAMESIZE_XGA;
  if (!strcmp(s, "HD"))   return FRAMESIZE_HD;
  if (!strcmp(s, "SXGA")) return FRAMESIZE_SXGA;
  if (!strcmp(s, "UXGA")) return FRAMESIZE_UXGA;
  return FRAMESIZE_INVALID;
}

// v2: pull behavior config from the hub and apply deltas (interval, framesize,
// quality, enable). No-ops if CONFIG_URL is unset or unreachable, so the camera
// keeps running on its last-known config through a server outage.
static void fetchConfig() {
  if (strlen(CONFIG_URL) == 0) return;
  WiFiClient client;
  HTTPClient http;
  if (!http.begin(client, CONFIG_URL)) return;
  bool doReboot = false;
  int code = http.GET();
  if (code == 200) {
    JsonDocument doc;
    if (deserializeJson(doc, http.getString()) == DeserializationError::Ok) {
      if (!doc["snapshot_interval_ms"].isNull()) {
        uint32_t v = doc["snapshot_interval_ms"].as<uint32_t>();
        if (v >= 5000 && v <= 3600000) g_snapshotIntervalMs = v;
      }
      if (!doc["enabled"].isNull()) g_enabled = doc["enabled"].as<bool>();
      if (!doc["reboot_interval_hours"].isNull()) {
        uint32_t h = doc["reboot_interval_hours"].as<uint32_t>();
        if (h <= 168) g_rebootIntervalHours = h;
      }
      // Server-driven health reboot: honored after we finish reading the body.
      doReboot = doc["reboot"].is<bool>() && doc["reboot"].as<bool>();
      sensor_t *s = esp_camera_sensor_get();
      if (s) {
        const char *fs = doc["framesize"] | "";
        framesize_t f = framesizeFromStr(fs);
        if (f != FRAMESIZE_INVALID) s->set_framesize(s, f);
        if (!doc["jpeg_quality"].isNull()) {
          int q = doc["jpeg_quality"].as<int>();
          if (q >= 4 && q <= 63) s->set_quality(s, q);
        }
      }
    }
  } else {
    Serial.printf("[config] GET %s -> %d\n", CONFIG_URL, code);
  }
  http.end();

  if (doReboot) {
    Serial.println("[reboot] server-commanded (health) — restarting");
    delay(200);  // let the serial line flush
    ESP.restart();
  }
}
#endif

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  Serial.println("\n\n=== ESP32-CAM IP Camera (v1) ===");

  initCamera();
#if SD_SAVE_ENABLED
  initSD();
#endif
  connectWiFi();

  if (MDNS.begin(MDNS_HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("[mdns] http://%s.local/\n", MDNS_HOSTNAME);
  } else {
    Serial.println("[mdns] failed to start (browse by IP instead)");
  }

  setupOTA();

  if (startCameraServer()) {
    Serial.printf("[http] web UI ready: http://%s/   (stream on :81)\n",
                  WiFi.localIP().toString().c_str());
  } else {
    Serial.println("[http] WARNING: web UI did not start (control server failed to bind)");
  }

  setupWatchdog();  // arm last — steady-state loop guard only
}

void loop() {
  esp_task_wdt_reset();  // feed the watchdog; a wedged loop stops feeding -> reset
  ArduinoOTA.handle();   // must be serviced often for wireless updates to work

  // Throttled link check: reconnect if WiFi drops (DHCP re-leases on reconnect).
  static uint32_t lastCheck = 0;
  if (millis() - lastCheck > 5000) {
    lastCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[wifi] link lost — reconnecting");
      WiFi.reconnect();
    }
  }

  // Local fallback reboot (camera-v2): if the server can't be reached to command
  // a health reboot, still recycle on our own schedule to clear slow degradation
  // and recover WiFi/DHCP. 0 disables. Server-driven reboot is the primary path.
  if (g_rebootIntervalHours > 0 && (millis() / 3600000UL) >= g_rebootIntervalHours) {
    Serial.println("[reboot] local uptime fallback — restarting");
    delay(200);
    ESP.restart();
  }

  // Periodic status heartbeat so the serial monitor always shows proof-of-life
  // and the current IP, even if you attach it long after boot.
  static uint32_t lastStatus = 0;
  if (millis() - lastStatus > 15000) {
    lastStatus = millis();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[status] up %lus  IP %s  RSSI %d dBm  heap %u\n",
                    (unsigned long)(millis() / 1000),
                    WiFi.localIP().toString().c_str(), WiFi.RSSI(),
                    (unsigned)ESP.getFreeHeap());
    } else {
      Serial.printf("[status] up %lus  WiFi DISCONNECTED\n",
                    (unsigned long)(millis() / 1000));
    }
  }

#if PUSH_ENABLED
  // Periodic cycle: snapshot + health telemetry (when enabled), then pull config
  // so a dashboard change (interval / resolution / enable) applies within one
  // cycle. Config is fetched even while disabled, so the camera can be re-enabled
  // remotely. Blocks the loop briefly; the HTTP server tasks keep serving.
  static uint32_t lastCycle = 0;
  if (WiFi.status() == WL_CONNECTED && millis() - lastCycle >= g_snapshotIntervalMs) {
    lastCycle = millis();
    if (g_enabled) {
      pushSnapshot();
      pushTelemetry();
    }
    fetchConfig();
  }
#endif

  delay(10);
}
