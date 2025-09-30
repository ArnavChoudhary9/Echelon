-- Echelon Engine (DLL)
project "Echelon"
    location "."
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"

    targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
    objdir ("../bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "**.h",
        "**.cpp"
    }

    removefiles
    {
        "Application/EntryPoint.cpp"
    }

    includedirs
    {
        ".",
        "../Vendor/spdlog/include"
    }

    links
    {
        -- Add any vendor libraries that need linking here
    }

    filter "system:windows"
        systemversion "latest"

        defines
        {
            "ECHELON_PLATFORM_WINDOWS",
            "ECHELON_BUILD_DLL"
        }

    filter "configurations:Debug"
        defines "ECHELON_DEBUG"
        runtime "Debug"
        symbols "on"
        buildoptions { "-Wall", "-Wextra", "-Wpedantic", "-g", "-fPIC" }

    filter "configurations:Release"
        defines "ECHELON_RELEASE"
        runtime "Release"
        optimize "on"
        buildoptions { "-Wall", "-Wextra", "-Wpedantic", "-O2", "-fPIC" }