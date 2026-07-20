#pragma once
#include <string>

// HTTP client to the Node web-server (Q2: this daemon is a pure client — no inbound
// port). All calls are best-effort with short timeouts and NEVER block the control
// loop meaningfully; a failure just logs and returns false/empty.
namespace net {

// Process-wide libcurl init/teardown.
void global_init();
void global_cleanup();

// POST a telemetry JSON body to <base_url>/api/v1/telemetry. Returns success.
bool post_telemetry(const std::string& base_url, long timeout_ms,
                    const std::string& json_body);

// The remote override as read from this device's lastCommand (desired-state).
// `present` is false if the network failed or no override has ever been set.
struct OverridePayload {
  bool present = false;
  std::string mode;       // "auto" | "force_on" (unknown values treated as auto)
  std::string until_iso;  // ISO-8601; empty if absent
};

// GET <base_url>/api/v1/devices, find `device_id`, return its lastCommand override.
OverridePayload fetch_override(const std::string& base_url, long timeout_ms,
                               const std::string& device_id);

}  // namespace net
