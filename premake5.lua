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

-- ------------------------------------------------------------
-- Graphics backend selection
-- ------------------------------------------------------------
-- One or more backends can be compiled into the same engine binary.
-- Pass a comma-separated list; the FIRST entry becomes the runtime default.
-- Examples:
--     premake5 gmake2                                  → OpenGL only
--     premake5 gmake2 --graphics-backends=Vulkan       → Vulkan only
--     premake5 gmake2 --graphics-backends=OpenGL,Vulkan→ both compiled, OpenGL default
newoption {
    trigger     = "graphics-backends",
    value       = "LIST",
    description = "Comma-separated graphics backends to compile (first = runtime default)",
    default     = "OpenGL",
}

-- Parse comma-separated list into a Lua table; trim surrounding whitespace.
do
    local raw = _OPTIONS["graphics-backends"] or "OpenGL"
    graphicsBackends = {}
    for entry in raw:gmatch("[^,]+") do
        graphicsBackends[#graphicsBackends + 1] = entry:match("^%s*(.-)%s*$")
    end
end
-- Convenience scalar: first entry is the runtime default.
defaultGraphicsBackend = graphicsBackends[1]

-- ------------------------------------------------------------
-- Application project selection
-- ------------------------------------------------------------
-- EchelonEditor is the default app project.  Point --project at any top-level
-- folder that contains a premake5.lua to use it instead.  The folder name must
-- match the project() name declared inside that file (same convention as the
-- existing sub-projects).  Example:
--     premake5 gmake2 --project=MyGame
newoption {
    trigger     = "project",
    value       = "NAME",
    description = "Application project folder to compile (must contain a premake5.lua)",
    default     = "EchelonEditor",
}
appProject = _OPTIONS["project"] or "EchelonEditor"

workspace "Echelon"
    architecture "x64"
    startproject(appProject)
    language "C++"
    cppdialect "C++20"
    staticruntime "off"

    configurations { "Debug", "Release", "Dist" }

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
    defines { "ECHELON_DEBUG", "ECHELON_PROFILE=1" }
    runtime "Debug"
    symbols "on"
    buildoptions { "-Wall", "-Wextra", "-Wpedantic", "-g" }

filter "configurations:Release"
    defines { "ECHELON_RELEASE", "ECHELON_PROFILE=1" }
    runtime "Release"
    optimize "on"
    buildoptions { "-Wall", "-Wextra", "-Wpedantic", "-O2" }

filter "configurations:Dist"
    -- ECHELON_DIST strips logging, instrumentation, and hot-reload.
    -- ECHELON_PROFILE is intentionally absent → defaults to 0 in Instrumentation.hpp.
    defines { "ECHELON_DIST" }
    runtime "Release"
    optimize "Full"
    symbols "Off"

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
    include(appProject)   -- EchelonEditor by default; override with --project=NAME
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
        os.remove("./" .. (appProject or "EchelonEditor") .. "/Makefile")
        os.remove("./Vendor/Makefile")
        print("Done.")
    end
}
