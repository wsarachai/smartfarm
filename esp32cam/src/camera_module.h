#pragma once
// Camera *module* profiles — selected with -DCAMERA_MODULE_* in platformio.ini.
//
// This is deliberately separate from camera_pins.h: both supported modules plug
// into the same AI-Thinker socket, so the pin map is identical. What differs is
// the sensor behind the ribbon, and the sensors are not interchangeable:
//
//   OV3660 / OV2640 (the original module)
//     Hardware JPEG encoder. Capture straight to PIXFORMAT_JPEG, so the ESP32
//     just moves bytes — UXGA at a useful rate is fine.
//
//   RHYX M21-45 (GC2145)
//     *No* hardware JPEG encoder — the sensor only emits RGB565/YUV422. Asking
//     esp_camera_init() for PIXFORMAT_JPEG makes it fail with
//     ESP_ERR_NOT_SUPPORTED, so this module must capture RGB565 and compress in
//     software (frame2jpg) before anything downstream sees a JPEG. It also
//     ignores set_brightness/contrast/saturation/quality (its driver returns -1).
//
// Everything downstream (HTTP handlers, the snapshot push, the hub's config
// pull) speaks JPEG only and does not care which of the two is fitted — see
// camera_frame.h for the abstraction that hides the difference.

#include "esp_camera.h"

// ─── SELECT YOUR CAMERA MODULE HERE ───────────────────────────────────────────
// Comment/uncomment ONE line, then just:  pio run -e esp32cam -t upload
// (No need for a separate build env — this is the switch.)
//
// A -DCAMERA_MODULE_* build flag, if passed on the command line, overrides this
// block — so `pio run -e esp32cam_rhyx` still works and wins over the line below.
#if !defined(CAMERA_MODULE_OV3660) && !defined(CAMERA_MODULE_RHYX_M21_45)

  #define CAMERA_MODULE_OV3660          // OV3660 / OV2640 — hardware JPEG
  // #define CAMERA_MODULE_RHYX_M21_45  // RHYX M21-45 (GC2145) — software JPEG

#endif
// ──────────────────────────────────────────────────────────────────────────────

#if defined(CAMERA_MODULE_RHYX_M21_45)

  #define CAM_MODULE_NAME     "RHYX M21-45 (GC2145)"
  // No hardware encoder: capture raw and compress on the CPU.
  #define CAM_NATIVE_JPEG     0
  #define CAM_PIXEL_FORMAT    PIXFORMAT_RGB565
  // GC2145 is driven at the usual 20 MHz. The 10 MHz used for the OV3660 exists
  // to protect its *JPEG* DMA path, which this sensor doesn't have.
  #define CAM_XCLK_HZ         20000000
  #define CAM_FRAME_SIZE      FRAMESIZE_SVGA
  // Hard ceiling, enforced at runtime on every framesize change. Two limits bite
  // before the sensor's own UXGA maximum does:
  //   1. RGB565 is 2 bytes/px, so UXGA needs a 3.84 MB frame buffer out of 4 MB
  //      of PSRAM — nothing left for WiFi or the encoder.
  //   2. fmt2jpg() compresses into a *fixed* 128 KB output buffer and simply
  //      fails if the encoded frame overflows it.
  // SVGA (800x600 = 960 KB raw, ~55 KB encoded) sits comfortably inside both.
  #define CAM_MAX_FRAME_SIZE  FRAMESIZE_SVGA
  // TWO buffers, not one. With a single buffer the non-JPEG (RGB565) continuous
  // capture has nowhere to DMA the next frame while the app holds the current
  // one, and esp_camera_fb_get() blocks forever waiting for a frame that never
  // completes — which wedges loop() and trips the task watchdog at boot. Two
  // SVGA RGB565 buffers are 1.92 MB, comfortably inside 4 MB PSRAM. GRAB_LATEST
  // keeps the freshest frame instead of queuing stale ones.
  #define CAM_FB_COUNT        2
  #define CAM_GRAB_MODE       CAMERA_GRAB_LATEST

#else  // default: CAMERA_MODULE_OV3660 — the original module, unchanged v1 tuning

  #define CAM_MODULE_NAME     "OV3660 / OV2640"
  #define CAM_NATIVE_JPEG     1
  #define CAM_PIXEL_FORMAT    PIXFORMAT_JPEG
  // 10 MHz (vs the usual 20) gives the DMA capture path maximum margin against
  // dropped bytes, which show up as random colored horizontal lines. This is an
  // AI feed — FPS is irrelevant, clean frames are everything.
  #define CAM_XCLK_HZ         10000000
  #define CAM_FRAME_SIZE      FRAMESIZE_UXGA  // 1600x1200 — max detail for AI
  #define CAM_MAX_FRAME_SIZE  FRAMESIZE_UXGA
  #define CAM_FB_COUNT        2               // double buffer (needs PSRAM)
  #define CAM_GRAB_MODE       CAMERA_GRAB_WHEN_EMPTY  // complete, in-order frames

#endif

// One quality dial, two encoders. The hub, the web UI and this firmware all
// speak the *hardware* scale — 4..63, where LOWER is better — because that is
// what the existing /control?var=quality and the hub's `jpeg_quality` config key
// already send. camera_set_quality() translates onto the software encoder's
// inverted 0..100 scale when the fitted module has no hardware encoder.
#define CAM_QUALITY_HW_MIN  4    // best
#define CAM_QUALITY_HW_MAX  63   // worst
#define CAM_QUALITY_DEFAULT 12   // safe floor at UXGA on the OV3660

// Band the software encoder is mapped into. Capped below 100 on purpose: the
// top of the range blows past fmt2jpg()'s fixed 128 KB output buffer.
#define CAM_QUALITY_SW_BEST  85
#define CAM_QUALITY_SW_WORST 25
