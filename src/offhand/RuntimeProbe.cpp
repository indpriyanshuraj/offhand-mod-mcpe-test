#include "offhand/RuntimeProbe.hpp"

#include <pl/Logger.hpp>

#include <link.h>
#include <elf.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>

namespace offhand_qol {
namespace {

constexpr const char* kGameLibrary = "libminecraftpe.so";

struct ProbeResult {
    bool mapped{};
    std::string buildId;
};

int findGameLibrary(dl_phdr_info* info, size_t, void* opaque) {
    if (!info || !info->dlpi_name ||
        std::strstr(info->dlpi_name, kGameLibrary) == nullptr) {
        return 0;
    }

    auto& result = *static_cast<ProbeResult*>(opaque);
    result.mapped = true;

    for (int i = 0; i < info->dlpi_phnum; ++i) {
        const ElfW(Phdr)& phdr = info->dlpi_phdr[i];
        if (phdr.p_type != PT_NOTE || phdr.p_memsz < 12) {
            continue;
        }

        const auto* p = reinterpret_cast<const unsigned char*>(info->dlpi_addr + phdr.p_vaddr);
        const auto* end = p + phdr.p_memsz;
        while (p + 12 <= end) {
            uint32_t nameSize{};
            uint32_t descSize{};
            uint32_t type{};
            std::memcpy(&nameSize, p, 4);
            std::memcpy(&descSize, p + 4, 4);
            std::memcpy(&type, p + 8, 4);
            p += 12;

            const size_t alignedName = (static_cast<size_t>(nameSize) + 3u) & ~size_t(3u);
            const size_t alignedDesc = (static_cast<size_t>(descSize) + 3u) & ~size_t(3u);
            if (p + alignedName + alignedDesc > end) {
                break;
            }

            if (type == NT_GNU_BUILD_ID && nameSize == 4 &&
                std::memcmp(p, "GNU", 4) == 0) {
                static constexpr char hex[] = "0123456789abcdef";
                result.buildId.clear();
                result.buildId.reserve(descSize * 2u);
                const unsigned char* bytes = p + alignedName;
                for (uint32_t n = 0; n < descSize; ++n) {
                    result.buildId.push_back(hex[bytes[n] >> 4]);
                    result.buildId.push_back(hex[bytes[n] & 0x0f]);
                }
                return 1;
            }
            p += alignedName + alignedDesc;
        }
    }
    return 1;
}

ProbeResult inspectGameLibrary() {
    ProbeResult result{};
    dl_iterate_phdr(findGameLibrary, &result);
    return result;
}

} // namespace

RuntimeProbe::~RuntimeProbe() {
    stop();
}

void RuntimeProbe::start(pl::log::Logger& logger) {
    if (mWatcher.joinable()) {
        return;
    }

    mLogger = &logger;
    mMapped.store(false, std::memory_order_release);
    mSupported.store(false, std::memory_order_release);
    mStop.store(false, std::memory_order_release);
    mWatcher = std::thread([this] {
        using namespace std::chrono_literals;
        constexpr int kPollAttempts = 1200; // bounded at roughly two minutes

        ProbeResult probe{};
        for (int attempt = 0; attempt < kPollAttempts && !mStop.load(std::memory_order_acquire); ++attempt) {
            probe = inspectGameLibrary();
            if (probe.mapped) {
                break;
            }
            std::this_thread::sleep_for(100ms);
        }

        if (mStop.load(std::memory_order_acquire)) {
            return;
        }
        if (!probe.mapped) {
            if (mLogger) mLogger->warn("Runtime probe timed out waiting for libminecraftpe.so; gameplay hooks remain disabled");
            return;
        }

        mMapped.store(true, std::memory_order_release);
        if (mLogger) mLogger->info("Minecraft library mapped; waiting 5 seconds for loader/game initialization to settle");
        for (int i = 0; i < 50 && !mStop.load(std::memory_order_acquire); ++i) {
            std::this_thread::sleep_for(100ms);
        }
        if (mStop.load(std::memory_order_acquire)) {
            return;
        }

        probe = inspectGameLibrary();
        mBuildId = probe.buildId;
        const bool supported = probe.mapped && mBuildId == ExpectedBuildId;
        mSupported.store(supported, std::memory_order_release);
        if (mLogger) {
            mLogger->info("Minecraft build ID: {}", mBuildId.empty() ? "unavailable" : mBuildId);
            if (supported) {
                mLogger->info("Target build verified. Inventory mutation/request backend is still fail-closed until its request ABI is verified.");
            } else {
                mLogger->warn("Unsupported Minecraft build; OffhandQoL will not issue inventory requests");
            }
        }
    });
}

void RuntimeProbe::stop() {
    mStop.store(true, std::memory_order_release);
    if (mWatcher.joinable()) {
        mWatcher.join();
    }
}

bool RuntimeProbe::targetMapped() const noexcept {
    return mMapped.load(std::memory_order_acquire);
}

bool RuntimeProbe::targetSupported() const noexcept {
    return mSupported.load(std::memory_order_acquire);
}

std::string RuntimeProbe::buildId() const {
    return mBuildId;
}

} // namespace offhand_qol