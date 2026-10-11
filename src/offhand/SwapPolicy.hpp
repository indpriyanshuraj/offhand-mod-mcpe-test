#pragma once

namespace offhand_qol {

enum class SwapOperation {
    NoOp,
    MoveMainhandToOffhand,
    SwapHands,
    MoveOffhandToMainhand,
};

struct SwapSnapshot {
    bool mainhandEmpty{};
    bool offhandEmpty{};
    bool mainhandAllowsOffhand{};
};

// Pure operation policy. It does not mutate inventory and can be tested
// without Minecraft or a PreLoader runtime.
[[nodiscard]] SwapOperation chooseSwapOperation(const SwapSnapshot& snapshot) noexcept;

} // namespace offhand_qol