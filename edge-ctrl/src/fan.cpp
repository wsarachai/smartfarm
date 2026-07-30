#include "fan.hpp"

#include <gpiod.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

#include "log.hpp"

using std::chrono::steady_clock;
using std::chrono::seconds;

namespace { constexpr char kConsumer[] = "edge-ctrl-fan"; }

Fan::Fan(std::string gpiochip, unsigned line_offset, bool active_high)
    : chip_name_(std::move(gpiochip)),
      line_off_(line_offset),
      active_high_(active_high) {}

Fan::~Fan() {
#if HAVE_GPIOD_V2
  if (req_v2_) gpiod_line_request_release(req_v2_);
  if (chip_) gpiod_chip_close(chip_);
#else
  if (line_) gpiod_line_release(line_);
  if (chip_) gpiod_chip_close(chip_);
#endif
}

void Fan::init_on() {
  const std::string where = chip_name_ + ":" + std::to_string(line_off_);

#if HAVE_GPIOD_V2
  std::string dev_path = chip_name_;
  if (dev_path.rfind("/dev/", 0) != 0) {
    dev_path = "/dev/" + dev_path;
  }
  chip_ = gpiod_chip_open(dev_path.c_str());
  if (!chip_) {
    for (const char* fb : {"/dev/gpiochip0", "/dev/gpiochip4", "/dev/gpiochip1"}) {
      chip_ = gpiod_chip_open(fb);
      if (chip_) break;
    }
  }
  if (!chip_) {
    throw std::runtime_error("fan: cannot open " + chip_name_ + ": " + std::strerror(errno));
  }

  int initial_val = active_high_ ? 1 : 0;
  gpiod_line_settings* settings = gpiod_line_settings_new();
  gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
  gpiod_line_settings_set_output_value(settings, initial_val ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE);

  gpiod_line_config* line_cfg = gpiod_line_config_new();
  unsigned int offsets[1] = { line_off_ };
  gpiod_line_config_add_line_settings(line_cfg, offsets, 1, settings);

  gpiod_request_config* req_cfg = gpiod_request_config_new();
  gpiod_request_config_set_consumer(req_cfg, kConsumer);

  req_v2_ = gpiod_chip_request_lines(chip_, req_cfg, line_cfg);

  gpiod_line_settings_free(settings);
  gpiod_line_config_free(line_cfg);
  gpiod_request_config_free(req_cfg);

  if (!req_v2_) {
    throw std::runtime_error("fan: cannot request output line " + where + ": " + std::strerror(errno));
  }

#else
  // libgpiod v1
  chip_ = gpiod_chip_open_by_name(chip_name_.c_str());
  if (!chip_ && chip_name_.rfind("/dev/", 0) == 0) {
    chip_ = gpiod_chip_open(chip_name_.c_str());
  }
  if (!chip_ && chip_name_.rfind("/dev/", 0) != 0) {
    std::string dev_path = "/dev/" + chip_name_;
    chip_ = gpiod_chip_open(dev_path.c_str());
  }
  if (!chip_) {
    for (const char* fb : {"gpiochip0", "/dev/gpiochip0", "gpiochip4", "/dev/gpiochip4"}) {
      chip_ = gpiod_chip_open_by_name(fb);
      if (!chip_) chip_ = gpiod_chip_open(fb);
      if (chip_) break;
    }
  }

  if (!chip_)
    throw std::runtime_error("fan: cannot open " + chip_name_ + ": " +
                             std::strerror(errno));
  line_ = gpiod_chip_get_line(chip_, line_off_);
  if (!line_)
    throw std::runtime_error("fan: cannot get line " + where + ": " +
                             std::strerror(errno));

  int initial = active_high_ ? 1 : 0;
  if (gpiod_line_request_output(line_, kConsumer, initial) != 0) {
    const int err = errno;
    std::string msg = "fan: cannot request output line " + where + ": " +
                      std::strerror(err);
    if (err == EBUSY)
      msg += " (another driver holds it -- check: gpioinfo " + chip_name_ + ")";
    throw std::runtime_error(msg);
  }
#endif

  on_ = true;
  last_change_ = steady_clock::now();
  LOG_INFO("fan: initialized ON (boot-safe default)");
}

bool Fan::write(bool on) {
  int level = (on == active_high_) ? 1 : 0;  // active_high_ maps logical->electrical
#if HAVE_GPIOD_V2
  if (!req_v2_) return false;
  gpiod_line_value val = level ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
  return gpiod_line_request_set_value(req_v2_, line_off_, val) == 0;
#else
  if (!line_) return false;
  return gpiod_line_set_value(line_, level) == 0;
#endif
}

bool Fan::set(bool desired_on, int min_on_seconds, int min_off_seconds,
              bool& write_ok) {
  write_ok = true;
  if (desired_on == on_) return on_;  // no change

  // Dwell: don't flip until we've held the current state long enough.
  auto held = std::chrono::duration_cast<seconds>(
                  steady_clock::now() - last_change_)
                  .count();
  int need = on_ ? min_on_seconds : min_off_seconds;
  if (held < need) return on_;  // still dwelling; keep current state

  if (!write(desired_on)) {
    write_ok = false;
    LOG_ERROR("fan: GPIO write failed (wanted %s)", desired_on ? "ON" : "OFF");
    return on_;  // hardware unchanged; caller will keep trying (fail-to-cooling)
  }
  on_ = desired_on;
  last_change_ = steady_clock::now();
  LOG_INFO("fan: -> %s", on_ ? "ON" : "OFF");
  return on_;
}
