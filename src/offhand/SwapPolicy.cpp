#include "offhand/SwapPolicy.hpp"

namespace offhand_qol {

SwapOperation chooseSwapOperation(const SwapSnapshot& s) noexcept {
    if (s.mainhandEmpty && s.offhandEmpty) {
        return SwapOperation::NoOp;
    }

    if (s.mainhandEmpty) {
        return SwapOperation::MoveOffhandToMainhand;
    }

    if (s.mainhandAllowsOffhand) {
        return s.offhandEmpty ? SwapOperation::MoveMainhandToOffhand
                              : SwapOperation::SwapHands;
    }

    return s.offhandEmpty ? SwapOperation::NoOp
                          : SwapOperation::MoveOffhandToMainhand;
}

} // namespace offhand_qol