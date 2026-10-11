#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${ANDROID_NDK_HOME:-}" && -z "${ANDROID_NDK_ROOT:-}" ]]; then
  echo "Set ANDROID_NDK_HOME or ANDROID_NDK_ROOT to an Android NDK directory." >&2
  exit 2
fi

NDK_PATH="${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT}}"
export XMAKE_COLORTERM="${XMAKE_COLORTERM:-truecolor}"
export XMAKE_THEME="${XMAKE_THEME:-default}"
export FORCE_COLOR="${FORCE_COLOR:-1}"

xmake f -y -p android -a arm64-v8a -m release \
  --ndk="${NDK_PATH}" --ccache=y --policies=build.ccache:y
xmake -y -v
python3 scripts/verify_native_library.py build/android/arm64-v8a/release/libOffhandQoL.so
python3 scripts/verify_levipack.py build/android/arm64-v8a/release/OffhandQoL.levipack