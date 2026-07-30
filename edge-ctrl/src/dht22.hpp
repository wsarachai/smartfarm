#pragma once
#include <atomic>
#include <mutex>
#include <thread>
#include <string>
#include <chrono>

// DHT22 (secondary, enclosure-air) reader.
//
// Bit-banging the DHT22 has microsecond-scale timing that a non-realtime kernel
// will occasionally corrupt, so the read runs in its OWN thread (SCHED_FIFO +
// mlockall attempted, best-effort) and publishes a last-good snapshot. The control
// loop only ever reads the cached snapshot — it never blocks on a device read, and
// a stale/failed DHT22 does NOT affect cooling (the thermal zones do that).
class Dht22 {
 public:
  struct Sample {
    bool valid = false;   // a good reading has ever landed
    bool stale = false;   // last good reading older than stale_after
    double temp_c = 0.0;
    double humidity = 0.0;
  };

  Dht22(std::string gpiochip, unsigned line_offset, int max_retries,
        std::chrono::milliseconds min_interval,
        std::chrono::milliseconds stale_after);
  ~Dht22();

  void start();  // spawns the sampling thread
  void stop();

  // Thread-safe snapshot for the control loop / telemetry.
  Sample snapshot() const;

  // Live-tunable knobs (config hot-reload); atomics so the sampler picks them up.
  void set_stale_after(std::chrono::milliseconds v) { stale_after_ms_ = v.count(); }
  void set_min_interval(std::chrono::milliseconds v) { min_interval_ms_ = v.count(); }

 private:
  void run();                             // thread body
  bool read_once(double& t, double& h);   // one bit-bang attempt (hardware)

  std::string chip_;
  unsigned line_;
  int max_retries_;
  std::atomic<long> min_interval_ms_;
  std::atomic<long> stale_after_ms_;

  std::thread th_;
  std::atomic<bool> running_{false};

  mutable std::mutex mu_;
  Sample last_;                                   // guarded by mu_
  std::chrono::steady_clock::time_point last_ok_; // guarded by mu_
};
