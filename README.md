# OffhandQoL CI fix patch

Copy `.github/workflows/build.yml` and `xmake.lua` into the corresponding paths in the full OffhandQoL repository.

This patch follows the build order and NDK setup style used by `indpriyanshuraj/Freecam-mcpe-test` without copying its mod code. It removes the xmake setup action's package/build cache-key helpers, which can fail before compilation, while preserving xmake compiler caching through `--ccache=y` and `build.ccache`.

The workflow checks the actual NDK `clang++` path before xmake runs. If that check fails, the setup-ndk output is the problem; if it passes and PreLoader CMake still reports a missing compiler, inspect the CMake configure command/log and any stale xmake package cache.
