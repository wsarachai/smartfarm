// Off-target unit tests for the pure control law (control::decide).
//
// No hardware, no libcurl/gpiod/json — control.cpp depends only on the Config
// struct, so this links against src/control.cpp alone and runs on any host.
// Tiny assertion harness (no gtest) to match the repo's dependency-light style.
#include <cstdio>
#include "control.hpp"

using control::Override;
using control::Inputs;

static int g_failed = 0, g_total = 0;

#define CHECK(cond)                                                       \
  do {                                                                    \
    ++g_total;                                                            \
    if (!(cond)) {                                                        \
      ++g_failed;                                                         \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    }                                                                     \
  } while (0)

// Default config: temp_on=60, temp_off=52, air_on=45, air_off=40 (see Config).
static Config cfg() { return Config{}; }

static Inputs base() {
  Inputs in;
  in.zone_valid = true;
  in.zone_max_c = 30.0;   // cool
  in.air_fresh = false;
  in.air_c = 0.0;
  in.fan_currently_on = false;
  in.ovr = Override::kAuto;
  return in;
}

// --- Fail-to-cooling (Q10) ---------------------------------------------------
static void test_fail_to_cooling() {
  Inputs in = base();
  in.zone_valid = false;          // unreadable thermal
  in.zone_max_c = 0.0;            // garbage
  auto d = control::decide(in, cfg());
  CHECK(d.fan_on == true);
  CHECK(d.degraded == true);

  // Even a cool DHT22 reading must not defeat fail-to-cooling.
  in.air_fresh = true;
  in.air_c = 10.0;
  CHECK(control::decide(in, cfg()).fan_on == true);
}

// --- Thermal-zone hysteresis (deadband 52..60) -------------------------------
static void test_zone_hysteresis() {
  Config c = cfg();

  // OFF + below on-threshold => stays OFF.
  Inputs in = base();
  in.zone_max_c = 59.9;
  CHECK(control::decide(in, c).fan_on == false);

  // OFF + reaches on-threshold => turns ON.
  in.zone_max_c = 60.0;
  CHECK(control::decide(in, c).fan_on == true);

  // ON + inside the deadband (52..60) => stays ON (no chatter).
  in.fan_currently_on = true;
  in.zone_max_c = 55.0;
  CHECK(control::decide(in, c).fan_on == true);

  // ON + just above the off-threshold => stays ON.
  in.zone_max_c = 52.1;
  CHECK(control::decide(in, c).fan_on == true);

  // ON + AT the off-threshold => turns OFF (design: OFF when <= T_off).
  in.zone_max_c = 52.0;
  CHECK(control::decide(in, c).fan_on == false);

  // OFF + inside the deadband => stays OFF (the other half of the deadband).
  in.fan_currently_on = false;
  in.zone_max_c = 55.0;
  CHECK(control::decide(in, c).fan_on == false);
}

// --- DHT22 enclosure-air OR-trigger + staleness ------------------------------
static void test_air_trigger() {
  Config c = cfg();

  // Fresh hot air with a cool zone => fan ON (OR trigger).
  Inputs in = base();
  in.zone_max_c = 30.0;
  in.air_fresh = true;
  in.air_c = 46.0;               // >= air_on (45)
  CHECK(control::decide(in, c).fan_on == true);

  // Same air reading but STALE (air_fresh=false) => ignored, fan stays OFF.
  in.air_fresh = false;
  CHECK(control::decide(in, c).fan_on == false);

  // Air hysteresis: ON + air inside 40..45 => stays ON.
  in.air_fresh = true;
  in.fan_currently_on = true;
  in.air_c = 42.0;
  CHECK(control::decide(in, c).fan_on == true);

  // ON + air drops below off (40) with cool zone => turns OFF.
  in.air_c = 39.9;
  CHECK(control::decide(in, c).fan_on == false);
}

// --- Override: add-only + expiry semantics live in main; here just the gate ---
static void test_override_add_only() {
  Config c = cfg();

  // force_on with cool zone + no air => fan ON, NOT degraded, reason names override.
  Inputs in = base();
  in.zone_max_c = 30.0;
  in.ovr = Override::kForceOn;
  auto d = control::decide(in, c);
  CHECK(d.fan_on == true);
  CHECK(d.degraded == false);
  CHECK(d.reason == "override force_on");

  // auto with cool zone => OFF (baseline).
  in.ovr = Override::kAuto;
  CHECK(control::decide(in, c).fan_on == false);

  // KEY SAFETY PROPERTY: there is no verb that turns the fan OFF. When the zone
  // demands cooling, the result is ON regardless of override value — an override
  // can only ADD cooling, never remove it.
  in.zone_max_c = 65.0;          // autonomously wants ON
  in.ovr = Override::kAuto;
  CHECK(control::decide(in, c).fan_on == true);
  in.ovr = Override::kForceOn;   // force_on can't change an already-ON decision
  CHECK(control::decide(in, c).fan_on == true);

  // force_on does NOT mask degraded: unreadable zone is still degraded AND on.
  Inputs bad = base();
  bad.zone_valid = false;
  bad.ovr = Override::kForceOn;
  auto dd = control::decide(bad, c);
  CHECK(dd.fan_on == true);
  CHECK(dd.degraded == true);
}

// --- Reason strings for combined triggers ------------------------------------
static void test_reasons() {
  Config c = cfg();
  Inputs in = base();
  in.zone_max_c = 61.0;          // zone hot
  in.air_fresh = true;
  in.air_c = 46.0;               // air hot too
  CHECK(control::decide(in, c).reason == "zone+air above threshold");

  in.air_c = 30.0;               // only zone
  CHECK(control::decide(in, c).reason == "zone above threshold");

  in.zone_max_c = 30.0;          // only air
  in.air_c = 46.0;
  CHECK(control::decide(in, c).reason == "enclosure air above threshold");

  in.air_c = 30.0;               // neither
  CHECK(control::decide(in, c).reason == "below thresholds");
}

int main() {
  test_fail_to_cooling();
  test_zone_hysteresis();
  test_air_trigger();
  test_override_add_only();
  test_reasons();

  std::printf("%d/%d checks passed\n", g_total - g_failed, g_total);
  return g_failed == 0 ? 0 : 1;
}
