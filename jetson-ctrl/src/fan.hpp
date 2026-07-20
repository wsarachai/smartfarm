#pragma once
#include <string>
#include <chrono>

struct gpiod_chip;
struct gpiod_line;

// The one and only actuated GPIO: the external enclosure fan (on/off).
//
// Enforces minimum on/off dwell so the relay can't chatter. The control loop asks
// for a desired state each tick; the fan applies it only if dwell allows AND the
// hardware write succeeds. Fail-to-cooling: on any write error the fan reports
// itself degraded and the loop is expected to keep asking for ON.
class Fan {
 public:
  Fan(std::string gpiochip, unsigned line_offset, bool active_high);
  ~Fan();
  Fan(const Fan&) = delete;             // owns raw gpiod handles
  Fan& operator=(const Fan&) = delete;

  // Acquire the line and drive it to the boot-safe default: ON (Q10 fail-to-cooling).
  // Throws std::runtime_error if the line can't be acquired at startup (fatal).
  void init_on();

  // Request a desired state. Honors min_on/min_off dwell. Returns the ACTUAL state
  // after the call. `write_ok` is set false if a hardware write was attempted and
  // failed (caller surfaces `degraded`).
  bool set(bool desired_on, int min_on_seconds, int min_off_seconds,
           bool& write_ok);

  bool is_on() const { return on_; }

 private:
  bool write(bool on);  // raw line write honoring active_high; returns success

  std::string chip_name_;
  unsigned line_off_;
  bool active_high_;
  gpiod_chip* chip_ = nullptr;
  gpiod_line* line_ = nullptr;

  bool on_ = false;
  std::chrono::steady_clock::time_point last_change_;
};
