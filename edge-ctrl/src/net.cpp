#include "net.hpp"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include "log.hpp"

using nlohmann::json;

namespace {
size_t sink(char* ptr, size_t size, size_t nmemb, void* userdata) {
  auto* out = static_cast<std::string*>(userdata);
  out->append(ptr, size * nmemb);
  return size * nmemb;
}
}  // namespace

namespace net {

void global_init() { curl_global_init(CURL_GLOBAL_DEFAULT); }
void global_cleanup() { curl_global_cleanup(); }

bool post_telemetry(const std::string& base_url, long timeout_ms,
                    const std::string& json_body) {
  CURL* c = curl_easy_init();
  if (!c) return false;
  std::string url = base_url + "/api/v1/telemetry";
  std::string resp;
  curl_slist* hdrs = curl_slist_append(nullptr, "Content-Type: application/json");

  curl_easy_setopt(c, CURLOPT_URL, url.c_str());
  curl_easy_setopt(c, CURLOPT_POST, 1L);
  curl_easy_setopt(c, CURLOPT_POSTFIELDS, json_body.c_str());
  curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdrs);
  curl_easy_setopt(c, CURLOPT_TIMEOUT_MS, timeout_ms);
  curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT_MS, timeout_ms);
  curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, sink);
  curl_easy_setopt(c, CURLOPT_WRITEDATA, &resp);

  CURLcode rc = curl_easy_perform(c);
  long code = 0;
  curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);
  curl_slist_free_all(hdrs);
  curl_easy_cleanup(c);

  if (rc != CURLE_OK) {
    LOG_WARN("telemetry POST failed: %s", curl_easy_strerror(rc));
    return false;
  }
  if (code < 200 || code >= 300) {
    LOG_WARN("telemetry POST http %ld", code);
    return false;
  }
  return true;
}

OverridePayload fetch_override(const std::string& base_url, long timeout_ms,
                               const std::string& device_id) {
  OverridePayload out;
  CURL* c = curl_easy_init();
  if (!c) return out;
  std::string url = base_url + "/api/v1/devices";
  std::string resp;

  curl_easy_setopt(c, CURLOPT_URL, url.c_str());
  curl_easy_setopt(c, CURLOPT_TIMEOUT_MS, timeout_ms);
  curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT_MS, timeout_ms);
  curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, sink);
  curl_easy_setopt(c, CURLOPT_WRITEDATA, &resp);

  CURLcode rc = curl_easy_perform(c);
  curl_easy_cleanup(c);
  if (rc != CURLE_OK) {
    LOG_WARN("override GET failed: %s", curl_easy_strerror(rc));
    return out;
  }

  try {
    json j = json::parse(resp);
    // /api/v1/devices returns an array of device objects.
    for (const auto& dev : j) {
      if (!dev.contains("device_id") || dev["device_id"] != device_id) continue;
      // Override lives in lastCommand (what POST /api/v1/control stored).
      if (dev.contains("lastCommand") && dev["lastCommand"].is_object()) {
        const auto& lc = dev["lastCommand"];
        if (lc.contains("external_fan_override")) {
          out.present = true;
          out.mode = lc["external_fan_override"].get<std::string>();
          if (lc.contains("until") && lc["until"].is_string())
            out.until_iso = lc["until"].get<std::string>();
        }
      }
      break;
    }
  } catch (const std::exception& e) {
    LOG_WARN("override parse failed: %s", e.what());
  }
  return out;
}

}  // namespace net
