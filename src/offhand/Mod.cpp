#include "offhand/Mod.hpp"

#include "offhand/SwapPolicy.hpp"

#include <pl/Logger.hpp>
#include <pl/ModMenu.hpp>

#include <string>
#include <string_view>

namespace offhand_qol {
namespace {

constexpr std::string_view kModuleId = "offhandqol.module";
constexpr std::string_view kSwapButtonId = "offhandqol.swap";

} // namespace

bool OffhandMod::load(pl::mod::ModContext& context) {
    mLogger = &pl::log::Logger::getOrCreate("OffhandQoL");
    mModId = context.id();
    mLogger->info("Offhand QoL loaded; client-only scope, no custom server packets");
    mLogger->info("Target build gate: {}", RuntimeProbe::ExpectedBuildId);
    mLogger->info("Startup path follows poll-and-settle; no dlopen hook is installed");
    return true;
}

bool OffhandMod::enable(pl::mod::ModContext& context) {
    mLogger = &pl::log::Logger::getOrCreate("OffhandQoL");
    mModId = context.id();
    mLifecycleEnabled.store(true, std::memory_order_release);
    mModuleEnabled.store(true, std::memory_order_release);

    pl::modmenu::ModuleBuilder module(std::string(kModuleId), "Offhand QoL");
    module
        .modId(mModId)
        .description("Adds an offhand HUD slot and a vanilla inventory swap shortcut.")
        .defaultEnabled(true)
        .onToggle([this](std::string_view, bool enabled) { onModuleToggle(enabled); });

    if (!module.registerModule()) {
        mLogger->error("Failed to register Offhand QoL module in Levi Mod Menu");
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
        mLogger->error("Failed to register Swap button; unregistering module");
        pl::modmenu::unregisterModule(kModuleId);
        mLifecycleEnabled.store(false, std::memory_order_release);
        mModuleEnabled.store(false, std::memory_order_release);
        return false;
    }

    mLogger->info("Registered single always-visible Swap button; no interact/use button is registered");
    mRuntime.start(*mLogger, [this](std::uintptr_t moduleBase) {
        std::lock_guard lock(mRequestTraceMutex);
        if (moduleBase != 0 && mLogger &&
            mLifecycleEnabled.load(std::memory_order_acquire) &&
            mModuleEnabled.load(std::memory_order_acquire)) {
            (void)mRequestTrace.start(moduleBase, *mLogger);
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
    if (mLogger) mLogger->info("Offhand QoL disabled; no hooks or inventory mutations remain active");
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
    if (mLogger) mLogger->info("Offhand QoL unloaded");
    mLogger = nullptr;
    return true;
}

void OffhandMod::onModuleToggle(bool enabled) {
    std::lock_guard lock(mRequestTraceMutex);
    mModuleEnabled.store(enabled, std::memory_order_release);
    if (enabled && mLifecycleEnabled.load(std::memory_order_acquire) &&
        mRuntime.targetSupported() && mLogger) {
        (void)mRequestTrace.start(mRuntime.moduleBase(), *mLogger);
    } else if (!enabled) {
        mRequestTrace.stop();
    }
    if (mLogger) mLogger->info("Offhand QoL module toggled: {}", enabled ? "enabled" : "disabled");
}

void OffhandMod::requestSwap() {
    if (!mLifecycleEnabled.load(std::memory_order_acquire) ||
        !mModuleEnabled.load(std::memory_order_acquire)) {
        return;
    }

    if (!mRuntime.targetSupported()) {
        if (mLogger) mLogger->warn("Swap ignored: target build has not been verified as supported");
        return;
    }

    // Deliberate fail-closed milestone: no inventory or packet state is mutated
    // until the ItemStackNetManagerClient request ABI has been verified against
    // the Android 1.26.52.3 binary. The button and HUD RP can be tested safely.
    if (mLogger) {
        mLogger->warn("Swap press received, but vanilla ItemStackRequest backend is not armed yet; inventory left untouched");
    }
}

} // namespace offhand_qol