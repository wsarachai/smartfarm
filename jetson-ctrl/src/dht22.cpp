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
constexpr char kConsumer[] = "jetson-ctrl-dht22";

// Busy-wait until the line reaches `level` or `timeout_us` elapses.
// Returns microseconds waited, or -1 on timeout. (Polling libgpiod is the
// pragmatic approach on the Nano; see DESIGN.md — this is the timing-fragile part.)
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
  // Best-effort real-time scheduling + memory locking to reduce the scheduler
  // jitter that corrupts the bit-bang. Failures are non-fatal (we just tolerate a
  // higher drop rate) — the DHT22 is secondary by design.
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
    // Respect the DHT22's 0.5 Hz ceiling (and let config tune it).
    long interval = min_interval_ms_.load();
    for (long slept = 0; slept < interval && running_; slept += 50) {
      timespec ts{0, 50L * 1000 * 1000};
      nanosleep(&ts, nullptr);
    }
  }
}

// One bit-bang attempt. Returns true and fills t/h on a checksum-valid frame.
//
// NOTE: hardware-timing code — structured but NOT yet verified on a real Jetson.
// DHT22 frame: host pulls low >=1ms, releases; sensor replies 80us low + 80us high,
// then 40 data bits where a ~26-28us high = 0 and ~70us high = 1. 5 bytes:
// [RH_hi RH_lo T_hi T_lo checksum]; RH and T are tenths; T sign in the high bit.
bool Dht22::read_once(double& t_out, double& h_out) {
  gpiod_chip* chip = gpiod_chip_open_by_name(chip_.c_str());
  if (!chip) return false;
  gpiod_line* line = gpiod_chip_get_line(chip, line_);
  if (!line) { gpiod_chip_close(chip); return false; }

  bool ok = false;
  uint8_t bytes[5] = {0, 0, 0, 0, 0};

  // Start pulse: drive low ~2ms, then release to input (external pull-up idles high).
  if (gpiod_line_request_output(line, kConsumer, 0) == 0) {
    gpiod_line_set_value(line, 0);
    timespec ts{0, 2L * 1000 * 1000};  // 2 ms
    nanosleep(&ts, nullptr);
    gpiod_line_release(line);

    if (gpiod_line_request_input(line, kConsumer) == 0) {
      // Sensor response preamble: ~80us low, ~80us high.
      if (wait_level(line, 0, 200) >= 0 && wait_level(line, 1, 200) >= 0 &&
          wait_level(line, 0, 200) >= 0) {
        ok = true;
        for (int i = 0; i < 40 && ok; ++i) {
          if (wait_level(line, 1, 200) < 0) { ok = false; break; }  // start of bit
          long high_us = wait_level(line, 0, 200);                  // length of high
          if (high_us < 0) { ok = false; break; }
          bytes[i / 8] <<= 1;
          if (high_us > 45) bytes[i / 8] |= 1;  // >~45us => '1'
        }
      }
      gpiod_line_release(line);
    }
  }
  gpiod_chip_close(chip);

  if (!ok) return false;
  uint8_t sum = bytes[0] + bytes[1] + bytes[2] + bytes[3];
  if (sum != bytes[4]) return false;  // checksum

  h_out = ((bytes[0] << 8) | bytes[1]) * 0.1;
  int raw_t = ((bytes[2] & 0x7F) << 8) | bytes[3];
  t_out = raw_t * 0.1;
  if (bytes[2] & 0x80) t_out = -t_out;
  return true;
}
