# OffhandQoL build fix — fmt header dependency

This patch addresses the compile error:

```
fatal error: 'fmt/format.h' file not found
```

PreLoader's logger header includes fmt headers, but the project only declared the PreLoader package. The replacement `xmake.lua` declares `fmt` directly with `add_requires("preloader", "fmt")` and attaches both packages to the target.

## Apply

Copy this `xmake.lua` over the repository's root `xmake.lua`, then reconfigure and rebuild from the repository root:

```bash
export ANDROID_NDK_ROOT="$NDK_PATH"
export ANDROID_NDK_HOME="$NDK_PATH"
xmake f -c -y -p android -a arm64-v8a -m release \
  --ndk="$NDK_PATH" --ccache=y --policies=build.ccache:y
xmake -y -v
```

The NDK path must point to the actual NDK directory, not directly to `clang++`.

## Status / next runtime step

This patch only addresses dependency discovery; it does not implement the inventory mutation backend. The repository currently intentionally leaves `requestSwap()` fail-closed until the live request manager and network-ID variants are verified. Do not remove that guard based only on a successful build.

For device diagnostics, capture the mod's Android log tag:

```bash
adb logcat -c
adb logcat -v threadtime -s OffhandQoL
```

Launch Minecraft through LeviLauncher, enable the mod, and press **Swap** once. Save the `OffhandQoL` lines, plus any crash stack from the same reproduction, to validate runtime initialization. I cannot inspect a device's launcher logcat remotely unless you provide that output.
