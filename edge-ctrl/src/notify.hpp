#pragma once
// Minimal sd_notify() implementation (no libsystemd dependency).
//
// systemd sets $NOTIFY_SOCKET for a Type=notify service; we send datagrams to it:
//   "READY=1"        once, after startup
//   "WATCHDOG=1"     every control tick, to feed WatchdogSec
// If NOTIFY_SOCKET is unset (e.g. running by hand), these are no-ops.
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace notify {

inline void send(const char* state) {
  const char* path = std::getenv("NOTIFY_SOCKET");
  if (!path || !*path) return;

  int fd = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  if (fd < 0) return;

  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  std::strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
  // Abstract namespace sockets start with '@' in $NOTIFY_SOCKET → leading NUL.
  if (addr.sun_path[0] == '@') addr.sun_path[0] = '\0';

  (void)::sendto(fd, state, std::strlen(state), MSG_NOSIGNAL,
                 reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
  ::close(fd);
}

inline void ready()    { send("READY=1"); }
inline void watchdog() { send("WATCHDOG=1"); }

}  // namespace notify
