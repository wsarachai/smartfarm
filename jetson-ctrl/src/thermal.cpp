#include "thermal.hpp"

#include <dirent.h>
#include <fstream>
#include <algorithm>

#include "log.hpp"

namespace {
std::string read_trim(const std::string& path) {
  std::ifstream f(path);
  std::string s;
  std::getline(f, s);
  while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
    s.pop_back();
  return s;
}
}  // namespace

Thermal::Thermal(std::string sysfs_root, std::vector<std::string> zone_names)
    : sysfs_root_(std::move(sysfs_root)), want_(std::move(zone_names)) {
  resolve();
}

void Thermal::resolve() {
  paths_.clear();
  DIR* d = ::opendir(sysfs_root_.c_str());
  if (!d) {
    LOG_ERROR("thermal: cannot open %s", sysfs_root_.c_str());
    return;
  }
  // Walk thermal_zone* entries, read each /type, keep the ones we want.
  for (dirent* e; (e = ::readdir(d)) != nullptr;) {
    std::string name = e->d_name;
    if (name.rfind("thermal_zone", 0) != 0) continue;
    std::string base = sysfs_root_ + "/" + name;
    std::string type = read_trim(base + "/type");
    if (std::find(want_.begin(), want_.end(), type) != want_.end()) {
      paths_[type] = base + "/temp";
    }
  }
  ::closedir(d);

  for (const auto& w : want_) {
    if (!paths_.count(w))
      LOG_WARN("thermal: requested zone '%s' not found in sysfs", w.c_str());
  }
  LOG_INFO("thermal: resolved %zu/%zu zones", paths_.size(), want_.size());
}

Thermal::Reading Thermal::read() const {
  Reading r;
  double best = -1e9;
  for (const auto& kv : paths_) {
    std::ifstream f(kv.second);
    long milli;
    if (f >> milli) {
      double c = milli / 1000.0;   // sysfs temp is milli-°C
      r.per_zone[kv.first] = c;
      best = std::max(best, c);
      r.valid = true;
    }
  }
  if (r.valid) r.max_c = best;
  return r;  // valid=false => caller applies fail-to-cooling
}
