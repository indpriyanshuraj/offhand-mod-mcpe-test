#pragma once

#include <cstdint>
#include <mutex>

namespace offhand_qol {

// Installs a read-only, BuildID-gated trace of vanilla Swap actions submitted
// to ItemStackNetManagerClient. It forwards every call unchanged and never
// creates actions or modifies inventory state.
class RequestTrace {
public:
    RequestTrace() = default;
    RequestTrace(const RequestTrace&) = delete;
    RequestTrace& operator=(const RequestTrace&) = delete;
    ~RequestTrace();

    [[nodiscard]] bool start(std::uintptr_t moduleBase);
    void stop();

private:
    std::mutex mMutex;
    std::uintptr_t mTarget{};
    bool mInstalled{};
};

} // namespace offhand_qol
