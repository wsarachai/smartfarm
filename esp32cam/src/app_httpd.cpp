// Stripped CameraWebServer for the ESP32-CAM IP Camera.
//
// Kept: control web UI (port 80), MJPEG stream (port 81), snapshot, live camera
//       controls (resolution, quality, brightness, mirror/flip, etc.).
// Removed: all face detection / recognition (esp-face) — saves flash & RAM and
//          keeps the stream fast.

#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "camera_module.h"
#include "camera_frame.h"

// ---- MJPEG multipart stream framing --------------------------------------
#define PART_BOUNDARY "123456789000000000000987654321"
static const char *STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *STREAM_BOUNDARY     = "\r\n--" PART_BOUNDARY "\r\n";
static const char *STREAM_PART         = "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %d.%06d\r\n\r\n";

static httpd_handle_t camera_httpd = NULL;  // port 80: UI + control + capture
static httpd_handle_t stream_httpd = NULL;  // port 81: MJPEG stream

// Onboard white flash LED (AI-Thinker board). Driven via LEDC PWM so it can be
// dimmed; uses channel 2 to stay clear of the camera's XCLK on channel 0.
#define LED_GPIO_NUM      4
#define LED_LEDC_CHANNEL  2
static int led_intensity = 0;  // 0 (off) .. 255 (full)

static void setupLedFlash() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  // Arduino-ESP32 core v3 uses ledcAttach(pin, freq, resolution).
  ledcAttach(LED_GPIO_NUM, 5000, 8);      // 5 kHz, 8-bit duty (0..255)
#else
  // Arduino-ESP32 core v2 uses channel-based setup + pin attach.
  ledcSetup(LED_LEDC_CHANNEL, 5000, 8);   // 5 kHz, 8-bit duty (0..255)
  ledcAttachPin(LED_GPIO_NUM, LED_LEDC_CHANNEL);
#endif
  ledcWrite(LED_LEDC_CHANNEL, led_intensity);  // start off
}

// ---- Web UI --------------------------------------------------------------
// Minimal but functional control panel. The stream <img> points at :81.
static const char INDEX_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32-CAM</title>
<style>
  body{font-family:system-ui,sans-serif;margin:0;background:#111;color:#eee}
  header{padding:10px 14px;background:#1b1b1b;font-weight:600}
  .wrap{display:flex;flex-wrap:wrap;gap:14px;padding:14px}
  .view{flex:1 1 480px;min-width:320px}
  img{width:100%;border-radius:8px;background:#000;display:block}
  .panel{flex:0 0 260px;background:#1b1b1b;border-radius:8px;padding:12px}
  .row{display:flex;align-items:center;justify-content:space-between;margin:10px 0}
  label{font-size:14px}
  input[type=range]{width:130px}
  select,button{background:#2a2a2a;color:#eee;border:1px solid #444;border-radius:6px;padding:6px 10px}
  button{cursor:pointer}
  button.primary{background:#2d6cdf;border-color:#2d6cdf}
  .btns{display:flex;gap:8px;margin-top:8px}
  .row.off{opacity:.4}
  .mod{font-weight:400;font-size:13px;color:#9aa}
</style>
</head>
<body>
<header>ESP32-CAM &middot; live <span class="mod" id="mod"></span></header>
<div class="wrap">
  <div class="view">
    <img id="stream" src="">
    <div class="btns">
      <button class="primary" id="toggle">Start stream</button>
      <button id="snap">Save snapshot</button>
    </div>
  </div>
  <div class="panel">
    <div class="row">
      <label for="framesize">Resolution</label>
      <select id="framesize">
        <option value="13">UXGA 1600x1200</option>
        <option value="11">HD 1280x720</option>
        <option value="9">XGA 1024x768</option>
        <option value="8" selected>SVGA 800x600</option>
        <option value="6">VGA 640x480</option>
        <option value="5">CIF 400x296</option>
        <option value="4">QVGA 320x240</option>
      </select>
    </div>
    <div class="row"><label>Quality</label><input type="range" id="quality" min="4" max="63" value="12"></div>
    <div class="row"><label>Brightness</label><input type="range" id="brightness" min="-2" max="2" value="0"></div>
    <div class="row"><label>Contrast</label><input type="range" id="contrast" min="-2" max="2" value="0"></div>
    <div class="row"><label>Saturation</label><input type="range" id="saturation" min="-2" max="2" value="0"></div>
    <div class="row"><label>Flash LED</label><input type="range" id="led_intensity" min="0" max="255" value="0"></div>
    <div class="row"><label for="hmirror">H-Mirror</label><input type="checkbox" id="hmirror"></div>
    <div class="row"><label for="vflip">V-Flip</label><input type="checkbox" id="vflip"></div>
  </div>
</div>
<script>
  var base = document.location.origin;
  var streamUrl = base + ':81/stream';
  var img = document.getElementById('stream');
  var toggle = document.getElementById('toggle');
  var streaming = false;

  function startStream(){ img.src = streamUrl; streaming = true; toggle.textContent = 'Stop stream'; }
  function stopStream(){ window.stop(); img.src = ''; streaming = false; toggle.textContent = 'Start stream'; }
  toggle.onclick = function(){ streaming ? stopStream() : startStream(); };

  document.getElementById('snap').onclick = function(){
    var a = document.createElement('a');
    a.href = base + '/capture?_=' + Date.now();
    a.download = 'esp32cam_' + Date.now() + '.jpg';
    document.body.appendChild(a); a.click(); a.remove();
  };

  function setControl(v, val){ fetch(base + '/control?var=' + v + '&val=' + val); }
  function bindRange(id){ var e = document.getElementById(id); e.onchange = function(){ setControl(id, e.value); }; }
  ['quality','brightness','contrast','saturation','framesize','led_intensity'].forEach(bindRange);
  document.getElementById('hmirror').onchange = function(){ setControl('hmirror', this.checked?1:0); };
  document.getElementById('vflip').onchange   = function(){ setControl('vflip',   this.checked?1:0); };

  // Load current sensor state so the controls reflect reality.
  fetch(base + '/status').then(r=>r.json()).then(function(s){
    for (var k in s){ var e = document.getElementById(k); if(!e) continue;
      if (e.type === 'checkbox') e.checked = !!s[k]; else e.value = s[k]; }

    document.getElementById('mod').textContent =
      '· ' + s.module + (s.native_jpeg ? '' : ' · software JPEG');

    // Hide resolutions above what this module can do — on a software-encoding
    // sensor the ceiling is PSRAM and the encoder's fixed output buffer, so
    // offering UXGA would just fail silently.
    var fsSel = document.getElementById('framesize');
    Array.prototype.forEach.call(fsSel.options, function(o){
      o.hidden = o.disabled = (+o.value > s.max_framesize);
    });

    // The GC2145 driver implements none of these, so don't pretend they work.
    if (!s.adjustable) {
      ['brightness','contrast','saturation'].forEach(function(id){
        var e = document.getElementById(id);
        e.disabled = true; e.closest('.row').classList.add('off');
      });
    }
  }).catch(()=>{});

  startStream();
</script>
</body>
</html>)HTML";

// ---- Handlers ------------------------------------------------------------

static esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Content-Encoding", "identity");
  return httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
}

// Frame acquisition (including the software-encode path for modules without a
// hardware JPEG encoder) lives in camera_frame.cpp — these handlers only ever
// see a finished JPEG.

static esp_err_t capture_handler(httpd_req_t *req) {
  jpeg_frame_t f;
  if (!jpeg_frame_grab(&f, 5)) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  esp_err_t res = httpd_resp_send(req, (const char *)f.buf, f.len);
  jpeg_frame_release(&f);
  return res;
}

static esp_err_t stream_handler(httpd_req_t *req) {
  esp_err_t res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "X-Framerate", "60");

  char part_buf[128];  // must fit the full header incl. 10-digit epoch + usec
  while (true) {
    // Fewer retries than the snapshot push (5): the live view would rather show
    // a slightly imperfect frame than stall, but still discards the obviously
    // truncated ones so it doesn't flash garbage.
    jpeg_frame_t f;
    if (!jpeg_frame_grab(&f, 3)) { res = ESP_FAIL; break; }

    struct timeval ts; gettimeofday(&ts, NULL);
    size_t hlen = snprintf(part_buf, sizeof(part_buf), STREAM_PART,
                           f.len, (int)ts.tv_sec, (int)ts.tv_usec);

    if ((res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY))) == ESP_OK)
      res = httpd_resp_send_chunk(req, part_buf, hlen);
    if (res == ESP_OK)
      res = httpd_resp_send_chunk(req, (const char *)f.buf, f.len);

    jpeg_frame_release(&f);
    if (res != ESP_OK) break;  // client disconnected
  }
  return res;
}

// /control?var=<name>&val=<int> — live sensor tweaks from the UI.
static esp_err_t control_handler(httpd_req_t *req) {
  char query[128]; char var[32]; char valstr[16];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
      httpd_query_key_value(query, "var", var, sizeof(var)) != ESP_OK ||
      httpd_query_key_value(query, "val", valstr, sizeof(valstr)) != ESP_OK) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad query");
    return ESP_FAIL;
  }
  int val = atoi(valstr);
  sensor_t *s = esp_camera_sensor_get();
  int r = 0;
  // framesize/quality go through camera_frame so the module ceiling is enforced
  // and quality reaches whichever encoder is actually in use. The rest are plain
  // sensor controls — note the GC2145 has none of them and returns -1, which the
  // UI reflects by disabling those sliders (see /status "adjustable").
  if      (!strcmp(var, "framesize"))  camera_set_framesize((framesize_t)val);
  else if (!strcmp(var, "quality"))    camera_set_quality(val);
  else if (!strcmp(var, "brightness")) r = s->set_brightness(s, val);
  else if (!strcmp(var, "contrast"))   r = s->set_contrast(s, val);
  else if (!strcmp(var, "saturation")) r = s->set_saturation(s, val);
  else if (!strcmp(var, "hmirror"))    r = s->set_hmirror(s, val);
  else if (!strcmp(var, "vflip"))      r = s->set_vflip(s, val);
  else if (!strcmp(var, "led_intensity")) {
    led_intensity = val < 0 ? 0 : (val > 255 ? 255 : val);
    ledcWrite(LED_LEDC_CHANNEL, led_intensity);
  }
  else { httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "unknown var"); return ESP_FAIL; }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, NULL, 0) == ESP_OK && r == 0 ? ESP_OK : ESP_OK;
}

// /status — current sensor state as JSON so the UI can sync its controls.
// Also reports the fitted module's capabilities: "max_framesize" so the UI can
// grey out resolutions this sensor can't reach, and "adjustable" so it can grey
// out the image sliders the GC2145 simply doesn't implement.
static esp_err_t status_handler(httpd_req_t *req) {
  sensor_t *s = esp_camera_sensor_get();
  bool adjustable = camera_native_jpeg();  // OV-series: yes; GC2145: no
  char json[384];
  snprintf(json, sizeof(json),
           "{\"framesize\":%u,\"quality\":%u,\"brightness\":%d,\"contrast\":%d,"
           "\"saturation\":%d,\"led_intensity\":%d,\"hmirror\":%u,\"vflip\":%u,"
           "\"module\":\"%s\",\"sensor_pid\":%u,\"native_jpeg\":%s,"
           "\"max_framesize\":%u,\"adjustable\":%s}",
           s->status.framesize, camera_get_quality(), s->status.brightness,
           s->status.contrast, s->status.saturation, led_intensity,
           s->status.hmirror, s->status.vflip,
           CAM_MODULE_NAME, s->id.PID, camera_native_jpeg() ? "true" : "false",
           (unsigned)camera_max_framesize(), adjustable ? "true" : "false");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, json, strlen(json));
}

// Returns true if the control UI (port 80) started. Logs the outcome of both
// the control server and the stream server so the serial monitor shows whether
// the web UI came up.
bool startCameraServer() {
  setupLedFlash();  // init the flash LED (starts off)

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_uri_handlers = 8;

  httpd_uri_t index_uri   = { .uri = "/",        .method = HTTP_GET, .handler = index_handler,   .user_ctx = NULL };
  httpd_uri_t capture_uri = { .uri = "/capture", .method = HTTP_GET, .handler = capture_handler, .user_ctx = NULL };
  httpd_uri_t control_uri = { .uri = "/control", .method = HTTP_GET, .handler = control_handler, .user_ctx = NULL };
  httpd_uri_t status_uri  = { .uri = "/status",  .method = HTTP_GET, .handler = status_handler,  .user_ctx = NULL };
  httpd_uri_t stream_uri  = { .uri = "/stream",  .method = HTTP_GET, .handler = stream_handler,  .user_ctx = NULL };

  bool controlOk = (httpd_start(&camera_httpd, &config) == ESP_OK);
  if (controlOk) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &capture_uri);
    httpd_register_uri_handler(camera_httpd, &control_uri);
    httpd_register_uri_handler(camera_httpd, &status_uri);
    Serial.printf("[http] control UI started on port %d\n", config.server_port);
  } else {
    Serial.printf("[http] ERROR: control UI failed to start on port %d\n", config.server_port);
  }

  // Stream lives on its own port so a long-running MJPEG connection never
  // blocks the control UI.
  config.server_port += 1;   // 81
  config.ctrl_port   += 1;
  bool streamOk = (httpd_start(&stream_httpd, &config) == ESP_OK);
  if (streamOk) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
    Serial.printf("[http] MJPEG stream started on port %d\n", config.server_port);
  } else {
    Serial.printf("[http] ERROR: stream server failed to start on port %d\n", config.server_port);
  }

  return controlOk;
}
