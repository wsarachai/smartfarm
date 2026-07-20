#pragma once
#include <string>
#include "config.hpp"

// Pure control law: bang-bang with hysteresis + override gate + fail-to-cooling.
// No I/O here so it can be unit-tested off-target.
namespace control {

enum class Override { kAuto, kForceOn };  // Q8a: no force-off verb exists.

struct Inputs {
  // Thermal zones (safety anchor).
  bool zone_valid = false;   // false => fail-to-cooling
  double zone_max_c = 0.0;

  // DHT22 enclosure air (secondary). Only used when a fresh reading exists.
  bool air_fresh = false;    // valid && !stale
  double air_c = 0.0;

  bool fan_currently_on = false;  // for hysteresis direction
  Override ovr = Override::kAuto;
};

struct Decision {
  bool fan_on = false;
  bool degraded = false;      // thermal unreadable -> forced cooling
  std::string reason;         // human-readable, for telemetry/logs
};

Decision decide(const Inputs& in, const Config& cfg);

}  // namespace control
