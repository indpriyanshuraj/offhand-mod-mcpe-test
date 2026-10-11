# Offhand QoL (LeviLaunchroid / Android)

A small native mod for **Minecraft Bedrock 1.26.52.3**. The intended scope is deliberately limited to an always-visible offhand HUD slot and one Android **Swap** button. It does not implement offhand use/interact, custom host packets, host handshakes, or a server mod.

## Current implementation status

- [x] Native Levi/PreLoader lifecycle registration.
- [x] One always-visible `Swap` button; no use/place/interact button.
- [x] Poll-and-settle game-library detection and exact BuildID gate.
- [x] Exact-BuildID, read-only trace for native Swap-action slot headers and network-ID variants.
- [x] RP HUD slot background always visible (no `item_id_aux` visibility binding).
- [x] Build/package pipeline using xmake, Android NDK r28c and a `.levipack` verifier.
- [x] Unit tests for the pure swap-operation decision policy.
- [x] Build-time ELF architecture/registration-export verifier and package structure verifier.
- [ ] Native HUD container integration so the slot's item/count/durability bindings receive the offhand collection.
- [ ] Server-authoritative vanilla `ItemStackRequest` backend.

**This is an initial source milestone, not a functioning swap release yet.** Until the request builder is verified against the supplied Android binary, clicking Swap deliberately does not modify inventory or send a guessed request. This fail-closed guard prevents client/server desynchronization and potential item loss. The CI can build and package the current scaffold, but an uploaded `.levipack` should not be described as feature-complete until the two unchecked runtime items are implemented and tested in-game.

## Target

- Minecraft library: `libminecraftpe.so`
- Exact expected BuildID: `56de9eed077631e03a31f4f58eb2f0e00071d335`
- ABI: `arm64-v8a`
- Toolchain: Android NDK r28c, xmake, Python 3

The mod refuses to enable gameplay code against an unrecognized BuildID. Update `include/offhand/version.hpp` and the version-specific research before supporting another game build.

## Build

```bash
export ANDROID_NDK_HOME=/path/to/android-ndk-r28c
./scripts/build-android.sh
```

Equivalent direct commands:

```bash
export XMAKE_COLORTERM=truecolor
export XMAKE_THEME=default
xmake f -y -p android -a arm64-v8a -m release \
  --ndk="$ANDROID_NDK_HOME" --ccache=y --policies=build.ccache:y
xmake -y -v
python3 scripts/verify_levipack.py build/android/arm64-v8a/release/OffhandQoL.levipack
```

The `Android Build` GitHub Actions workflow performs the same configure/build/package path, runs the pure policy test, verifies the resulting ELF and `.levipack`, and uploads `OffhandQoL.levipack` as a workflow artifact. Its xmake setup enables build caching but disables the separate package-cache feature, avoiding the failing `xmake l utils.ci.packageskey` calculation while preserving xmake build cache and compiler cache. The local environment used for this source milestone does not have xmake or the Android NDK configured, so a native build was not executed in this session.

Local xmake uses the `build.ccache` policy. The local build script and CI set `XMAKE_COLORTERM=truecolor`, `XMAKE_THEME=default`, and `FORCE_COLOR=1`; xmake's own build output handles colour. Project messages use `print()` because `cprint()` is not a global interface in the affected xmake version.

## Runtime logs

The mod calls Android's `__android_log_print` directly using the stable tag `OffhandQoL`; it does not depend on the PreLoader logger configuration. With a device connected and Android platform-tools installed, start the launcher and capture only this mod's log lines:

```bash
adb logcat -v threadtime -s OffhandQoL:V '*:S'
```

Equivalent helper: `./scripts/filter-logcat.sh`. To include crash context while debugging, use a second terminal with `adb logcat -v threadtime -s OffhandQoL:V Preloader:V AndroidRuntime:E DEBUG:E`.

The read-only request trace uses the same `OffhandQoL` Android log tag. On the verified target build, open the normal inventory and perform a vanilla operation that generates a Swap action; the trace logs source/destination slot headers and network-ID variant bytes. It forwards the native request unchanged and does not implement the offhand button action.

## Reference compatibility notes

The supplied `offhand.levipack` is an Android AArch64 package targeting the same observed BuildID, but its strings and description indicate a custom Offhand host handshake and host-side support for swap/use. FrederoxDev's [`Offhand`](https://github.com/FrederoxDev/Offhand) source is a Windows/Amethyst mod pinned to Bedrock 1.26.42; its `SwapOffhandPacket` handler changes server inventory directly, which cannot be reused as a vanilla-server request. The implementation here borrows the feature decomposition (swap button, offhand HUD model/slot and scoped item operations) but keeps custom packet sending and client-only inventory mutation out of the code path.

## Architecture

- `RuntimeProbe` waits for `libminecraftpe.so`, allows initialization to settle, then checks the exact BuildID.
- `RequestTrace` hooks the target request-action ownership-transfer helper only after that gate, logs the payload of native Swap actions, and unhooks on module/mod disable.
- `SwapPolicy` is a game-independent decision function that will select one of move-main-to-offhand, move-offhand-to-main, swap-hands, or no-op.
- `Mod` owns the module and Android button lifecycle.
- The RP adds the offhand slot background and keeps native collection bindings; it no longer carries unrelated custom-use animation/attachable assets.
- Future request construction must go through the game's client inventory request manager, using the stack network IDs of the current slots. It must not mutate local inventory directly or use the old custom `SwapOffhandPacket`.
