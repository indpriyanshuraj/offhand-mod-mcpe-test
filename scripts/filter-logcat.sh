#!/usr/bin/env bash
set -euo pipefail

if ! command -v adb >/dev/null 2>&1; then
  echo "adb was not found. Install Android platform-tools and connect the device." >&2
  exit 2
fi

# Keep the capture focused on this mod's stable Android log tag.
exec adb logcat -v threadtime -s OffhandQoL:V '*:S'