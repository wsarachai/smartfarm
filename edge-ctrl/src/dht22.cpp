#include "dht22.hpp"

#include <gpiod.h>
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
#include <time.h>
#include <cstdint>

#include "log.hpp"

using std::chrono::milliseconds;
using std::chrono::steady_clock;

namespace {
constexpr char kConsumer[] = "edge-ctrl-dht22";

#if HAVE_GPIOD_V2
// Busy-wait until the line reaches `level` or `timeout_us` elapses (libgpiod v2).
long wait_level(gpiod_line_request* req, unsigned int offset, int level, long timeout_us) {
  timespec t0, now;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  gpiod_line_value expected = level ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
  for (;;) {
    if (gpiod_line_request_get_value(req, offset) == expected) {
      clock_gettime(CLOCK_MONOTONIC, &now);
      return (now.tv_sec - t0.tv_sec) * 1000000L +
             (now.tv_nsec - t0.tv_nsec) / 1000L;
    }
    clock_gettime(CLOCK_MONOTONIC, &now);
    long us = (now.tv_sec - t0.tv_sec) * 1000000L +
              (now.tv_nsec - t0.tv_nsec) / 1000L;
    if (us > timeout_us) return -1;
  }
}
#else
// Busy-wait until the line reaches `level` or `timeout_us` elapses (libgpiod v1).
long wait_level(gpiod_line* line, int level, long timeout_us) {
  timespec t0, now;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  for (;;) {
    if (gpiod_line_get_value(line) == level) {
      clock_gettime(CLOCK_MONOTONIC, &now);
      return (now.tv_sec - t0.tv_sec) * 1000000L +
             (now.tv_nsec - t0.tv_nsec) / 1000L;
    }
    clock_gettime(CLOCK_MONOTONIC, &now);
    long us = (now.tv_sec - t0.tv_sec) * 1000000L +
              (now.tv_nsec - t0.tv_nsec) / 1000L;
    if (us > timeout_us) return -1;
  }
}
#endif
}  // namespace

Dht22::Dht22(std::string gpiochip, unsigned line_offset, int max_retries,
             milliseconds min_interval, milliseconds stale_after)
    : chip_(std::move(gpiochip)),
      line_(line_offset),
      max_retries_(max_retries),
      min_interval_ms_(min_interval.count()),
      stale_after_ms_(stale_after.count()) {}

Dht22::~Dht22() { stop(); }

void Dht22::start() {
  running_ = true;
  th_ = std::thread([this] { run(); });
}

void Dht22::stop() {
  running_ = false;
  if (th_.joinable()) th_.join();
}

Dht22::Sample Dht22::snapshot() const {
  std::lock_guard<std::mutex> lk(mu_);
  Sample s = last_;
  if (s.valid) {
    auto age = std::chrono::duration_cast<milliseconds>(
                   steady_clock::now() - last_ok_)
                   .count();
    s.stale = age > stale_after_ms_.load();
  }
  return s;
}

void Dht22::run() {
  sched_param sp{};
  sp.sched_priority = 10;
  if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp) != 0)
    LOG_WARN("dht22: SCHED_FIFO unavailable (run as root); continuing best-effort");
  if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0)
    LOG_WARN("dht22: mlockall failed; continuing best-effort");

  while (running_) {
    double t = 0, h = 0;
    bool ok = false;
    for (int i = 0; i < max_retries_ && running_ && !ok; ++i) {
      ok = read_once(t, h);
      if (!ok) {
        timespec ts{0, 60L * 1000 * 1000};  // 60 ms between retries
        nanosleep(&ts, nullptr);
      }
    }
    if (ok) {
      std::lock_guard<std::mutex> lk(mu_);
      last_.valid = true;
      last_.temp_c = t;
      last_.humidity = h;
      last_ok_ = steady_clock::now();
    }
    long interval = min_interval_ms_.load();
    for (long slept = 0; slept < interval && running_; slept += 50) {
      timespec ts{0, 50L * 1000 * 1000};
      nanosleep(&ts, nullptr);
    }
  }
}

// One bit-bang attempt. Returns true and fills t/h on a checksum-valid frame.
bool Dht22::read_once(double& t_out, double& h_out) {
#if HAVE_GPIOD_V2
  std::string dev_path = chip_;
  if (dev_path.rfind("/dev/", 0) != 0) dev_path = "/dev/" + dev_path;

  gpiod_chip* chip = gpiod_chip_open(dev_path.c_str());
  if (!chip) {
    for (const char* fb : {"/dev/gpiochip0", "/dev/gpiochip4", "/dev/gpiochip1"}) {
      chip = gpiod_chip_open(fb);
      if (chip) break;
    }
  }
  if (!chip) return false;

  bool ok = false;
  uint8_t bytes[5] = {0, 0, 0, 0, 0};

  gpiod_line_settings* out_set = gpiod_line_settings_new();
  gpiod_line_settings_set_direction(out_set, GPIOD_LINE_DIRECTION_OUTPUT);
  gpiod_line_settings_set_output_value(out_set, GPIOD_LINE_VALUE_INACTIVE);

  gpiod_line_config* line_cfg = gpiod_line_config_new();
  unsigned int offsets[1] = { line_ };
  gpiod_line_config_add_line_settings(line_cfg, offsets, 1, out_set);

  gpiod_request_config* req_cfg = gpiod_request_config_new();
  gpiod_request_config_set_consumer(req_cfg, kConsumer);

  gpiod_line_request* req = gpiod_chip_request_lines(chip, req_cfg, line_cfg);

  gpiod_line_settings_free(out_set);
  gpiod_line_config_free(line_cfg);
  gpiod_request_config_free(req_cfg);

  if (req) {
    timespec ts{0, 2L * 1000 * 1000};  // 2 ms
    nanosleep(&ts, nullptr);

    gpiod_line_settings* in_set = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(in_set, GPIOD_LINE_DIRECTION_INPUT);

    gpiod_line_config* in_cfg = gpiod_line_config_new();
    gpiod_line_config_add_line_settings(in_cfg, offsets, 1, in_set);

    if (gpiod_line_request_reconfigure_lines(req, in_cfg) == 0) {
      if (wait_level(req, line_, 0, 200) >= 0 && wait_level(req, line_, 1, 200) >= 0 &&
          wait_level(req, line_, 0, 200) >= 0) {
        ok = true;
        for (int i = 0; i < 40 && ok; ++i) {
          if (wait_level(req, line_, 1, 200) < 0) { ok = false; break; }
          long high_us = wait_level(req, line_, 0, 200);
          if (high_us < 0) { ok = false; break; }
          bytes[i / 8] <<= 1;
          if (high_us > 45) bytes[i / 8] |= 1;
        }
      }
    }
    gpiod_line_settings_free(in_set);
    gpiod_line_config_free(in_cfg);
    gpiod_line_request_release(req);
  }
  gpiod_chip_close(chip);

#else
  // libgpiod v1
  gpiod_chip* chip = gpiod_chip_open_by_name(chip_.c_str());
  if (!chip && chip_.rfind("/dev/", 0) == 0) chip = gpiod_chip_open(chip_.c_str());
  if (!chip && chip_.rfind("/dev/", 0) != 0) {
    std::string dev_path = "/dev/" + chip_;
    chip = gpiod_chip_open(dev_path.c_str());
  }
  if (!chip) {
    for (const char* fb : {"gpiochip0", "/dev/gpiochip0", "gpiochip4", "/dev/gpiochip4"}) {
      chip = gpiod_chip_open_by_name(fb);
      if (!chip) chip = gpiod_chip_open(fb);
      if (chip) break;
    }
  }
  if (!chip) return false;
  gpiod_line* line = gpiod_chip_get_line(chip, line_);
  if (!line) { gpiod_chip_close(chip); return false; }

  bool ok = false;
  uint8_t bytes[5] = {0, 0, 0, 0, 0};

  if (gpiod_line_request_output(line, kConsumer, 0) == 0) {
    gpiod_line_set_value(line, 0);
    timespec ts{0, 2L * 1000 * 1000};  // 2 ms
    nanosleep(&ts, nullptr);
    gpiod_line_release(line);

    if (gpiod_line_request_input(line, kConsumer) == 0) {
      if (wait_level(line, 0, 200) >= 0 && wait_level(line, 1, 200) >= 0 &&
          wait_level(line, 0, 200) >= 0) {
        ok = true;
        for (int i = 0; i < 40 && ok; ++i) {
          if (wait_level(line, 1, 200) < 0) { ok = false; break; }
          long high_us = wait_level(line, 0, 200);
          if (high_us < 0) { ok = false; break; }
          bytes[i / 8] <<= 1;
          if (high_us > 45) bytes[i / 8] |= 1;
        }
      }
      gpiod_line_release(line);
    }
  }
  gpiod_chip_close(chip);
#endif

  if (!ok) return false;
  uint8_t sum = bytes[0] + bytes[1] + bytes[2] + bytes[3];
  if (sum != bytes[4]) return false;

  h_out = ((bytes[0] << 8) | bytes[1]) * 0.1;
  int raw_t = ((bytes[2] & 0x7F) << 8) | bytes[3];
  t_out = raw_t * 0.1;
  if (bytes[2] & 0x80) t_out = -t_out;
  return true;
}
