# Offhand QoL — Reverse Engineering Research

> **Address correction / priority warning (2026-10-11):** The exploratory text later in this document incorrectly called `0x1005a14c` a probable `ContainerScreenSimulation::tryAutoPlace` entry point. That identification is withdrawn. The confirmed evidence is the string's rodata location `0x237c267` and a direct code reference at `0x1005a214`; the reference site is **not proof of the function entry point**. Do not use `0x1005a14c` as a target or build it into a signature. Use [`REVERSE_ENGINEERING.md`](REVERSE_ENGINEERING.md) for the corrected, current findings.

**Project:** Levi / LeviLaunchroid Android native mod  
**Feature scope:** swap selected hotbar slot with offhand + visible offhand HUD slot only  
**Initial UI strategy:** reuse the existing Offhand RP, modified; use a LeviLaunchroid/PreLoader Android module button; no dynamic button visibility  
**Target Minecraft build:** Bedrock 1.26.52.3  
**Target ABI:** AArch64 / arm64-v8a  
**Research status:** Phase 1 — protocol/native target discovery in progress

---

## 1. Final feature contract

This project is **not** a continuation of the original full Offhand behavior.

The mod should provide only:

1. A single Android **Swap** button that is always registered/visible while the module is enabled.
2. An **always-visible offhand inventory slot** in the HUD.
3. A swap/move operation implemented through the game's normal inventory request path.

There is **no** offhand-use button, custom offhand interaction, custom attack, custom swing system, host handshake, or arbitrary-item offhand patching.

### Intended operation

Let:

- `M` = selected hotbar ItemStack
- `O` = offhand ItemStack
- `allow(M)` = the game's native decision that `M` may be placed in the offhand.

| Mainhand `M` | Offhand `O` | Button action |
|---|---|---|
| empty | empty | no-op |
| non-empty, allowed | empty | move `M` -> offhand |
| non-empty, allowed | non-empty | swap `M` <-> `O` |
| non-empty, not allowed | non-empty | move `O` -> selected hotbar slot; offhand becomes empty |
| non-empty, not allowed | empty | no-op |
| empty | non-empty | move `O` -> selected hotbar slot |

The button remains visible even for states where the operation is a no-op. The callback simply refuses an incompatible operation.

There is **no item-name whitelist**. Eligibility must come from the Minecraft runtime/item definition.

---

## 2. Target binary verification

The supplied target library is:

`libminecraftpe-v1.26.52.3.so.xz`

Decompressed target:

`/mnt/data/offhand_re/libminecraftpe-1.26.52.3.so`

Verified ELF metadata:

- ELF64
- AArch64
- Android API 26
- Build ID:
  `56de9eed077631e03a31f4f58eb2f0e00071d335`

The supplied compiled Levi Offhand module contains the same build identifier in its hook-gating logic, including strings equivalent to:

- `Full Android hook set installed (build 56de9eed)`
- `Unsupported libminecraftpe build; refusing to apply hooks`

Therefore the compiled Android Offhand build and the supplied `libminecraftpe-v1.26.52.3.so` are a matching target pair.

### Important scope rule

`libminecraftpe.so` / the supplied versioned library is used only to establish the exact Minecraft build and recover target-specific native layouts/signatures. It is not a runtime dependency to package with the mod.

---

## 3. Original Offhand source: what is useful and what must not be copied

Reference repository:

- `https://github.com/FrederoxDev/Offhand`

The original source provides useful behavioral and structural evidence, especially:

- `src/features/SwapOffhand.cpp`
- `src/features/OffhandHud.cpp`
- `src/features/OffhandTransactions.cpp`
- `src/offhand/Hands.cpp`
- `src/offhand/HandSwapScope.cpp`
- `data/packs/RP/ui/hud_screen.tsx`

### Old swap implementation

The original public implementation sends a custom `SwapOffhandPacket` and handles it on the server by directly exchanging the selected inventory slot with the offhand slot.

That design is **not** the target design here.

We specifically do not want to reproduce:

- `SwapOffhandPacket`
- a custom packet registration/handler
- server-side custom Offhand code
- `TRANSACTION_SLOT = -2`
- `HandSwapScope` as a mechanism for offhand use
- custom transaction remapping
- host capability/revision handshake
- client-side direct inventory mutation as the authoritative action
- `Item::mAllowOffhand = true` for every item
- full offhand use/swing/attack behavior

The old `OffhandItems.cpp` explicitly forces every item to allow offhand placement. That conflicts with the new requirement to respect the game's native item eligibility and allow custom behavior-pack item definitions to decide their own offhand support.

---

## 4. Supplied compiled Levi Offhand module: what it proves

The supplied `.levipack` contains:

- `offhand.so`
- modified resource pack under `resources/minecraft_resource_packs/offhand_android/`
- `resources/minecraft.ttf`
- icon/manifest

Relevant native strings in `offhand.so` include:

- `offhand.android.swap`
- `offhand.android.place`
- `external_button:offhand.android.swap`
- `external_button:offhand.android.place`
- `Swap button`
- `Interact offhand button`
- `Swap button opacity (%)`
- `F or Swap exchanges hands. Hold Interact to use the offhand. Requires Offhand on the host.`
- `The host needs Offhand to enable swapping and use.`
- `Waiting for the world host to acknowledge Offhand revision 2`

This proves the existing Android port had a separate Android button/overlay integration and a host handshake for the old full feature set.

For the simplified QoL module, these parts should be removed or replaced:

| Existing port feature | New design |
|---|---|
| Swap button | Keep, but register directly with PreLoader Mod Menu API |
| Interact/place button | Remove |
| Host handshake | Remove |
| Offhand revision advertisement | Remove |
| Custom Offhand packet | Remove |
| RP-driven button | Remove as soon as module button is working |

---

# 5. Protocol discovery — major finding

The target protocol infrastructure is already suitable for a normal inventory operation.

The current Bedrock protocol documentation for the matching protocol line defines `ItemStackRequestPacket` as packet **147**.

The request contains a list of `RequestData` objects. Each request contains:

- a client request ID;
- one or more ItemStackRequest actions;
- string filter data/origin.

The request ID is a typed client network ID and is constrained to valid negative odd values.

### Slot information

`ItemStackRequestCereal::SlotInfoData` contains:

1. `FullContainerName`
2. `Slot` (`uint8`)
3. `Net Id Variant` (`int32`)

`FullContainerName` contains:

- container enum (`uint8`)
- optional dynamic ID (`uint32`)

### Swap action

`ItemStackRequestCereal::SwapActionData` contains:

- action type = `2` (`Swap`)
- source `SlotInfoData`
- destination `SlotInfoData`

### Relevant container enum values in the current matching protocol docs

- `hotbarcontainer = 28`
- `inventorycontainer = 29`
- `offhandcontainer = 34`

### Important conclusion

The network protocol has a **first-class Swap action** and an **actual OffhandContainer**. Therefore the desired QoL operation does not inherently require a custom packet or server-side Offhand mod.

This is the central protocol direction to validate against the actual client UI behavior on 1.26.52.3.

### What this does NOT yet prove

Protocol documentation alone does not prove that Minecraft's mobile inventory UI uses exactly one `SwapAction` with:

`selected hotbar -> offhand`

or exactly how it handles the reverse/incompatible case.

That must still be reverse-engineered from the 1.26.52.3 client implementation.

---

# 6. Native target string evidence in 1.26.52.3

Strings extracted from the supplied target binary show native code for the exact mechanisms we need.

### Request/action system

Relevant strings include:

- `ItemStackRequestActionSwap`
- `ItemStackRequestActionTake`
- `ItemStackRequestActionPlace`
- `ItemStackRequestActionDrop`
- `ItemStackRequestActionDestroy`
- `ItemStackRequestActionConsume`
- `ItemStackRequestActionCreate`

This is strong evidence that the shipped client contains native implementations corresponding to the protocol actions.

### Request validation

Relevant strings include:

- `ItemStackRequestActionHandler::_validateRequestSlot - request server net id doesn't match existing item net id`
- `ItemStackRequestActionHandler::_validateRequestSlot - no existing item`
- `ItemStackRequestActionHandler::_validateRequestSlot - invalid existing item stack net id`
- `ItemStackRequestActionHandler::_validateRequestSlot - invalid request server net id`
- `ItemStackRequestActionHandler::_validateRequestSlot - no resolved id assignment`
- `ItemStackRequestActionHandler::_validateRequestSlot - resolve id slot doesn't match requested slot`
- `ItemStackRequestActionHandler::_cacheLegacySlotIdAssignment - ItemStackLegacyRequestId:`

This establishes that stack network IDs and slot/request mappings are not optional details. A synthesized request must contain the correct values.

### HUD/container evidence

Relevant strings include:

- `ContainerScreenSimulation::tryAutoPlace`
- `OffhandContainer`
- `OffhandContainerModel`
- `slot.weapon.offhand`
- `use_offhand`

`ContainerScreenSimulation::tryAutoPlace` is particularly interesting because it is a likely bridge between UI slot operations and inventory request generation.

### Item eligibility evidence

The target contains:

`The allow_off_hand component determines whether the item can be placed in the off hand slot of the inventory.`

This supports the design of reading the game's native item eligibility instead of maintaining an item whitelist or setting every item to true.

---

# 7. Exact string xrefs recovered from target binary

The target is stripped, but exact ADRP+ADD xrefs were recovered for several diagnostic strings.

| Evidence | String/file offset | Exact code xref |
|---|---:|---:|
| `tryAutoPlace` failure string | `0x237c267` | `0x1005a214` |
| `_validateRequestSlot` net-id mismatch | `0x255f234` | `0xf876cd4` |
| `_validateRequestSlot` no-existing-item | `0x25ad23d` | `0xf876d04` |
| `_validateRequestSlot` invalid existing net-id | `0x25ad284` | `0xf876d34` |
| `_cacheLegacySlotIdAssignment` text | `0x264002b` | `0xf8781d0` |
| `ServerPlayerInventoryTransactionSystem` text | `0x242cfae` | `0xba33ce4` |
| `use_offhand` query/text | `0x23ec144` | `0xfc839c4`, `0xfc86128`, `0x12638fdc` |

### Likely validation routine

A function beginning around `0xf876634` is strongly consistent with `ItemStackRequestActionHandler::_validateRequestSlot`, because its body contains the exact xrefs to multiple `_validateRequestSlot` error messages.

Observed behavior includes:

- copying the request slot data from the incoming structure;
- invoking an internal lookup/helper;
- checking resolved item/container state;
- checking network-ID consistency;
- producing the exact validation error strings when a condition fails.

This is **high-confidence functional identification**, but because the target is stripped it should still be labeled as a recovered/likely function until cross-validated through more call-graph analysis.

### `tryAutoPlace` string reference — entry point unconfirmed

The diagnostic string is referenced from `0x1005a214`. The earlier interpretation that a function begins at `0x1005a14c` was an inference based on proximity and is withdrawn. Recover the actual function start from control flow and a valid function boundary before tracing its callers/callees. Until that is done, this reference is a string xref only.

---

# 8. Important BedrockTools finding: current signatures do not directly match this target

Reference repository:

`https://github.com/QYCottage/BedrockTools`

BedrockTools already has useful abstractions/signature IDs for:

- `ContainerScreenControllerHandleAutoPlace`
- `ContainerScreenControllerGetItemStack`
- `ContainerScreenControllerOnContainerSlotSelected`
- `LoopbackPacketSenderSendToServer`
- `ClientInstanceGetPacketSender`
- `MinecraftPacketsCreatePacket`
- `ActorGetOffhandSlot`
- inventory access

However, the **current repository signature patterns must not be assumed to match Minecraft 1.26.52.3**.

An exact scan of the supplied 1.26.52.3 binary found **no exact match** for the current BedrockTools patterns for at least:

- `LocalPlayerSwing`
- `ContainerScreenControllerHandleAutoPlace`
- `ActorGetOffhandSlot`

Therefore:

> BedrockTools is the preferred architectural/reference layer, but its current signature database needs target-specific additions/updates for BuildID `56de9eed...` before depending on those functions.

Do not copy a current BedrockTools RVA into the mod merely because the symbol/function has the same conceptual name.

The old project assumption that `LocalPlayerSwing` could be used at RVA `0x0aaca7ac` is also rejected for this exact target; disassembly at that address does not match the current BedrockTools `LocalPlayerSwing` signature/function shape.

---

# 9. BedrockTools inventory structures already useful

Current BedrockTools inventory code uses runtime offsets for its supported target set around:

- `PlayerInventory`
- `PlayerInventoryContainer`
- `FillingContainerItems`
- `ItemStackSize`
- `ActorGetOffhandSlot`

Its documented current offsets include:

```text
PlayerInventory           = 0x570
PlayerInventoryContainer  = 0xB8
FillingContainerItems     = 0x140
ItemStackSize             = 0x98
ItemStackCount            = 0x22
ItemStackValid            = 0x23
ItemStackItemCounter      = 0x8
```

These are useful reference points, but they should be independently verified against the supplied 1.26.52.3 target before treating them as authoritative for this mod.

---

# 10. PreLoader / LeviLaunchroid button migration

Reference repository:

`https://github.com/LiteLDev/preloader-android`

The current public Mod Menu API provides:

```cpp
pl::modmenu::registerModule(...)
pl::modmenu::registerButton(...)
pl::modmenu::unregisterButton(...)
```

`ButtonInfo` supports:

- button ID
- module ID
- label
- Android key code
- click/hold/toggle behavior
- default visibility
- style
- icon data
- event callback

The current API also stores `default_visible`, but the public interface inspected does not expose a per-button runtime visibility setter.

That is **not a problem for the current design**, because the button is intentionally always visible.

### New UI arrangement

```text
OffhandQoL module
        |
        +-- registerButton("offhand.swap", defaultVisible=true)
        |
        +-- Click -> tryOffhandOperation()
        |
        +-- always-visible button
```

The button callback checks the current inventory state and simply returns when no valid operation exists.

The old `external_button:*` RP/overlay mechanism should therefore not be necessary for the final Android button.

---

# 11. Initial RP strategy

The user requirement for the first implementation pass is to **reuse the current Offhand RP**, with only targeted modifications.

Current RP source/reference:

`data/packs/RP/ui/hud_screen.tsx`

The current offhand slot uses a visibility binding based on:

```text
(#item_id_aux > -1)
```

This must be removed.

### Required initial RP changes

1. Keep the offhand slot control.
2. Make the slot always visible.
3. Keep the native item icon binding.
4. Keep stack-count/durability/storage bindings if they continue to work.
5. Keep the offhand slot artwork (`textures/ui/offhand_slot.png`).
6. Remove any RP or overlay feature that creates an Interact/Use button.
7. Do not add any dynamic Swap-button visibility logic.
8. Keep layout close to the current slot position but adjust the hotbar/offhand spacing as needed.

### Animation/resource-pack policy

For the initial build, do **not** prematurely remove every animation/controller file.

Reuse the existing RP first so that HUD rendering is validated independently from the protocol rewrite.

Later cleanup can remove files related solely to:

- offhand attacking
- offhand swinging
- offhand use progress
- crossbow/offhand use state
- spyglass/attachable behavior

if those pieces are proven unnecessary for the final swap-only feature.

---

# 12. Native HUD plan

The original Offhand HUD code proved that an `OffhandContainerModel` can be inserted into the HUD container-manager path.

Relevant original mechanism:

```text
HudContainerManagerModel::_postInit
        -> create/add OffhandContainerModel
```

For the first version, this concept should be preserved while the RP provides the actual visual slot/control.

The HUD should be driven by the real Minecraft offhand container so that the RP's item renderer can continue displaying:

- item icon
- stack count
- durability
- storage bar where applicable

The slot itself must remain visible when empty.

This avoids implementing an independent item renderer in the first iteration.

---

# 13. Main unresolved RE target: exact vanilla request generation

This is the highest-priority remaining task.

We need to recover the actual sequence used by the Minecraft client when the normal inventory UI moves an item between the selected hotbar slot and offhand.

## Required data

For each valid swap/move operation, recover:

1. `ItemStackRequest` packet creation path.
2. Request ID allocation/generation.
3. Action vector/list construction.
4. Exact `SwapActionData` source/destination values.
5. Exact `FullContainerName` values.
6. Exact hotbar/inventory/offhand slot numbers.
7. Exact source/destination stack network IDs.
8. Any `Take + Place` sequence that the UI uses instead of a single `Swap`.
9. Request send path.
10. Client handling of the server response.
11. How request state is invalidated/reconciled after rejection.

---

# 14. Best native tracing route

The strongest target discovered so far is:

```text
ContainerScreenSimulation::tryAutoPlace
```

Previous, withdrawn start-address guess (do not use):

```text
0x1005a14c  [WITHDRAWN]
```

Exact diagnostic string reference:

```text
0x1005a214
```

### Proposed call-graph tracing

```text
ContainerScreenSimulation::tryAutoPlace
            |
            +--> request/action builder
            |
            +--> SlotInfoData construction
            |
            +--> stack net-id lookup
            |
            +--> request ID generation
            |
            +--> ItemStackRequestPacket creation
            |
            +--> packet sender
```

The next RE pass should identify those callees by following calls and data structures from `tryAutoPlace`.

---

# 15. Second tracing route: ItemStackRequest action handler

The second major route is the request validator/dispatcher.

Known likely region:

```text
0xf876634    -> strongly consistent with _validateRequestSlot
```

Nearby code should be used to recover:

- SlotInfoData layout in native memory.
- how `Net Id Variant` is interpreted;
- container lookup;
- resolved slot assignment;
- client/server net-ID verification.

A likely action-dispatch region was also identified around:

```text
0xf875ec0
```

where code compares action-type values across the request action enum range. This is consistent with an `ItemStackRequestActionHandler` dispatcher and should be traced further.

---

# 16. Third tracing route: request packet creation/sending

Target functions/objects to recover:

- `MinecraftPacketsCreatePacket`
- `ItemStackRequestPacket`
- `LoopbackPacketSenderSendToServer`
- `ClientInstanceGetPacketSender`

The objective is to discover whether the mod can safely call an existing packet constructor/factory and then submit the request using the same sender path as vanilla UI code.

Prefer reusing Minecraft's own constructors/builders over manually serializing packet bytes.

---

# 17. Request ID and network-ID requirements

The protocol docs confirm that request IDs are validated and slot information includes stack network ID variants.

Therefore the final implementation should not use:

```text
fixed request ID
fixed net ID
zero net ID for non-empty stack
```

Instead it must source these values exactly as the vanilla client does.

### Required RE

Find:

```text
client request ID generator
stack server net ID / net ID variant accessor
legacy slot-ID assignment path
```

The presence of:

`ItemStackRequestActionHandler::_cacheLegacySlotIdAssignment`

is important because the client/server inventory system maintains mappings between legacy request state and newer request state.

---

# 18. Why a direct memory swap is rejected

Do **not** implement:

```cpp
mainhand = offhand;
offhand = oldMainhand;
```

as the actual operation.

That would modify the client representation without producing the same authoritative inventory request/validation path used by the server.

Consequences can include:

- server correction
- rollback
- desynchronization
- stale stack network IDs
- incorrect inventory state on remote servers

The local world should also use the normal request path rather than getting a special direct-memory code path.

---

# 19. Local world and remote server requirement

The final architecture is explicitly:

```text
button
  -> same SwapEngine
     -> same ItemStackRequest path
        -> integrated server OR remote server
```

There is no:

```cpp
if (localWorld) { special implementation; }
```

The integrated server must see the same legitimate inventory request that a remote server sees.

This is the intended meaning of **QoL-only** in this project.

Actual compatibility still has to be demonstrated experimentally against real servers; it should not be claimed merely from protocol documentation.

---

# 20. Custom behavior-pack item handling

The mod must not hardcode:

- totem
- shield
- map
- arrow
- fireworks
- any other vanilla ID

The eligibility test must instead use the game's actual item definition / native allow-offhand state.

Desired behavior:

```text
custom BP item with native allow-offhand = true
        -> treated as compatible

custom BP item with native allow-offhand = false
        -> treated as incompatible
```

No mod-side registry patch should modify this property.

The target binary's `allow_off_hand component` text is supporting evidence that this is a real item-definition concept in the target build.

---

# 21. Required target-specific native data to recover

Before implementation is considered complete, recover/verify on BuildID `56de9eed...`:

### Inventory

```text
Player inventory object
selected slot
inventory container
item stack array
item stack size
```

### Offhand

```text
Actor/Player offhand accessor
OffhandContainer backing storage
offhand slot index
```

### Item

```text
Item pointer
native allow-offhand field/accessor
```

### Network

```text
ItemStackRequest construction
Request ID generator
SlotInfoData native layout
Stack net-ID access
Packet sender
request-response handling
```

### UI/HUD

```text
OffhandContainerModel creation/usage
HudContainerManagerModel integration
minimum bind lifecycle requirements
```

---

# 22. No-go list for implementation

The following should **not** appear in the simplified module unless later RE proves an unrelated runtime dependency that forces a replacement:

```text
SwapOffhandPacket
custom packet registration
custom host handshake
Offhand revision handshake
TRANSACTION_SLOT = -2
HandSwapScope for gameplay use
OffhandUseSync
OffhandSwing
ShieldBlocking
OffhandMobs
mAllowOffhand = true patch
custom item whitelist
local-world special code path
direct client-authoritative inventory mutation
RP-created Android button
RP dynamic Swap visibility
Interact/Use button
```

---

# 23. Logging plan

Use Android logcat with stable tags so the mod can be filtered easily.

Recommended tags:

```text
OffhandRE
OffhandInv
OffhandHUD
OffhandButton
OffhandState
```

Important diagnostics:

```text
build ID detected
signature resolved / unresolved
selected slot
mainhand empty/item/allow-offhand
offhand empty/item
selected operation
request ID
source container/slot/net ID
destination container/slot/net ID
action count/type
packet creation result
packet send result
server/request response
HUD initialized
button callback entered/no-op reason
```

Do not log full item NBT or unrelated game traffic in normal operation.

---

# 24. Proposed implementation architecture after RE

```text
src/offhand/
    OffhandModule.cpp/.hpp
    OffhandState.cpp/.hpp
    OffhandEligibility.cpp/.hpp
    OffhandOperation.cpp/.hpp
    OffhandInventory.cpp/.hpp
    OffhandButton.cpp/.hpp
    OffhandHud.cpp/.hpp

src/re/
    Target_1_26_52_3.cpp/.hpp
    target signatures / native adapters
```

Logical layers:

```text
OffhandModule
    |
    +-- OffhandState
    |      +-- selected hotbar slot
    |      +-- selected ItemStack
    |      +-- offhand ItemStack
    |
    +-- OffhandEligibility
    |      +-- native allow-offhand
    |
    +-- OffhandOperation
    |      +-- MoveMainToOffhand
    |      +-- Swap
    |      +-- MoveOffhandToMain
    |      +-- None
    |
    +-- OffhandInventory
    |      +-- build vanilla ItemStackRequest
    |      +-- send through normal packet path
    |
    +-- OffhandButton
    |      +-- always-visible PreLoader button
    |
    +-- OffhandHud
           +-- reused RP-backed OffhandContainerModel
```

---

# 25. Immediate RE task queue

## Priority 1 — `tryAutoPlace`

Use the xref at `0x1005a214` as a starting point, recover the true containing function boundary, then identify the code that builds the inventory request. Do not treat `0x1005a14c` as a function entry.

Desired output:

```text
UI auto-place input
-> action type
-> source slot
-> destination slot
-> source net ID
-> destination net ID
-> request ID
-> packet object
```

## Priority 2 — SwapAction

Find actual native construction/use of `ItemStackRequestActionSwap` in the client, preferably from a real UI code path.

Verify whether swapping hotbar/offhand uses:

```text
one SwapAction
```

or a sequence of Take/Place actions.

## Priority 3 — request IDs

Recover request ID allocation and constraints on 1.26.52.3.

## Priority 4 — slot/net-ID accessor

Recover the exact target-build accessors for stack network IDs and offhand slot state.

## Priority 5 — target signatures

Add target-specific signatures/adapters to the BedrockTools-based layer.

## Priority 6 — live validation

Build a tiny diagnostic prototype that only reports the structures generated when manually moving/switching inventory items. Do not send synthesized requests until the format is known.

## Priority 7 — implementation

Only after the request format is confirmed, implement the final `SwapEngine`.

---

# 26. Current confidence summary

| Finding | Confidence |
|---|---|
| Supplied target is Minecraft 1.26.52.3 | **High** |
| BuildID is `56de9eed...` | **High** |
| Protocol has ItemStackRequest packet 147 | **High** |
| Protocol has SwapAction | **High** |
| Protocol has OffhandContainer | **High** |
| Target binary has native ItemStackRequestActionSwap | **High** |
| Target binary has OffhandContainer-related code | **High** |
| Target binary has allow_off_hand component | **High** |
| Target binary has request-slot/net-ID validation | **High** |
| `0x1005a214` is a reference to the `tryAutoPlace` diagnostic string | **High** |
| `0x1005a14c` is the `tryAutoPlace` function entry | **Withdrawn / unverified** |
| `0xf876634` is `_validateRequestSlot` | **High / still functionally cross-check** |
| Current BedrockTools signatures directly match this target | **No — exact scans found mismatches** |
| One vanilla SwapAction definitely represents hotbar<->offhand UI operation | **Not yet proven** |
| Remote vanilla server will definitely accept our synthesized request | **Not yet proven** |
| Local and remote can use one request path | **Design target; requires testing** |
| PreLoader can host the always-visible Swap button | **High** |
| Dynamic button visibility is necessary | **No** |
| RP can initially be reused for the offhand slot | **High** |

---

# 27. Reference sources

### Primary project references

- Offhand original source: https://github.com/FrederoxDev/Offhand
- BedrockTools: https://github.com/QYCottage/BedrockTools
- LeviLaunchroid Android template: supplied project file `levilauncher-android-mod-template-main.tar.br`
- PreLoader Android: https://github.com/LiteLDev/preloader-android
- LeviLauncher developer references: https://levilaunchroid.levimc.org/ and https://levilaunchroid.levimc.org/guide/developer

### Protocol reference used for the packet schema investigation

- EndstoneMC protocol docs, matching 1.26.52 / protocol line: https://github.com/EndstoneMC/protocol-docs

Relevant recovered protocol files:

```text
packets/ItemStackRequestPacket.json
types/ItemStackRequestPacketData__RequestData.json
types/ItemStackRequestCereal__RequestData.json
types/ItemStackRequestCereal__SlotInfoData.json
types/ItemStackRequestCereal__SwapActionData.json
types/FullContainerName.json
enums/ContainerEnumName.json
enums/ItemStackRequestActionType.json
```

### Item eligibility documentation

- `minecraft:allow_off_hand`: https://learn.microsoft.com/en-us/minecraft/creator/reference/content/itemreference/examples/itemcomponents/minecraft_allow_off_hand?view=minecraft-bedrock-stable

The documentation is secondary to the target binary/runtime evidence for implementation decisions.

---

# 28. Bottom line

The reverse-engineering evidence strongly supports the new design:

```text
No custom Offhand protocol
No host handshake
No local special case
No item whitelist

          ↓

Use Minecraft's existing ItemStackRequest inventory system
          +
Native allow-offhand state
          +
Reused RP OffhandContainer HUD
          +
Always-visible PreLoader Swap button
```

The **single biggest remaining unknown** is not whether the protocol can represent the operation—it clearly can—but the exact 1.26.52.3 client-side construction of that request, especially the stack network IDs and whether the vanilla UI uses one `SwapAction` or a multi-action sequence.

That is the next RE target before writing the actual SwapEngine.
