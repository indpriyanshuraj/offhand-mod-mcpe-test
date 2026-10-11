#include "offhand/RequestTrace.hpp"

#include "offhand/Log.hpp"
#include <pl/memory/Hook.hpp>

#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace offhand_qol {
namespace {

// Target RVAs belong only to ExpectedBuildId in RuntimeProbe.hpp.
constexpr std::uintptr_t kAddRequestActionRva = 0x10041D9C;
constexpr std::size_t kSwapActionSize = 0x60;
constexpr std::size_t kActionTypeOffset = 0x08;
constexpr std::size_t kSourceSlotOffset = 0x10;
constexpr std::size_t kDestinationSlotOffset = 0x38;
constexpr std::size_t kSlotInfoSize = 0x28;
constexpr std::size_t kSlotHeaderSize = 0x0D;
constexpr std::uint8_t kSwapActionType = 2;
static_assert(kDestinationSlotOffset + kSlotInfoSize == kSwapActionSize);

using AddRequestActionFn = void (*)(void* manager, void* uniqueActionHolder);
void* gOriginalRaw{};

std::string hexBytes(const std::uint8_t* bytes, std::size_t count) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < count; ++i) {
        if (i != 0) output << ' ';
        output << std::setw(2) << static_cast<unsigned int>(bytes[i]);
    }
    return output.str();
}

void traceAddRequestAction(void* manager, void* uniqueActionHolder) {
    auto original = reinterpret_cast<AddRequestActionFn>(gOriginalRaw);
    if (!original) {
        // The hook library initializes the trampoline before publishing the
        // detour. This branch is defensive; it fails closed rather than
        // consuming ownership of the action.
        return;
    }

    if (uniqueActionHolder) {
        auto* action = *static_cast<void**>(uniqueActionHolder);
        if (action) {
            const auto* bytes = static_cast<const std::uint8_t*>(action);
            if (bytes[kActionTypeOffset] == kSwapActionType) {
                // This action type's Android 1.26.52.3 constructor allocates
                // 0x60 bytes. Each SlotInfo is 0x28 bytes: 13-byte header,
                // padding, and a 24-byte discriminated network-ID variant.
                const auto* source = bytes + kSourceSlotOffset;
                const auto* destination = bytes + kDestinationSlotOffset;
                const std::string sourceHeader = hexBytes(source, kSlotHeaderSize);
                const std::string sourceNetId = hexBytes(source + 0x10, kSlotInfoSize - 0x10);
                const std::string destinationHeader = hexBytes(destination, kSlotHeaderSize);
                const std::string destinationNetId = hexBytes(destination + 0x10, kSlotInfoSize - 0x10);
                log::info(
                    "Vanilla Swap action observed (read-only): src_header=[%s] src_netid_variant=[%s] dst_header=[%s] dst_netid_variant=[%s]",
                    sourceHeader.c_str(), sourceNetId.c_str(),
                    destinationHeader.c_str(), destinationNetId.c_str());
            }
        }
    }

    // Preserve native ownership and request behavior exactly.
    original(manager, uniqueActionHolder);
}

} // namespace

RequestTrace::~RequestTrace() {
    stop();
}

bool RequestTrace::start(std::uintptr_t moduleBase) {
    std::lock_guard lock(mMutex);
    if (mInstalled) return true;
    if (moduleBase == 0) {
        log::warn("Request trace not installed: Minecraft module base is unavailable");
        return false;
    }

    const auto target = moduleBase + kAddRequestActionRva;
    gOriginalRaw = nullptr;
    const int result = pl::memory::hook(
        reinterpret_cast<void*>(target),
        reinterpret_cast<void*>(&traceAddRequestAction),
        &gOriginalRaw,
        pl::memory::HookPriority::Low);

    if (result != 0 || gOriginalRaw == nullptr) {
        if (result == 0) {
            pl::memory::unhook(reinterpret_cast<void*>(target), reinterpret_cast<void*>(&traceAddRequestAction));
        }
        gOriginalRaw = nullptr;
        log::warn("Read-only request trace hook failed at RVA 0x10041D9C (status %d)", result);
        return false;
    }

    mTarget = target;
    mInstalled = true;
    log::info("Read-only vanilla Swap-action trace installed for verified Android build");
    return true;
}

void RequestTrace::stop() {
    std::lock_guard lock(mMutex);
    if (mInstalled) {
        pl::memory::unhook(reinterpret_cast<void*>(mTarget), reinterpret_cast<void*>(&traceAddRequestAction));
        mInstalled = false;
        mTarget = 0;
    }
}

} // namespace offhand_qol
