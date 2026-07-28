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

    -- Export symbols on Windows
    filter "system:windows"
        defines { "RAY_BUILD_DLL" }
        linkoptions { "-Wl,--allow-multiple-definition" }

    -- GNU ld flag; not supported by Apple ld
    filter "system:linux"
        linkoptions { "-Wl,--allow-multiple-definition" }

    -- Frameworks required by glad/GLFW on macOS
    filter "system:macosx"
        links { "OpenGL" }

    -- PIC is required for shared libraries on all non-Windows platforms
    filter "configurations:Debug"
        buildoptions { "-fPIC" }

    filter "configurations:Release"
        buildoptions { "-fPIC" }

    filter {}
