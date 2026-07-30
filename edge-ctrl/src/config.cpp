#include "config.hpp"

#include <sys/stat.h>
#include <fstream>
#include <stdexcept>
#include <nlohmann/json.hpp>

#include "log.hpp"

using nlohmann::json;

namespace {

// Fetch an optional key, keeping the struct default if absent.
template <typename T>
void opt(const json& j, const char* key, T& dst) {
  if (j.contains(key)) dst = j.at(key).get<T>();
}

// Validate cross-field invariants. Throws std::runtime_error on violation so a
// bad edit is rejected wholesale (never partially applied).
void validate(const Config& c) {
  auto require = [](bool ok, const char* msg) {
    if (!ok) throw std::runtime_error(msg);
  };
  require(!c.web.base_url.empty(), "web_server.base_url must be set");
  require(!c.web.device_id.empty(), "web_server.device_id must be set");
  require(c.cadence.control_tick_ms >= 200, "cadence.control_tick_ms too small");
  require(c.cadence.network_every_ticks >= 1, "cadence.network_every_ticks < 1");
  require(!c.thermal.zone_names.empty(), "thermal.zone_names must be non-empty");
  // Hysteresis must be a real deadband: off point strictly below on point.
  require(c.control.temp_off_c < c.control.temp_on_c,
          "control.temp_off_c must be < temp_on_c");
  require(c.control.enclosure_air_off_c < c.control.enclosure_air_on_c,
          "control.enclosure_air_off_c must be < enclosure_air_on_c");
  require(c.control.min_on_seconds >= 0 && c.control.min_off_seconds >= 0,
          "control.min_on/off_seconds must be >= 0");
  require(c.override_.max_duration_minutes >= 1 &&
              c.override_.max_duration_minutes <= 240,
          "override.max_duration_minutes out of range (1..240)");
}

}  // namespace

bool ConfigStore::parse_file(Config& out) const {
  std::ifstream f(path_);
  if (!f) {
    LOG_ERROR("config: cannot open %s", path_.c_str());
    return false;
  }
  try {
    json j = json::parse(f);

    const auto& w = j.at("web_server");
    opt(w, "base_url", out.web.base_url);
    opt(w, "device_id", out.web.device_id);
    opt(w, "http_timeout_ms", out.web.http_timeout_ms);

    if (j.contains("cadence")) {
      const auto& cad = j.at("cadence");
      opt(cad, "control_tick_ms", out.cadence.control_tick_ms);
      opt(cad, "network_every_ticks", out.cadence.network_every_ticks);
      opt(cad, "dht22_min_interval_ms", out.cadence.dht22_min_interval_ms);
    }

    const auto& th = j.at("thermal");
    opt(th, "sysfs_root", out.thermal.sysfs_root);
    opt(th, "zone_names", out.thermal.zone_names);

    const auto& d = j.at("dht22");
    opt(d, "gpiochip", out.dht22.gpiochip);
    opt(d, "line_offset", out.dht22.line_offset);
    opt(d, "max_retries", out.dht22.max_retries);
    opt(d, "stale_after_ms", out.dht22.stale_after_ms);

    const auto& fan = j.at("external_fan");
    opt(fan, "gpiochip", out.fan.gpiochip);
    opt(fan, "line_offset", out.fan.line_offset);
    opt(fan, "active_high", out.fan.active_high);

    const auto& ct = j.at("control");
    opt(ct, "temp_on_c", out.control.temp_on_c);
    opt(ct, "temp_off_c", out.control.temp_off_c);
    opt(ct, "enclosure_air_on_c", out.control.enclosure_air_on_c);
    opt(ct, "enclosure_air_off_c", out.control.enclosure_air_off_c);
    opt(ct, "min_on_seconds", out.control.min_on_seconds);
    opt(ct, "min_off_seconds", out.control.min_off_seconds);

    if (j.contains("override")) {
      opt(j.at("override"), "max_duration_minutes",
          out.override_.max_duration_minutes);
    }

    validate(out);
    return true;
  } catch (const std::exception& e) {
    LOG_ERROR("config: parse/validate failed: %s", e.what());
    return false;
  }
}

const Config& ConfigStore::load() {
  Config fresh;
  if (!parse_file(fresh)) {
    throw std::runtime_error("initial config load failed: " + path_);
  }
  cfg_ = fresh;
  struct stat st{};
  if (::stat(path_.c_str(), &st) == 0) mtime_ = st.st_mtime;
  LOG_INFO("config: loaded %s", path_.c_str());
  return cfg_;
}

bool ConfigStore::maybe_reload() {
  struct stat st{};
  if (::stat(path_.c_str(), &st) != 0) return false;  // gone? keep last-good
  if (st.st_mtime == mtime_) return false;             // unchanged

  Config fresh;
  if (!parse_file(fresh)) {
    // Bad edit: keep the old config running, but advance mtime so we don't retry
    // the same broken file every tick.
    mtime_ = st.st_mtime;
    LOG_WARN("config: reload rejected, keeping previous good config");
    return false;
  }
  cfg_ = fresh;
  mtime_ = st.st_mtime;
  LOG_INFO("config: hot-reloaded %s", path_.c_str());
  return true;
}
