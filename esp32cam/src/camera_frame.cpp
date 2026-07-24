// Sensor-agnostic JPEG frame source — see camera_frame.h.

#include "camera_frame.h"
#include "camera_module.h"
#include "img_converters.h"
#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Set by camera_frame_begin() from what esp_camera_init() actually accepted.
static bool        s_nativeJpeg   = CAM_NATIVE_JPEG;
static framesize_t s_maxFramesize = CAM_MAX_FRAME_SIZE;

// The config that succeeded, kept so the software path can replay it, plus the
// module's post-init sensor tweaks (reinit wipes sensor registers).
static camera_config_t s_cfg;
static bool            s_cfgValid = false;
static void          (*s_applyDefaults)(sensor_t *) = NULL;

// Serializes frame grabs against a deinit/reinit. The control server (:80) can
// change framesize on a different task from the one streaming on :81; without
// this, a reinit could free the frame-buffer pool mid-grab. A framesize change
// reinits the camera (on either module), and every grab copies the JPEG out and
// returns the camera buffer before releasing this lock — so no buffer is ever
// checked out across a reinit.
static SemaphoreHandle_t s_lock = NULL;
static inline void lock()   { if (s_lock) xSemaphoreTake(s_lock, portMAX_DELAY); }
static inline void unlock() { if (s_lock) xSemaphoreGive(s_lock); }

// Quality is stored on the hardware scale (4..63, lower = better) because that
// is the scale every caller speaks; s_swQuality is the translated value handed
// to frame2jpg() when there is no hardware encoder.
static int s_quality   = CAM_QUALITY_DEFAULT;
static int s_swQuality = CAM_QUALITY_SW_BEST;

static int quality_hw_to_sw(int q) {
  // Invert 4..63 (lower = better) onto SW_WORST..SW_BEST (higher = better).
  const int span = CAM_QUALITY_HW_MAX - CAM_QUALITY_HW_MIN;
  const int band = CAM_QUALITY_SW_BEST - CAM_QUALITY_SW_WORST;
  return CAM_QUALITY_SW_BEST - ((q - CAM_QUALITY_HW_MIN) * band) / span;
}

void camera_frame_begin(const camera_config_t *cfg, bool nativeJpeg,
                        framesize_t maxFramesize, void (*applyDefaults)(sensor_t *)) {
  s_nativeJpeg    = nativeJpeg;
  s_maxFramesize  = maxFramesize;
  s_applyDefaults = applyDefaults;
  if (cfg) { s_cfg = *cfg; s_cfgValid = true; }
  if (!s_lock) s_lock = xSemaphoreCreateMutex();
  camera_set_quality(s_quality);  // push the default down the right path
}

bool camera_native_jpeg() { return s_nativeJpeg; }
framesize_t camera_max_framesize() { return s_maxFramesize; }

// A structurally valid JPEG starts with SOI (FF D8) and ends with EOI (FF D9).
// Truncated/byte-dropped frames fail this — cheap way to reject obvious garbage.
static bool looks_like_jpeg(const uint8_t *buf, size_t len) {
  return buf && len > 100 && buf[0] == 0xFF && buf[1] == 0xD8 &&
         buf[len - 2] == 0xFF && buf[len - 1] == 0xD9;
}

bool jpeg_frame_grab(jpeg_frame_t *out, int max_tries) {
  out->buf = NULL; out->len = 0; out->fb = NULL; out->owned = false;
  if (max_tries < 1) max_tries = 1;

  // Held for the whole grab so a framesize reinit can't free the fb pool under
  // us. Both paths return the camera buffer before releasing the lock (the
  // native path copies the JPEG into an owned buffer first), so nothing is ever
  // checked out across a reinit.
  lock();
  for (int i = 0; i < max_tries; i++) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) continue;

    if (fb->format == PIXFORMAT_JPEG) {
      // Hardware encoder: the buffer already is a JPEG. Favor returning a clean
      // still over returning quickly (AI use-case), but on the final attempt
      // take whatever we have rather than dropping the cycle entirely.
      if (looks_like_jpeg(fb->buf, fb->len) || i == max_tries - 1) {
        // Copy out (PSRAM-first — a UXGA JPEG can overflow internal RAM) so the
        // camera buffer goes straight back and isn't held past the lock.
        uint8_t *copy = (uint8_t *)ps_malloc(fb->len);
        if (!copy) copy = (uint8_t *)malloc(fb->len);
        if (copy) {
          memcpy(copy, fb->buf, fb->len);
          out->buf = copy; out->len = fb->len; out->fb = NULL; out->owned = true;
          esp_camera_fb_return(fb);
        } else {
          // Out of memory to copy: fall back to zero-copy (hold the fb). Rare;
          // a concurrent reinit would just have to wait for the release.
          out->buf = fb->buf; out->len = fb->len; out->fb = fb; out->owned = false;
        }
        unlock();
        return true;
      }
      esp_camera_fb_return(fb);  // drop bad frame, try again
      continue;
    }

    // No hardware encoder (RHYX M21-45 / GC2145): compress the RGB565 buffer on
    // the CPU. fmt2jpg() allocates its 128 KB working buffer in PSRAM, so the
    // raw frame can go back to the driver as soon as it returns.
    uint8_t *jpg = NULL; size_t jpg_len = 0;
    bool ok = frame2jpg(fb, s_swQuality, &jpg, &jpg_len);
    esp_camera_fb_return(fb);
    if (!ok) continue;  // allocation failure or encoder overflow — retry
    out->buf = jpg; out->len = jpg_len; out->fb = NULL; out->owned = true;
    unlock();
    return true;
  }
  unlock();
  return false;
}

void jpeg_frame_release(jpeg_frame_t *f) {
  if (!f) return;
  if (f->owned && f->buf) free(f->buf);
  if (f->fb) esp_camera_fb_return(f->fb);
  f->buf = NULL; f->len = 0; f->fb = NULL; f->owned = false;
}

framesize_t camera_set_framesize(framesize_t f) {
  if (f > s_maxFramesize) f = s_maxFramesize;  // RGB565 modules run out of PSRAM
  if (!s_cfgValid) return camera_get_framesize();
  if (f == s_cfg.frame_size) return f;  // nothing to do

  // Full deinit+reinit at the new size, on BOTH modules. The GC2145's
  // set_framesize only rewrites sensor registers without resizing the RGB565
  // DMA buffer; the OV sensors re-window live but that proved unreliable here.
  // A reinit is the one path that always actually changes the resolution.
  // Serialized against grabs by the lock so no frame is in flight across the
  // teardown (every grab returns its camera buffer before releasing the lock).
  lock();
  framesize_t prev = s_cfg.frame_size;
  esp_camera_deinit();
  s_cfg.frame_size = f;
  esp_err_t err = esp_camera_init(&s_cfg);
  if (err != ESP_OK) {
    // Roll back to the size that was working rather than leave the camera down.
    Serial.printf("[cam] reinit at framesize %d failed (0x%x) — reverting\n", f, err);
    s_cfg.frame_size = prev;
    err = esp_camera_init(&s_cfg);
    f = prev;
  }
  if (err == ESP_OK) {
    // Reinit reset the sensor: re-apply the module's tuning and the current
    // quality (the OV quality register reverts to the config default otherwise).
    sensor_t *s = esp_camera_sensor_get();
    if (s && s_applyDefaults) s_applyDefaults(s);
    camera_set_quality(s_quality);
  }
  unlock();
  Serial.printf("[cam] framesize -> %d (reinit %s)\n", f, err == ESP_OK ? "ok" : "FAILED");
  return f;
}

framesize_t camera_get_framesize() {
  sensor_t *s = esp_camera_sensor_get();
  return s ? (framesize_t)s->status.framesize : s_maxFramesize;
}

void camera_set_quality(int q) {
  if (q < CAM_QUALITY_HW_MIN) q = CAM_QUALITY_HW_MIN;
  if (q > CAM_QUALITY_HW_MAX) q = CAM_QUALITY_HW_MAX;
  s_quality = q;

  if (s_nativeJpeg) {
    sensor_t *s = esp_camera_sensor_get();
    if (s) s->set_quality(s, q);
  } else {
    // The GC2145 driver returns -1 for set_quality, so the dial has to act on
    // the software encoder instead.
    s_swQuality = quality_hw_to_sw(q);
  }
}

int camera_get_quality() { return s_quality; }
