# Implementation Status

## Milestone A: buildable safe scaffold

Implemented in this source tree:

- PreLoader lifecycle registration and Mod Menu module registration.
- Exactly one `Swap` click button, always visible while the module is enabled.
- No `Interact`, `Use` or `Place` button registration.
- Runtime game library poll, settle delay and exact BuildID check.
- BuildID-gated, read-only hook at the target request-action ownership-transfer helper (`0x10041D9C`); it logs the raw source/destination slot-header and net-ID-variant bytes for native Swap actions, forwards the owning request unchanged, and is removed on disable/unload.
- Always-visible RP offhand slot definition, keeping the source collection/item rendering controls.
- Pure operation policy and unit tests.
- xmake Android build, package generation and GitHub Actions artifact workflow.
- Build-time native ELF/export verifier and a package-content verifier.
- CI xmake setup no longer runs the failing package-cache key calculation; the xmake build cache and ccache policy remain enabled.
- Explicit Android log tag `OffhandQoL` for logcat filtering.

## Not yet implemented

1. Verify at runtime whether the existing Android `HudContainerManagerModel` registers its built-in `OffhandContainerModel` under `offhand_items`. Static ELF metadata contains the model/collection names; only add a model registration hook if runtime inspection confirms it is missing.
2. Build the `ItemStackRequestActionSwap` (target constructor and object/slot-info sizes are now confirmed statically) via the real `ItemStackNetManagerClient` HUD request scope, with a verified manager pointer, correct request ID and live slot network-ID variants.
3. Verify request acceptance/rejection in local integrated worlds and remote vanilla servers after the backend is implemented.
4. Use the read-only trace on-device to capture a vanilla Swap-action sample, then validate its fields against live slots before implementing the request builder. The trace now targets the request-action vector ownership-transfer helper at `moduleBase + 0x10041D9C`, rather than the higher-level validation wrapper at `0xF88F0FC`.
5. Run the Android NDK build and in-game runtime test. The local workspace used for this milestone has no xmake installation or Android NDK configured, so no native `.so` or `.levipack` release artifact was produced here.

The current `requestSwap()` intentionally logs and returns without touching inventory. Do not remove this guard until the live manager retrieval, `FullContainerName` values, and runtime `ItemStackNetIdVariant` values are verified against the exact target ELF. Static analysis confirmed constructor `0xF87B400`, a `0x60`-byte swap action and two 40-byte slot-info fields. The new logger-only trace is meant to capture runtime variants; neither change alone provides a safe request builder.
