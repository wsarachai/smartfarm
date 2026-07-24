#pragma once
// Sensor-agnostic JPEG frame source.
//
// The rest of the firmware (snapshot push, /capture, :81/stream, SD saving) only
// ever wants "a JPEG and its length". Where that JPEG comes from depends on the
// fitted module — the sensor's own encoder on an OV3660/OV2640, or a software
// compress of an RGB565 buffer on an RHYX M21-45 (GC2145). This hides that.

#include "esp_camera.h"

// A JPEG ready to send. Two shapes, distinguished by `owned`:
//   owned == false -> buf points into the camera frame buffer `fb` (hardware
//                     encoder); the fb must go back to the driver.
//   owned == true  -> buf was malloc'd by the software encoder and must be
//                     free'd; `fb` was already returned during the grab.
// Either way, pair a successful grab with exactly one release.
struct jpeg_frame_t {
  uint8_t     *buf;
  size_t       len;
  camera_fb_t *fb;
  bool         owned;
};

// Record what esp_camera_init() actually ended up with. Called once from
// initCamera() — including after a fallback, so the runtime state reflects the
// sensor that is physically fitted rather than the profile that was compiled in.
//
//   cfg           the camera_config_t that succeeded — kept so the software
//                 path can replay it (deinit+reinit) to change framesize.
//   nativeJpeg    true if the sensor encodes JPEG in hardware.
//   maxFramesize  ceiling for camera_set_framesize().
//   applyDefaults called on the sensor after every (re)init to re-apply the
//                 module's orientation/tuning (reinit resets sensor registers).
void camera_frame_begin(const camera_config_t *cfg, bool nativeJpeg,
                        framesize_t maxFramesize, void (*applyDefaults)(sensor_t *));

// True if the fitted sensor encodes JPEG in hardware.
bool camera_native_jpeg();
// Largest framesize this module may be set to (see CAM_MAX_FRAME_SIZE).
framesize_t camera_max_framesize();

// Grab a JPEG, retrying up to max_tries. On a hardware-encoder sensor this also
// rejects truncated frames; on the software path a retry only helps against a
// transient allocation failure. Returns false if every attempt failed.
bool jpeg_frame_grab(jpeg_frame_t *out, int max_tries);
void jpeg_frame_release(jpeg_frame_t *f);

// Framesize and quality go through here rather than straight to sensor_t so the
// module ceiling and the two quality scales are applied in one place.
// Quality is always the hardware scale: 4..63, LOWER is better.
framesize_t camera_set_framesize(framesize_t f);
framesize_t camera_get_framesize();
void        camera_set_quality(int q);
int         camera_get_quality();
