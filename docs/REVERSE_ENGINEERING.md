# Offhand QoL Reverse-Engineering Notes

## Target identity

- Target file supplied for this project: `libminecraftpe-v1.26.52.3.so`
- ELF: 64-bit little-endian AArch64 shared object
- Expected BuildID: `56de9eed077631e03a31f4f58eb2f0e00071d335`
- Native mod scope: client-side QoL only; no new server packet or protocol handshake.

## Confirmed protocol structures

Cross-checked against Bedrock protocol schema and LeviLamina's public generated headers (used only as candidates; runtime use must be checked against Android target):

- `ItemStackRequestPacket` ID: 147.
- `ItemStackRequestActionType::Swap`: 2.
- `FullContainerName` consists of a container enum plus optional dynamic ID.
- `ContainerEnumName`: `HotbarContainer=28`, `InventoryContainer=29`, `OffhandContainer=34`.
- `ItemStackRequestSlotInfo` carries `FullContainerName`, slot byte and `ItemStackNetIdVariant`.
- `ItemStackRequestActionSwap` inherits transfer-action storage for source and destination slot info.
- `ItemStack` contains `mNetIdVariant`; `ItemStackNetManagerClient` has `_beginRequest`, `addRequestAction`, `_endRequest` and response-handling machinery.

These facts establish that the protocol has a native swap action. They do **not** by themselves prove the exact HUD container IDs, stack-ID variant values, manager offsets or the safe way to start/send a request when the inventory screen is closed.

## Candidate manager and action layout

LeviLamina's generated `Player.h` includes `std::unique_ptr<ItemStackNetManagerBase> mItemStackNetManager`, and its client-side derived manager exposes the request functions above. A candidate layout derived from that header placed the pointer around `Player + 0xA70` when calibrated using a BedrockTools `PlayerInventory` offset. **Do not use this candidate in production without independently verifying it against the supplied Android `.so`.**

The target ELF has RTTI/name data for `ItemStackRequestActionSwap`. A read-only relocation scan found a candidate vtable group whose typeinfo name resolves to that class. This is useful for later binary analysis, not yet a safe call target: its derived vtable slots and class/field layouts still require exact verification and source-level wrapper design.


## Additional target-ELF findings (static analysis)

The following addresses are ELF virtual addresses / file offsets from the supplied `libminecraftpe-v1.26.52.3.so`; they are **research notes, not an implementation contract**. All use is gated to the BuildID above.

| Candidate | Address | Evidence / confidence |
|---|---:|---|
| `ItemStackNetManagerClient::_beginRequest` | `0xF88DF04` | Function prologue is followed by the `_beginRequest` string reference at `0xF88E004`; code branches on an 8-bit request-screen argument. Strong candidate, ABI still needs call-site confirmation. |
| `ItemStackNetManagerClient::_endRequest` | `0xF88E0C4` | Function body checks manager request state and contains the `_endRequest` error string reference at `0xF88E344`. Strong candidate, ABI still needs call-site confirmation. |
| `ItemStackNetManagerClient::addRequestAction` validation wrapper | `0xF88F0FC` | Validates the current action and includes the success/failure string references at `0xF88F1A4` / `0xF88F284`; it then passes the unique-action holder to the vector-insertion helper. Do not confuse this wrapper with the actual ownership-transfer helper. |
| Request-action ownership-transfer helper | `0x10041D9C` | AArch64 disassembly directly moves `*uniqueAction` into the request action array and zeroes the caller's holder (`ldr x8, [x1]`, `str xzr, [x1]`, `str x8, [x22], #8`). Two-argument, void-return ABI is supported by the body and call sites. Best passive observation point for a read-only trace; exact semantic class name remains inferred. |
| `ItemStackRequestActionSwap` vtable address point | `0x13014E38` | RTTI name `26ItemStackRequestActionSwap`, a relocation to its typeinfo, and destructor logic touching source/destination slot-info variants at offsets consistent with two 40-byte slots. Candidate validated by static layout evidence, but not callable until the complete table and object initialization are reviewed. |
| BedrockTools `ClientInstanceUpdate` signature match | `0x98036A4` | The exact signature matched once in the target ELF; code uses `this` and a 32-bit second argument. It is a suitable candidate for the game's client-frame/tick integration after runtime ABI confirmation. |
| BedrockTools `ContainerScreenControllerHandleAutoPlace` signature match | `0x9A262D4` | Exact pattern matched once. Useful for comparison with native UI inventory operations, not itself a solution to bypassing the screen. |

### Swap-action constructor and request-scope confirmation (new)

Additional disassembly around the three direct call sites to `0xF87B400` confirms more than the earlier RTTI-only observation:

- `ItemStackRequestActionSwap` constructor candidate: `0xF87B400`.
- All three direct callers allocate `0x60` bytes for the action before invoking it (`0xCD92640`, `0xF92FC40`, `0xF931FF8`).
- The constructor passes action enum value `2` to the base action constructor, then places its final vtable address point at `0x13014E38`.
- The source and destination slot-info payloads begin at action offsets `+0x10` and `+0x38`. Each is 40 bytes (`0x28`): a 13-byte header copied inline plus a discriminated net-ID variant copied through `0xF887710`.
- The copy helper `0xF887710` resets the variant discriminator to `-1`, then dispatches variant-copy behavior by the source discriminator. This confirms the variant cannot safely be zero-filled or synthesized from item identity.

The request scope and action ownership helpers are also visible around `0x1004205C`–`0x10042330`: the scope stores the manager pointer and tracks whether `_endRequest` is required; its action helper transfers a `unique_ptr` payload to `addRequestAction`. This strongly supports using the game's scoped request lifecycle rather than constructing/sending a packet directly. It does **not** yet identify a safe way to retrieve the live manager or each slot's network-ID variant from the local player.

These constructor and helper addresses are private implementation details for this exact BuildID. They are not used to build a swap action yet.

The source milestone now includes a **read-only request trace** at `moduleBase + 0x10041D9C`, installed only after the exact BuildID gate passes and removed on mod disable/unload. It observes actions whose native action-type byte is `2`, logs the 13-byte source/destination slot headers and the 24-byte net-ID variants using Android's `__android_log_print` with the `OffhandQoL` tag, then forwards the original action holder unchanged. This is intended to capture native data generated by ordinary inventory operations on-device; it must never write into the action or cancel the request.

To gather a sample on the target build, open the normal inventory and perform a vanilla operation that swaps two occupied slots. Filter launcher logcat using `OffhandQoL`. A captured sample can reveal the live slot-header and variant encoding. It still will not prove the offhand slot's own header or empty-slot variant until a native offhand-originated action is observable.

### Signature findings that are not safe to consume as-is

- BedrockTools' current `ClientInstanceGetLocalPlayer` pattern matched at `0x9808050`, but disassembly looks like a hidden-return aggregate / local-player-changed connector path rather than a simple `LocalPlayer*` getter. Do not call it using an assumed `LocalPlayer* (*)(ClientInstance*)` ABI.
- The BedrockTools `ActorGetOffhandSlot` byte pattern matched at two addresses (`0xF579FEC` and `0xF57A064`). A non-unique signature must not be resolved by taking an arbitrary first match.
- The `Player + 0xA70` manager pointer and the exact `ItemStack` net-ID variant offsets remain candidate layouts from generated headers, not accepted production offsets.

These findings narrow the next RE task, but do not justify enabling packet/request code yet: we still need a verified local-player pointer on the game thread, independently confirmed manager offset, and exact slot/network-ID state as captured by vanilla HUD inventory operations.

## Required next reverse-engineering steps

1. Resolve the actual `ItemStackNetManagerClient` pointer from a player obtained on the game thread, using an independently verified Android offset or native accessor.
2. Determine the existing HUD request lifecycle and whether `_beginRequest(ItemStackRequestScreen::Hud)`, `addRequestAction`, and `_endRequest` can be safely invoked from the button callback or must be marshalled onto the game thread.
3. Confirm the live `HotbarContainer` vs `CombinedHotbarAndInventoryContainer` identifiers for HUD-originated requests. The offhand destination should use `OffhandContainer` and slot 0 only if confirmed by the game path.
4. Copy/construct each `ItemStackNetIdVariant` in the same way the native inventory UI does; do not assume zero or derive it from item names.
5. Verify the action vtable, already-observed `0x60`-byte `SwapAction` storage and 40-byte slot-info fields, optional container dynamic-ID initialization, request-ID lifecycle and destruction semantics in the Android target.
6. Observe request acceptance and response handling on the local integrated server and at least one remote vanilla server. Confirm rejected requests leave both local and server inventory unchanged.
7. For HUD rendering, verify whether the existing Android HUD model manager already registers `OffhandContainerModel` under `offhand_items`. The target ELF contains `OffhandContainerModel`, `HudContainerModel` and `offhand_items` metadata. Only if runtime inspection shows the collection is not registered, locate the Android implementation of `HudContainerManagerModel::_postInit` and add the missing model without assuming Windows vtable RVAs or allocators are compatible.

## Address correction

An earlier note in the initial research incorrectly described `0x1005A14C` as a candidate `ContainerScreenSimulation::tryAutoPlace` location. That claim is withdrawn. The string `ContainerScreenSimulation::tryAutoPlace - failed to init source sparsecontainer` is at rodata VMA/file offset `0x237C267`; its single direct AArch64 ADRP+ADD reference was observed at `0x1005A214`. This is a string-reference site, not proof of the function entry point or of the complete function's identity.

## References

- [BedrockTools](https://github.com/QYCottage/BedrockTools) — preferred Android mod structure/build/signature baseline.
- [LeviLamina generated headers](https://github.com/LiteLDev/LeviLamina) — candidate class/request structure declarations only; Windows-only build context means signatures/offsets are not assumed to match Android.
- [Endstone Bedrock protocol schema](https://github.com/EndstoneMC/protocol-docs) — packet/action/container enum cross-check.
- Supplied original `FrederoxDev/Offhand` source — native HUD container concept and legacy behavior reference. Its custom server packet and `TRANSACTION_SLOT=-2` path are explicitly not part of this QoL design.
