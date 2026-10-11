#pragma once

#include "offhand/RuntimeProbe.hpp"

#include <pl/Mod.hpp>

#include <atomic>
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
    RuntimeProbe mRuntime;
};

} // namespace offhand_qol