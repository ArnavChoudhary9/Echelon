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
        defines { "ECHELON_BUILD_DLL", "YAML_CPP_STATIC_DEFINE" }

    -- Linux system libs: X11 for GLFW, dl for dlopen (RendererLoader)
    filter "system:linux"
        links { "dl", "X11", "Xrandr", "Xinerama", "Xcursor", "Xi" }

    -- macOS frameworks: Cocoa/IOKit/CoreVideo for GLFW, OpenGL for glad
    filter "system:macosx"
        links { "Cocoa", "IOKit", "CoreVideo", "OpenGL" }

    -- Shared libs need position-independent code
    filter "configurations:Debug"
        buildoptions { "-fPIC" }

    filter "configurations:Release"
        buildoptions { "-fPIC" }

    -- -Wa,-mbig-obj is required on MinGW for translation units with heavy
    -- template usage (yaml-cpp + entt) that exceed the PE/COFF section limit.
    filter { "system:windows", "configurations:Debug" }
        buildoptions { "-Wa,-mbig-obj" }

    filter { "system:windows", "configurations:Release" }
        buildoptions { "-Wa,-mbig-obj" }

    filter {}
        