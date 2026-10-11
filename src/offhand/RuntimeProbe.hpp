#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

namespace pl::log { class Logger; }

namespace offhand_qol {

class RuntimeProbe {
public:
    RuntimeProbe() = default;
    RuntimeProbe(const RuntimeProbe&) = delete;
    RuntimeProbe& operator=(const RuntimeProbe&) = delete;
    ~RuntimeProbe();

    void start(pl::log::Logger& logger, std::function<void(std::uintptr_t)> onSupported = {});
    void stop();

    [[nodiscard]] bool targetMapped() const noexcept;
    [[nodiscard]] bool targetSupported() const noexcept;
    [[nodiscard]] std::string buildId() const;
    [[nodiscard]] std::uintptr_t moduleBase() const noexcept;

    static constexpr const char* ExpectedBuildId =
        "56de9eed077631e03a31f4f58eb2f0e00071d335";

private:
    std::atomic<bool> mStop{false};
    std::atomic<bool> mMapped{false};
    std::atomic<bool> mSupported{false};
    std::atomic<std::uintptr_t> mModuleBase{0};
    std::thread mWatcher;
    pl::log::Logger* mLogger{};
    std::string mBuildId;
};

} // namespace offhand_qol