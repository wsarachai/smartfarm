// jetson-ctrl — enclosure external-fan controller (see DESIGN.md).
#include <csignal>
#include <cstring>
#include <ctime>
#include <fstream>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>

#include "config.hpp"
#include "control.hpp"
#include "dht22.hpp"
#include "fan.hpp"
#include "log.hpp"
#include "net.hpp"
#include "notify.hpp"
#include "thermal.hpp"

using nlohmann::json;
using namespace std::chrono;

namespace {

volatile std::sig_atomic_t g_stop = 0;
void on_signal(int) { g_stop = 1; }

std::string arg_config(int argc, char** argv) {
  for (int i = 1; i < argc - 1; ++i)
    if (std::strcmp(argv[i], "--config") == 0) return argv[i + 1];
  return "/etc/jetson-ctrl/config.json";
}

// Parse an ISO-8601 UTC timestamp ("2026-07-20T15:30:00Z"). Returns (time_t)-1 on
// failure. Assumes UTC/'Z'; fractional seconds and numeric offsets are ignored.
std::time_t parse_iso_utc(const std::string& s) {
  std::tm tm{};
  if (!strptime(s.c_str(), "%Y-%m-%dT%H:%M:%S", &tm)) return (std::time_t)-1;
  return timegm(&tm);
}

// Resolve the effective override, enforcing Q8: force_on only while unexpired AND
// within the configured cap; everything else (absent, expired, over-cap, bad time,
// "auto", unknown verb) degrades to auto. Add-only safety is enforced in control.
control::Override resolve_override(const net::OverridePayload& p, int max_minutes) {
  if (!p.present || p.mode != "force_on" || p.until_iso.empty())
    return control::Override::kAuto;
  std::time_t until = parse_iso_utc(p.until_iso);
  if (until == (std::time_t)-1) return control::Override::kAuto;
  std::time_t now = std::time(nullptr);
  if (now >= until) return control::Override::kAuto;                 // expired
  if (until - now > (std::time_t)max_minutes * 60)                   // over cap
    return control::Override::kAuto;
  return control::Override::kForceOn;
}

// Built-in fan duty (governor-owned; telemetry only). -1 if unreadable.
int read_builtin_pwm() {
  std::ifstream f("/sys/devices/pwm-fan/target_pwm");
  int v;
  return (f >> v) ? v : -1;
}

}  // namespace

int main(int argc, char** argv) {
  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);

  const std::string cfg_path = arg_config(argc, argv);
  ConfigStore store(cfg_path);
  try {
    store.load();  // fatal if the initial config is bad
  } catch (const std::exception& e) {
    LOG_ERROR("fatal: %s", e.what());
    return 1;
  }
  const Config* cfg = &store.get();

  net::global_init();

  Thermal thermal(cfg->thermal.sysfs_root, cfg->thermal.zone_names);

  Dht22 dht(cfg->dht22.gpiochip, cfg->dht22.line_offset, cfg->dht22.max_retries,
            milliseconds(cfg->cadence.dht22_min_interval_ms),
            milliseconds(cfg->dht22.stale_after_ms));
  dht.start();

  Fan fan(cfg->fan.gpiochip, cfg->fan.line_offset, cfg->fan.active_high);
  try {
    fan.init_on();  // Q10: boot-safe default ON; fatal if the line won't acquire
  } catch (const std::exception& e) {
    LOG_ERROR("fatal: %s", e.what());
    dht.stop();
    net::global_cleanup();
    return 1;
  }

  notify::ready();
  LOG_INFO("jetson-ctrl started (config %s)", cfg_path.c_str());

  net::OverridePayload override_payload;  // refreshed on network ticks
  long tick = 0;

  while (!g_stop) {
    notify::watchdog();  // feed WatchdogSec every tick

    // Hot-reload config; re-resolve thermal zones if the list may have changed.
    if (store.maybe_reload()) {
      cfg = &store.get();
      thermal = Thermal(cfg->thermal.sysfs_root, cfg->thermal.zone_names);
      dht.set_min_interval(milliseconds(cfg->cadence.dht22_min_interval_ms));
      dht.set_stale_after(milliseconds(cfg->dht22.stale_after_ms));
    }

    // --- Read inputs ---
    Thermal::Reading zone = thermal.read();
    Dht22::Sample air = dht.snapshot();

    control::Inputs in;
    in.zone_valid = zone.valid;
    in.zone_max_c = zone.max_c;
    in.air_fresh = air.valid && !air.stale;
    in.air_c = air.temp_c;
    in.fan_currently_on = fan.is_on();
    in.ovr = resolve_override(override_payload, cfg->override_.max_duration_minutes);

    // --- Decide + actuate ---
    control::Decision dec = control::decide(in, *cfg);
    bool write_ok = true;
    fan.set(dec.fan_on, cfg->control.min_on_seconds, cfg->control.min_off_seconds,
            write_ok);
    bool degraded = dec.degraded || !write_ok;

    // --- Network (best-effort, every Nth tick) ---
    if (tick % cfg->cadence.network_every_ticks == 0) {
      json metrics;
      metrics["zone_max_c"] = zone.valid ? json(zone.max_c) : json(nullptr);
      for (const auto& kv : zone.per_zone) metrics[kv.first] = kv.second;
      if (air.valid) {
        metrics["enclosure_temp_c"] = air.temp_c;
        metrics["enclosure_humidity"] = air.humidity;
      }
      metrics["enclosure_stale"] = air.stale;
      metrics["builtin_fan_pwm"] = read_builtin_pwm();
      metrics["external_fan"] = fan.is_on() ? "on" : "off";
      metrics["override"] =
          in.ovr == control::Override::kForceOn ? "force_on" : "auto";
      metrics["degraded"] = degraded;
      metrics["reason"] = dec.reason;

      json body;
      body["device_id"] = cfg->web.device_id;
      body["metrics"] = metrics;
      net::post_telemetry(cfg->web.base_url, cfg->web.http_timeout_ms, body.dump());

      override_payload = net::fetch_override(
          cfg->web.base_url, cfg->web.http_timeout_ms, cfg->web.device_id);
    }

    ++tick;
    std::this_thread::sleep_for(milliseconds(cfg->cadence.control_tick_ms));
  }

  LOG_INFO("jetson-ctrl stopping (relay holds last state)");
  dht.stop();
  net::global_cleanup();
  return 0;
}
