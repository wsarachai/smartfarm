#include "control.hpp"

namespace control {

Decision decide(const Inputs& in, const Config& cfg) {
  Decision d;
  const auto& c = cfg.control;

  // (1) Fail-to-cooling: no trustworthy temperature => fan ON, flag degraded.
  //     An override cannot weaken this (it can only add cooling anyway).
  if (!in.zone_valid) {
    d.fan_on = true;
    d.degraded = true;
    d.reason = "degraded: thermal unreadable -> forced ON";
    return d;
  }

  // (2) Autonomous bang-bang with hysteresis. When the fan is already ON we compare
  //     against the *off* points (must fall back below to turn off); when OFF we
  //     compare against the *on* points (must exceed to turn on). This is the
  //     deadband that prevents chatter.
  bool zone_active = in.fan_currently_on ? (in.zone_max_c > c.temp_off_c)
                                         : (in.zone_max_c >= c.temp_on_c);

  bool air_active = false;
  if (in.air_fresh) {
    air_active = in.fan_currently_on ? (in.air_c > c.enclosure_air_off_c)
                                     : (in.air_c >= c.enclosure_air_on_c);
  }

  bool autonomous_on = zone_active || air_active;

  // (3) Override is ADD-ONLY (Q8): force_on can turn the fan on early, but nothing
  //     can turn it off below the autonomous decision. The safety trigger always wins.
  bool forced = (in.ovr == Override::kForceOn);
  d.fan_on = autonomous_on || forced;

  if (forced && !autonomous_on)
    d.reason = "override force_on";
  else if (zone_active && air_active)
    d.reason = "zone+air above threshold";
  else if (zone_active)
    d.reason = "zone above threshold";
  else if (air_active)
    d.reason = "enclosure air above threshold";
  else
    d.reason = "below thresholds";

  return d;
}

}  // namespace control
