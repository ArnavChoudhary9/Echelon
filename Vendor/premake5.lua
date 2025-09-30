-- Vendor Dependencies
group "Vendor"

-- spdlog (Header-only library, no compilation needed)
-- This project is mainly for organization and include directories
project "spdlog"
    location "spdlog"
    kind "None"
    language "C++"

    targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
    objdir ("../bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "spdlog/include/spdlog/**.h",
        "spdlog/include/spdlog/**.hpp",
        "spdlog/src/**.cpp"
    }

    includedirs
    {
        "spdlog/include"
    }

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"

group ""