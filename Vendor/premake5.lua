-- ============================================================
-- Vendor Dependencies (header-only libraries)
-- ============================================================
-- These "None" projects exist purely so IDEs can browse vendor
-- source.  All include paths are centralised in Dependencies.lua.
-- No compilation is performed for header-only libs.
-- ============================================================

local function header_only_project(name, loc, fileGlobs)
    project(name)
        location(loc)
        kind "None"
        language "C++"
        files(fileGlobs)
end

header_only_project("spdlog", "spdlog", {
    "spdlog/include/spdlog/**.h",
    "spdlog/include/spdlog/**.hpp",
})

header_only_project("glm", "glm", {
    "glm/glm/**.hpp",
    "glm/glm/**.inl",
})

header_only_project("entt", "entt", {
    "entt/single_include/entt/**.hpp",
})

-- ============================================================
-- yaml-cpp  (Compiled static library)
-- ============================================================
project "yaml-cpp"
    location "yaml"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"
    warnings "off"

    targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
    objdir ("../bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "yaml/src/**.h",
        "yaml/src/**.cpp",
        "yaml/include/**.h",
    }

    includedirs
    {
        "yaml/include",
    }

    defines
    {
        "YAML_CPP_STATIC_DEFINE",
    }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"

    filter {}

-- ============================================================
-- glad  (Compiled static library — OpenGL loader)
-- ============================================================
project "glad"
    location "glad/generated"
    kind "StaticLib"
    language "C"
    staticruntime "off"
    warnings "off"

    targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
    objdir ("../bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "glad/generated/src/gl.c",
        "glad/generated/include/glad/gl.h",
        "glad/generated/include/KHR/khrplatform.h",
    }

    includedirs
    {
        "glad/generated/include",
    }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"

    filter {}

-- ============================================================
-- GLFW  (Compiled static library)
-- ============================================================
project "GLFW"
    location "GLFW"
    kind "StaticLib"
    language "C"
    staticruntime "off"
    warnings "off"

    targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
    objdir ("../bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "GLFW/include/GLFW/glfw3.h",
        "GLFW/include/GLFW/glfw3native.h",
        "GLFW/src/context.c",
        "GLFW/src/init.c",
        "GLFW/src/input.c",
        "GLFW/src/monitor.c",
        "GLFW/src/platform.c",
        "GLFW/src/vulkan.c",
        "GLFW/src/window.c",
        "GLFW/src/egl_context.c",
        "GLFW/src/osmesa_context.c",
        "GLFW/src/null_init.c",
        "GLFW/src/null_monitor.c",
        "GLFW/src/null_window.c",
        "GLFW/src/null_joystick.c",
    }

    includedirs
    {
        "GLFW/include",
    }

    filter "system:windows"
        files
        {
            "GLFW/src/win32_init.c",
            "GLFW/src/win32_module.c",
            "GLFW/src/win32_joystick.c",
            "GLFW/src/win32_monitor.c",
            "GLFW/src/win32_window.c",
            "GLFW/src/win32_thread.c",
            "GLFW/src/win32_time.c",
            "GLFW/src/wgl_context.c",
        }
        defines { "_GLFW_WIN32" }

    filter "system:linux"
        files
        {
            "GLFW/src/x11_init.c",
            "GLFW/src/x11_monitor.c",
            "GLFW/src/x11_window.c",
            "GLFW/src/posix_module.c",
            "GLFW/src/posix_poll.c",
            "GLFW/src/posix_thread.c",
            "GLFW/src/posix_time.c",
            "GLFW/src/glx_context.c",
            "GLFW/src/linux_joystick.c",
            "GLFW/src/xkb_unicode.c",
        }
        defines { "_GLFW_X11" }

    filter "system:macosx"
        files
        {
            "GLFW/src/cocoa_init.m",
            "GLFW/src/cocoa_monitor.m",
            "GLFW/src/cocoa_window.m",
            "GLFW/src/cocoa_joystick.m",
            "GLFW/src/cocoa_time.c",
            "GLFW/src/nsgl_context.m",
            "GLFW/src/posix_module.c",
            "GLFW/src/posix_thread.c",
        }
        defines { "_GLFW_COCOA" }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"

    filter {}