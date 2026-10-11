#pragma once

#include "offhand/RuntimeProbe.hpp"
#include "offhand/RequestTrace.hpp"

#include <pl/Mod.hpp>

#include <atomic>
#include <mutex>
#include <string>

namespace pl::log { class Logger; }
namespace pl::mod { class ModContext; }

namespace offhand_qol {

class OffhandMod {
public:
    bool load(pl::mod::ModContext& context);
    bool enable(pl::mod::ModContext& context);
    bool disable(pl::mod::ModContext& context);
    bool unload(pl::mod::ModContext& context);

private:
    void onModuleToggle(bool enabled);
    void requestSwap();

    pl::log::Logger* mLogger{};
    std::string mModId;
    std::atomic<bool> mLifecycleEnabled{false};
    std::atomic<bool> mModuleEnabled{true};
    std::mutex mRequestTraceMutex;
    RequestTrace mRequestTrace;
    // Declared after RequestTrace so its watcher joins before the trace is destroyed.
    RuntimeProbe mRuntime;
};

} // namespace offhand_qol