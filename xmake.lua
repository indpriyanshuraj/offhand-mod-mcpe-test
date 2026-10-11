-- Offhand QoL: Android-native LeviLaunchroid mod.
-- Build conventions follow QYCottage/BedrockTools.

add_rules("mode.debug", "mode.release")
set_languages("c++20")
set_policy("package.requires_lock", true)

-- Keep both local builds and CI incremental. Xmake's console theme handles
-- coloured progress; scripts/build-android.sh forces a colour-capable theme.
set_policy("build.ccache", true)
if os.getenv("XMAKE_COLORTERM") == nil then
    os.setenv("XMAKE_COLORTERM", "truecolor")
end
if os.getenv("XMAKE_THEME") == nil then
    os.setenv("XMAKE_THEME", "default")
end

package("preloader")
    set_homepage("https://github.com/LiteLDev/preloader-android")
    set_description("LeviLauncher Android native-mod preloader SDK")
    add_urls("https://github.com/LiteLDev/preloader-android.git")
    add_versions("main", "main")
    add_deps("cmake")
    on_install("android", function (package)
        import("package.tools.cmake").install(package)
    end)
package_end()

add_requires("preloader")

local mod_name = "OffhandQoL"
local mod_version = "0.1.0"

-- `print()` is stable across xmake releases; xmake's own coloured progress
-- is controlled by XMAKE_COLORTERM/XMAKE_THEME in the local script and CI.
print("[OffhandQoL] configuring Android-native build")

target(mod_name)
    set_kind("shared")
    set_version(mod_version)
    set_languages("c++20")
    set_warnings("all", "extra")
    set_strip("all")
    add_files("src/main.cpp", "src/offhand/*.cpp")
    add_includedirs("include", "src")
    add_packages("preloader")

    if is_plat("android") then
        add_defines("OFFHAND_QOL_ANDROID=1")
        add_cxflags(
            "-fPIC",
            "-Oz",
            "-ffunction-sections",
            "-fdata-sections",
            "-flto",
            "-fno-unwind-tables",
            "-fno-asynchronous-unwind-tables",
            "-fmerge-all-constants",
            "-fexceptions",
            "-fvisibility=hidden"
        )
        add_cxxflags("-fno-rtti", "-fvisibility-inlines-hidden")
        add_shflags(
            "-Wl,--gc-sections",
            "-Wl,--icf=all",
            "-flto",
            "-Wl,--hash-style=gnu",
            "-Wl,-z,max-page-size=16384"
        )
        add_links("android", "log", "dl")
    end

    after_build(function (target)
        if not target:is_plat("android") then
            return
        end

        import("lib.detect.find_tool")
        local python = find_tool("python3") or find_tool("python")
        assert(python, "Python 3 is required to package OffhandQoL.levipack")

        local args = {
            path.join(os.projectdir(), "scripts", "package_levipack.py"),
            "--library", target:targetfile(),
            "--icon", path.join(os.projectdir(), "assets", "icon.png"),
            "--font", path.join(os.projectdir(), "resources", "minecraft.ttf"),
            "--resource-pack", path.join(os.projectdir(), "resources", "minecraft_resource_packs", "offhand_android"),
            "--name", "Offhand QoL",
            "--author", "Levi community",
            "--description", "Offhand HUD slot and vanilla inventory swap shortcut.",
            "--version", mod_version,
            "--output", path.join(target:targetdir(), "OffhandQoL.levipack")
        }

        print("[OffhandQoL] packaging .levipack")
        os.vrunv(python.program, args)
    end)