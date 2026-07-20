#include "fan.hpp"

#include <gpiod.h>
#include <stdexcept>

#include "log.hpp"

using std::chrono::steady_clock;
using std::chrono::seconds;

namespace { constexpr char kConsumer[] = "jetson-ctrl-fan"; }

Fan::Fan(std::string gpiochip, unsigned line_offset, bool active_high)
    : chip_name_(std::move(gpiochip)),
      line_off_(line_offset),
      active_high_(active_high) {}

Fan::~Fan() {
  if (line_) gpiod_line_release(line_);
  if (chip_) gpiod_chip_close(chip_);
}

void Fan::init_on() {
  chip_ = gpiod_chip_open_by_name(chip_name_.c_str());
  if (!chip_) throw std::runtime_error("fan: cannot open " + chip_name_);
  line_ = gpiod_chip_get_line(chip_, line_off_);
  if (!line_) throw std::runtime_error("fan: cannot get line");

  // Boot-safe default: ON (Q10). Request the line already driven to the active
  // level so there's no OFF glitch between request and first write.
  int initial = active_high_ ? 1 : 0;
  if (gpiod_line_request_output(line_, kConsumer, initial) != 0)
    throw std::runtime_error("fan: cannot request output line");
  on_ = true;
  last_change_ = steady_clock::now();
  LOG_INFO("fan: initialized ON (boot-safe default)");
}

bool Fan::write(bool on) {
  int level = (on == active_high_) ? 1 : 0;  // active_high_ maps logical->electrical
  return gpiod_line_set_value(line_, level) == 0;
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
