#!/bin/bash
# Install the DS3231 host timekeeping scripts + systemd units on the Jetson.
#
#   sudo ./install.sh
#
# Idempotent: re-run after editing any script or unit. Does NOT set the RTC --
# see docs/host-setup.md for the one-time migration if the chip currently holds
# LOCAL time, or is unset (OSF).
set -euo pipefail

if [ "$(id -u)" -ne 0 ]; then
  echo "install.sh: must run as root (sudo ./install.sh)" >&2
  exit 1
fi

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN=/usr/local/bin
UNITS=/etc/systemd/system
DOCS=/usr/share/doc/jetson-ctrl

echo "==> scripts -> $BIN"
install -m 0644 "$HERE/ds3231.py"            "$BIN/ds3231.py"
install -m 0755 "$HERE/ds3231_sync.py"       "$BIN/ds3231_sync.py"
install -m 0755 "$HERE/ds3231_writeback.py"  "$BIN/ds3231_writeback.py"

echo "==> units -> $UNITS"
install -m 0644 "$HERE/systemd/ds3231-sync.service"      "$UNITS/"
install -m 0644 "$HERE/systemd/ds3231-writeback.service" "$UNITS/"
install -m 0644 "$HERE/systemd/ds3231-writeback.timer"   "$UNITS/"

if [ -f "$HERE/../docs/host-setup.md" ]; then
  echo "==> docs -> $DOCS"
  install -d "$DOCS"
  install -m 0644 "$HERE/../docs/host-setup.md" "$DOCS/host-setup.md"
fi

# i2c-dev must be loaded before sysinit runs the sync unit.
if ! grep -qx 'i2c-dev' /etc/modules-load.d/*.conf 2>/dev/null; then
  echo "==> enabling i2c-dev at boot"
  echo 'i2c-dev' > /etc/modules-load.d/ds3231.conf
fi

echo "==> reloading systemd"
systemctl daemon-reload
systemctl enable ds3231-sync.service
systemctl enable --now ds3231-writeback.timer

cat <<'EOF'

Installed. Verify before trusting it:

  sudo i2cdetect -y -r 1          # 0x68 must be present
  sudo systemctl start ds3231-sync
  systemctl status ds3231-sync
  journalctl -u ds3231-sync -b

If the RTC still holds LOCAL time (the old convention), migrate it once while
the system clock is correct:

  sudo /usr/bin/python3 /usr/local/bin/ds3231_writeback.py --force

EOF
