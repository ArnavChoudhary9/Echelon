-- ============================================================
-- Echelon Workspace
-- ============================================================

include "Dependencies.lua"

workspace "Echelon"
    architecture "x64"
    startproject "EchelonEditor"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"

    configurations { "Debug", "Release" }

-- Shared output directory token used by all projects
outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- ---- Workspace-wide build settings ----
filter "system:windows"
    systemversion "latest"
    defines { "ECHELON_PLATFORM_WINDOWS" }

filter "configurations:Debug"
    defines { "ECHELON_DEBUG" }
    runtime "Debug"
    symbols "on"
    buildoptions { "-Wall", "-Wextra", "-Wpedantic", "-g" }

filter "configurations:Release"
    defines { "ECHELON_RELEASE" }
    runtime "Release"
    optimize "on"
    buildoptions { "-Wall", "-Wextra", "-Wpedantic", "-O2" }

filter {}

-- ---- Sub-projects ----
group "Vendor"
    include "Vendor"
group ""

group "Engine"
    include "Echelon"
group ""

group "App"
    include "EchelonEditor"
group ""

-- ============================================================
-- Clean action  (premake5 clean)
-- ============================================================
newaction {
    trigger     = "clean",
    description = "Remove all generated build files",
    execute     = function()
        print("Cleaning build artifacts...")
        os.rmdir("./bin")
        os.rmdir("./bin-int")
        os.rmdir("./Makefile")
        os.remove("./Echelon/Makefile")
        os.remove("./EchelonEditor/Makefile")
        os.remove("./Vendor/Makefile")
        print("Done.")
    end
}
