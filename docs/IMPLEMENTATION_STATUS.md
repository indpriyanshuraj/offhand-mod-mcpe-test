# Implementation Status

## Milestone A: buildable safe scaffold

Implemented in this source tree:

- PreLoader lifecycle registration and Mod Menu module registration.
- Exactly one `Swap` click button, always visible while the module is enabled.
- No `Interact`, `Use` or `Place` button registration.
- Runtime game library poll, settle delay and exact BuildID check.
- Always-visible RP offhand slot definition, keeping the source collection/item rendering controls.
- Pure operation policy and unit tests.
- xmake Android build, package generation and GitHub Actions artifact workflow.
- Build-time native ELF/export verifier and a package-content verifier.
- Explicit Android log tag `OffhandQoL` for logcat filtering.

## Not yet implemented

1. Hook/extend `HudContainerManagerModel::_postInit` on Android so the RP collection points at the real offhand container and displays a native ItemStack, count and durability.
2. Build the `ItemStackRequestActionSwap` via the real `ItemStackNetManagerClient` HUD request scope, with correct request ID and slot network-ID variants.
3. Verify request acceptance/rejection in local integrated worlds and remote vanilla servers after the backend is implemented.
4. Run the Android NDK build and in-game runtime test. The local workspace used for this milestone has no xmake installation or Android NDK configured, so no native `.so` or `.levipack` release artifact was produced here.

The current `requestSwap()` intentionally logs and returns without touching inventory. Do not remove this guard until the action object's layout, manager/request lifecycle, `FullContainerName` values and `ItemStackNetIdVariant` handling are verified against the exact target ELF.
