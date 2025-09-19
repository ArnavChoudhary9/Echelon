workspace "Echelon"
    architecture "x64"
    startproject "Sandbox"

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
        "%{prj.name}/Application/Application.cpp"
    }

    includedirs
    {
        "%{prj.name}"
    }

    filter "system:windows"
        systemversion "latest"

        defines
        {
            "ECHELON_PLATFORM_WINDOWS",
            "ECHELON_BUILD_DLL"
        }

        postbuildcommands
        {
            ("copy \"%{cfg.buildtarget.abspath}\" \"%{wks.location}\\bin\\" .. outputdir .. "\\Sandbox\\\"")
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

-- Sandbox Application (Executable)
project "Sandbox"
    location "Sandbox"
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
        "Echelon/Application"
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

    filter "configurations:Release"
        defines "ECHELON_RELEASE"
        runtime "Release"
        optimize "on"
        buildoptions { "-Wall", "-Wextra", "-Wpedantic", "-O2" }