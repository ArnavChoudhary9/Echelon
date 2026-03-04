-- ============================================================
-- Ray Renderer  (Shared Library / DLL → Renderer.dll)
-- ============================================================
-- This project compiles to Renderer.dll (or libRenderer.so).
-- Swap it with any DLL that exports the same CreateRenderer /
-- DestroyRenderer factory and the engine works unchanged.
-- ============================================================

project "Ray"
    location "."
    kind "SharedLib"

    -- Output name is "Renderer" — not "Ray" — so the engine always
    -- loads the same filename regardless of which renderer is active.
    targetname "Renderer"

    targetdir ("../bin/" .. outputdir .. "/Renderer")
    objdir ("../bin-int/" .. outputdir .. "/Ray")

    files
    {
        "**.h",
        "**.hpp",
        "**.cpp",
    }

    includedirs
    {
        "%{wks.location}",
        "%{wks.location}/Echelon",
        "%{IncludeDir.glm}",
        "%{IncludeDir.glad}",
        "%{IncludeDir.GLFW}",
        "%{IncludeDir.spdlog}",
    }

    links
    {
        "Echelon",
        "glad",
    }

    -- Allow multiple definitions when linking against engine DLL + system libs
    linkoptions { "-Wl,--allow-multiple-definition" }

    -- Export symbols on Windows
    filter "system:windows"
        defines { "RAY_BUILD_DLL" }

    -- PIC needed for shared libraries on all platforms
    filter "configurations:Debug"
        buildoptions { "-fPIC" }

    filter "configurations:Release"
        buildoptions { "-fPIC" }

    filter {}
