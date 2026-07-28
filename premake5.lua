-- ============================================================
-- Echelon Workspace
-- ============================================================

include "Dependencies.lua"

-- ------------------------------------------------------------
-- Default renderer selection
-- ------------------------------------------------------------
-- The engine loads one renderer plugin by default. Which plugin is built,
-- shipped next to the editor, and auto-loaded at runtime is selected here.
-- Override on the command line, e.g.:
--     premake5 gmake2 --renderer=MyRenderer
-- A renderer must live in a top-level folder named after it, with its own
-- premake5.lua whose targetname == the folder name (see Ray/ for the template).
newoption {
    trigger     = "renderer",
    value       = "NAME",
    description = "Default renderer plugin to build + ship (folder & output lib base name)",
    default     = "Ray",
}
defaultRenderer = _OPTIONS["renderer"] or "Ray"

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

filter "system:linux"
    systemversion "latest"
    defines { "ECHELON_PLATFORM_LINUX" }

filter "system:macosx"
    systemversion "latest"
    defines { "ECHELON_PLATFORM_MACOS" }

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

group "Renderer"
    include(defaultRenderer)   -- only the selected renderer plugin is built
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
