#include "offhand/RequestTrace.hpp"

#include <pl/Logger.hpp>
#include <pl/memory/Hook.hpp>

#include <atomic>
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
std::atomic<pl::log::Logger*> gLogger{nullptr};

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
                if (auto* logger = gLogger.load(std::memory_order_acquire)) {
                    logger->info(
                        "Vanilla Swap action observed (read-only): src_header=[{}] src_netid_variant=[{}] dst_header=[{}] dst_netid_variant=[{}]",
                        hexBytes(source, kSlotHeaderSize),
                        hexBytes(source + 0x10, kSlotInfoSize - 0x10),
                        hexBytes(destination, kSlotHeaderSize),
                        hexBytes(destination + 0x10, kSlotInfoSize - 0x10));
                }
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

bool RequestTrace::start(std::uintptr_t moduleBase, pl::log::Logger& logger) {
    std::lock_guard lock(mMutex);
    if (mInstalled) return true;
    if (moduleBase == 0) {
        logger.warn("Request trace not installed: Minecraft module base is unavailable");
        return false;
    }

    const auto target = moduleBase + kAddRequestActionRva;
    gLogger.store(&logger, std::memory_order_release);
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
        gLogger.store(nullptr, std::memory_order_release);
        logger.warn("Read-only request trace hook failed at RVA 0x10041D9C (status {})", result);
        return false;
    }

    mTarget = target;
    mInstalled = true;
    logger.info("Read-only vanilla Swap-action trace installed for the verified Android build");
    return true;
}

void RequestTrace::stop() {
    std::lock_guard lock(mMutex);
    if (mInstalled) {
        pl::memory::unhook(reinterpret_cast<void*>(mTarget), reinterpret_cast<void*>(&traceAddRequestAction));
        mInstalled = false;
        mTarget = 0;
    }
    gLogger.store(nullptr, std::memory_order_release);
}

} // namespace offhand_qol
