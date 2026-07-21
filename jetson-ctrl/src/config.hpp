#pragma once
#include <string>
#include <vector>
#include <ctime>

// Parsed, validated config. Loaded at startup and hot-reloaded when the file's
// mtime changes; a failed reload keeps the previous good value (see Config::maybe_reload).
struct Config {
  struct {
    std::string base_url = "http://localhost:3000";
    std::string device_id = "jetson_ctrl_01";
    long http_timeout_ms = 2000;
  } web;

  struct {
    long control_tick_ms = 2000;
    int network_every_ticks = 5;
    long dht22_min_interval_ms = 2000;
  } cadence;

  struct {
    std::string sysfs_root = "/sys/class/thermal";
    std::vector<std::string> zone_names{"CPU-therm", "GPU-therm"};
  } thermal;

  struct {
    std::string gpiochip = "gpiochip0";
    unsigned line_offset = 149;  // Nano header pin 29; verify with gpioinfo
    int max_retries = 3;
    long stale_after_ms = 30000;
  } dht22;

  struct {
    std::string gpiochip = "gpiochip0";
    unsigned line_offset = 200;  // Nano header pin 31; 194 is SD card-detect
    bool active_high = true;     // bench-test before trusting: fan.cpp fails to ON
  } fan;

  struct {
    double temp_on_c = 60.0;
    double temp_off_c = 52.0;
    double enclosure_air_on_c = 45.0;
    double enclosure_air_off_c = 40.0;
    int min_on_seconds = 30;
    int min_off_seconds = 30;
  } control;

  struct {
    int max_duration_minutes = 120;
  } override_;
};

// Owns the live config + the file mtime it was loaded from.
class ConfigStore {
 public:
  explicit ConfigStore(std::string path) : path_(std::move(path)) {}

  // Load once at startup. Throws std::runtime_error on failure (fatal at boot).
  const Config& load();

  // Cheap per-tick check: if the file mtime changed, parse+validate. On success,
  // swap in the new config and return true. On any error, log and KEEP the old one.
  bool maybe_reload();

  const Config& get() const { return cfg_; }

 private:
  bool parse_file(Config& out) const;  // parse + validate into `out`

  std::string path_;
  Config cfg_;
  std::time_t mtime_ = 0;
};
