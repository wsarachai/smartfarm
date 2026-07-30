#pragma once
#include <string>
#include <vector>
#include <map>

// Reads the Jetson on-die thermal zones. Zones are resolved BY NAME (each
// thermal_zone*/type) because zone indices reorder across L4T kernels.
class Thermal {
 public:
  struct Reading {
    bool valid = false;                    // false => fail-to-cooling
    double max_c = 0.0;                     // max over the resolved allowlist
    std::map<std::string, double> per_zone; // name -> °C (for telemetry)
  };

  Thermal(std::string sysfs_root, std::vector<std::string> zone_names);

  // (Re)scan sysfs and map each requested name -> its thermal_zoneN/temp path.
  // Safe to call after a config reload (zone_names may have changed).
  void resolve();

  // Read every resolved zone; max over them. valid=false if none could be read.
  Reading read() const;

 private:
  std::string sysfs_root_;
  std::vector<std::string> want_;            // requested zone names
  std::map<std::string, std::string> paths_; // name -> .../tempN path
};
