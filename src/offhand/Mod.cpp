#include "offhand/Mod.hpp"

#include "offhand/Log.hpp"
#include "offhand/SwapPolicy.hpp"

#include <pl/ModMenu.hpp>

#include <string>
#include <string_view>

namespace offhand_qol {
namespace {

constexpr std::string_view kModuleId = "offhandqol.module";
constexpr std::string_view kSwapButtonId = "offhandqol.swap";

} // namespace

bool OffhandMod::load(pl::mod::ModContext& context) {
    mModId = context.id();
    log::info("Mod load entered; id=%s", mModId.c_str());
    log::info("Target BuildID gate: %s", RuntimeProbe::ExpectedBuildId);
    log::info("Startup uses poll-and-settle; no dlopen hook is installed");
    return true;
}

bool OffhandMod::enable(pl::mod::ModContext& context) {
    mModId = context.id();
    log::info("Mod enable entered; registering Mod Menu controls");
    mLifecycleEnabled.store(true, std::memory_order_release);
    mModuleEnabled.store(true, std::memory_order_release);

    pl::modmenu::ModuleBuilder module(std::string(kModuleId), "Offhand QoL");
    module
        .modId(mModId)
        .description("Adds an offhand HUD slot and a vanilla inventory swap shortcut.")
        .defaultEnabled(true)
        .onToggle([this](std::string_view, bool enabled) { onModuleToggle(enabled); });

    if (!module.registerModule()) {
        log::error("Failed to register Offhand QoL module in Levi Mod Menu");
        mLifecycleEnabled.store(false, std::memory_order_release);
        mModuleEnabled.store(false, std::memory_order_release);
        return false;
    }

    pl::modmenu::ButtonBuilder button(std::string(kSwapButtonId), "Offhand Swap");
    button
        .moduleId(std::string(kModuleId))
        .modId(mModId)
        .label("Swap")
        .behavior(pl::modmenu::ButtonBehavior::Click)
        .defaultVisible(true)
        .stylePreset(pl::modmenu::ButtonStylePreset::Accent)
        .sizeScale(1.0f, 1.0f)
        .onEvent([this](std::string_view, pl::modmenu::ButtonEvent event, float) {
            if (event == pl::modmenu::ButtonEvent::Click) {
                requestSwap();
            }
        });

    if (!button.registerButton()) {
        log::error("Failed to register Swap button; unregistering module");
        pl::modmenu::unregisterModule(kModuleId);
        mLifecycleEnabled.store(false, std::memory_order_release);
        mModuleEnabled.store(false, std::memory_order_release);
        return false;
    }

    log::info("Registered always-visible Swap button; no interact/use button is registered");
    mRuntime.start([this](std::uintptr_t moduleBase) {
        std::lock_guard lock(mRequestTraceMutex);
        if (moduleBase != 0 &&
            mLifecycleEnabled.load(std::memory_order_acquire) &&
            mModuleEnabled.load(std::memory_order_acquire)) {
            (void)mRequestTrace.start(moduleBase);
        }
    });
    return true;
}

bool OffhandMod::disable(pl::mod::ModContext&) {
    mLifecycleEnabled.store(false, std::memory_order_release);
    mModuleEnabled.store(false, std::memory_order_release);
    mRuntime.stop();
    {
        std::lock_guard lock(mRequestTraceMutex);
        mRequestTrace.stop();
    }
    pl::modmenu::unregisterButton(kSwapButtonId);
    pl::modmenu::unregisterModule(kModuleId);
    log::info("Mod disabled; request tracing removed and inventory remains untouched");
    return true;
}

bool OffhandMod::unload(pl::mod::ModContext&) {
    mLifecycleEnabled.store(false, std::memory_order_release);
    mModuleEnabled.store(false, std::memory_order_release);
    mRuntime.stop();
    {
        std::lock_guard lock(mRequestTraceMutex);
        mRequestTrace.stop();
    }
    pl::modmenu::unregisterButton(kSwapButtonId);
    pl::modmenu::unregisterModule(kModuleId);
    log::info("Mod unloaded");
    return true;
}

void OffhandMod::onModuleToggle(bool enabled) {
    std::lock_guard lock(mRequestTraceMutex);
    mModuleEnabled.store(enabled, std::memory_order_release);
    if (enabled && mLifecycleEnabled.load(std::memory_order_acquire) &&
        mRuntime.targetSupported()) {
        (void)mRequestTrace.start(mRuntime.moduleBase());
    } else if (!enabled) {
        mRequestTrace.stop();
    }
    log::info("Mod Menu feature toggle changed: %s", enabled ? "enabled" : "disabled");
}

void OffhandMod::requestSwap() {
    if (!mLifecycleEnabled.load(std::memory_order_acquire) ||
        !mModuleEnabled.load(std::memory_order_acquire)) {
        return;
    }

    if (!mRuntime.targetSupported()) {
        log::warn("Swap ignored: target build has not been verified as supported");
        return;
    }

    // Deliberate fail-closed milestone: no inventory or packet state is mutated
    // until the ItemStackNetManagerClient request ABI has been verified against
    // the Android 1.26.52.3 binary. The button and HUD RP can be tested safely.
    log::warn("Swap press received, but native ItemStackRequest backend is not armed; inventory left untouched");
}

} // namespace offhand_qol