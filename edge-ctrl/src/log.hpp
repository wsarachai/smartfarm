#pragma once
// Tiny stderr logger. journalctl captures stderr for the systemd unit, so we keep
// this dependency-free and just prefix a level. Not hot-path sensitive.
#include <cstdarg>
#include <cstdio>
#include <ctime>

namespace logging {

inline void line(const char* level, const char* fmt, ...) {
  char ts[32];
  std::time_t t = std::time(nullptr);
  std::strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", std::localtime(&t));
  std::fprintf(stderr, "%s [%s] ", ts, level);
  va_list ap;
  va_start(ap, fmt);
  std::vfprintf(stderr, fmt, ap);
  va_end(ap);
  std::fputc('\n', stderr);
}

#define LOG_INFO(...)  ::logging::line("INFO",  __VA_ARGS__)
#define LOG_WARN(...)  ::logging::line("WARN",  __VA_ARGS__)
#define LOG_ERROR(...) ::logging::line("ERROR", __VA_ARGS__)

}  // namespace logging
