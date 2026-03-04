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
    }

    -- Engine-specific defines
    filter "system:windows"
        defines { "ECHELON_BUILD_DLL" }

    -- Shared libs need position-independent code
    filter "configurations:Debug"
        buildoptions { "-fPIC" }

    filter "configurations:Release"
        buildoptions { "-fPIC" }

    filter {}
        