-- ============================================================
-- Echelon Engine  (Shared Library / DLL)
-- ============================================================

project "Echelon"
    location "."
    kind "SharedLib"

    targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
    objdir ("../bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "**.h",
        "**.hpp",
        "**.cpp",
    }

    removefiles
    {
        "Application/EntryPoint.cpp",
    }

    includedirs
    {
        ".",
        "%{wks.location}",
        "%{IncludeDir.spdlog}",
        "%{IncludeDir.glm}",
        "%{IncludeDir.entt}",
        "%{IncludeDir.GLFW}",
        "%{IncludeDir.glad}",
        "%{IncludeDir.yaml}",
    }

    links
    {
        "GLFW",
        "glad",
        "yaml-cpp",
    }

    -- Windows system libs needed by GLFW
    filter "system:windows"
        links { "gdi32", "opengl32" }

    -- Engine-specific defines
    filter "system:windows"
        defines { "ECHELON_BUILD_DLL", "YAML_CPP_STATIC_DEFINE" }

    -- Shared libs need position-independent code
    -- -Wa,-mbig-obj is required on MinGW for translation units with heavy
    -- template usage (yaml-cpp + entt) that exceed the PE/COFF section limit.
    filter "configurations:Debug"
        buildoptions { "-fPIC", "-Wa,-mbig-obj" }

    filter "configurations:Release"
        buildoptions { "-fPIC", "-Wa,-mbig-obj" }

    filter {}
        