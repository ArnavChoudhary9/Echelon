-- ============================================================
-- Ray Renderer  (Shared Library / DLL → libRay.so / Ray.dll)
-- ============================================================
-- This project compiles to the "Ray" renderer plugin
-- (libRay.so / Ray.dll / libRay.dylib). Any plugin that exports the same
-- CreateRenderer / DestroyRenderer factory can replace it — build it in a
-- folder named after itself and select it with `--renderer=<name>`.
-- ============================================================

project "Ray"
    location "."
    kind "SharedLib"

    -- Output name matches the plugin name so the engine can load it by name.
    targetname "Ray"

    targetdir ("../bin/" .. outputdir .. "/Ray")
    objdir ("../bin-int/" .. outputdir .. "/Ray")

    files
    {
        "**.h",
        "**.hpp",
        "**.cpp",
    }

    includedirs { "%{wks.location}", "%{wks.location}/Echelon" }
    UseDeps("glm", "GLFW", "spdlog", "yaml", "entt", "uuid")

    -- Ray uses only the engine's GraphicsAPI abstraction — the OpenGL backend
    -- and glad live inside libEchelon, so the plugin links ONLY the engine.
    -- (This is why no `-Wl,--allow-multiple-definition` is needed: there is a
    -- single glad instance shared by all modules, so no duplicate symbols.)
    links { "Echelon" }

    -- Export symbols on Windows
    filter "system:windows"
        defines { "RAY_BUILD_DLL", "YAML_CPP_STATIC_DEFINE" }

    -- PIC is required for shared libraries on all non-Windows platforms
    filter "configurations:Debug"
        buildoptions { "-fPIC" }

    filter "configurations:Release"
        buildoptions { "-fPIC" }

    filter {}
