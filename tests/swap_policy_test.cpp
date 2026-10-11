#include "offhand/SwapPolicy.hpp"

#include <cassert>

using offhand_qol::SwapOperation;
using offhand_qol::SwapSnapshot;
using offhand_qol::chooseSwapOperation;

int main() {
    assert(chooseSwapOperation({true, true, false}) == SwapOperation::NoOp);
    assert(chooseSwapOperation({true, false, false}) == SwapOperation::MoveOffhandToMainhand);
    assert(chooseSwapOperation({false, true, true}) == SwapOperation::MoveMainhandToOffhand);
    assert(chooseSwapOperation({false, false, true}) == SwapOperation::SwapHands);
    assert(chooseSwapOperation({false, true, false}) == SwapOperation::NoOp);
    assert(chooseSwapOperation({false, false, false}) == SwapOperation::MoveOffhandToMainhand);
    return 0;
}