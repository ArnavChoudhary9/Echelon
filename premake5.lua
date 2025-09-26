workspace "Echelon"
    architecture "x64"
    startproject "EchelonEditor"

    configurations
    {
        "Debug",
        "Release"
    }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- Echelon Engine (DLL)
project "Echelon"
    location "Echelon"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "%{prj.name}/**.h",
        "%{prj.name}/**.cpp"
    }

    removefiles
    {
        "%{prj.name}/Application/EntryPoint.cpp"
    }

    includedirs
    {
        "%{prj.name}",
        "Vendor/spdlog/include"
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

-- EchelonEditor Application (Executable)
project "EchelonEditor"
    location "EchelonEditor"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "%{prj.name}/**.h",
        "%{prj.name}/**.cpp",
        "Echelon/Application/EntryPoint.cpp"
    }

    includedirs
    {
        "Echelon",
        "Echelon/Application",
        "Vendor/spdlog/include"
    }

    links
    {
        "Echelon"
    }

    filter "system:windows"
        systemversion "latest"

        defines
        {
            "ECHELON_PLATFORM_WINDOWS"
        }

    filter "configurations:Debug"
        defines "ECHELON_DEBUG"
        runtime "Debug"
        symbols "on"
        buildoptions { "-Wall", "-Wextra", "-Wpedantic", "-g" }
        linkoptions { "-Wl,--allow-multiple-definition" }

    filter "configurations:Release"
        defines "ECHELON_RELEASE"
        runtime "Release"
        optimize "on"
        buildoptions { "-Wall", "-Wextra", "-Wpedantic", "-O2" }
        linkoptions { "-Wl,--allow-multiple-definition" }

    filter {}
        postbuildcommands
        {
            ("copy \"..\\bin\\" .. outputdir .. "\\Echelon\\Echelon.dll\" \"%{cfg.buildtarget.directory}\"")
        }