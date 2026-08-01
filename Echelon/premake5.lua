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

    -- ---- Platform file-watcher backend: compile only the matching OS backend ----
    -- Each backend .cpp is also guarded by #if defined(ECHELON_PLATFORM_*) as a
    -- safety net, but removefiles keeps the build clean on known platforms.
    filter "system:linux"
        removefiles {
            "Platform/Backends/Windows/**",
            "Platform/Backends/MacOS/**",
            "Platform/Backends/Generic/**",
        }
    filter "system:windows"
        removefiles {
            "Platform/Backends/Linux/**",
            "Platform/Backends/MacOS/**",
            "Platform/Backends/Generic/**",
        }
    filter "system:macosx"
        removefiles {
            "Platform/Backends/Linux/**",
            "Platform/Backends/Windows/**",
            "Platform/Backends/Generic/**",
        }
    filter {}

    -- ---- Graphics API backends: compile every selected backend, exclude the rest ----
    -- graphicsBackends (table) and defaultGraphicsBackend (string) come from root premake5.lua.
    -- Add entries to knownBackends as new backends are implemented; the folder name must match.
    do
        local knownBackends = { "OpenGL", "Vulkan", "DirectX12", "Metal" }

        local selected = {}
        for _, b in ipairs(graphicsBackends) do selected[b] = true end

        -- One ECHELON_GRAPHICS_BACKEND_* per compiled backend so source can guard
        -- backend-specific code with #ifdef.
        -- ECHELON_DEFAULT_GRAPHICS_BACKEND_* marks the first-in-list backend as the
        -- runtime default returned by GraphicsAPI::GetDefaultBackend().
        local defs = {}
        for _, b in ipairs(graphicsBackends) do
            defs[#defs + 1] = "ECHELON_GRAPHICS_BACKEND_" .. b:upper()
        end
        defs[#defs + 1] = "ECHELON_DEFAULT_GRAPHICS_BACKEND_" .. defaultGraphicsBackend:upper()
        defines(defs)

        -- Exclude source directories for backends that are not selected.
        for _, b in ipairs(knownBackends) do
            if not selected[b] then
                removefiles { "Platform/Backends/" .. b .. "/**" }
            end
        end
    end

    includedirs { ".", "%{wks.location}" }
    UseDeps("spdlog", "glm", "entt", "GLFW", "glad", "yaml", "uuid", "tinyobjloader", "slang")
    LinkDeps("GLFW", "glad", "yaml", "slang")

    -- Slang is a prebuilt SDK: point the linker at its lib directory (libslang.so).
    libdirs { Dep.slang.libdir }

    -- Windows system libs needed by GLFW
    filter "system:windows"
        links { "gdi32", "opengl32" }
        defines { "ECHELON_BUILD_DLL", "YAML_CPP_STATIC_DEFINE" }

    -- Linux system libs: X11 for GLFW, dl for dlopen (RendererLoader).
    -- rpath=$ORIGIN lets libEchelon.so find its transitive dep libslang-*.so when
    -- both are copied into the runtime directory (DT_RUNPATH does not cascade from
    -- the executable to a library's own NEEDED deps, so the engine carries its own).
    filter "system:linux"
        links { "dl", "X11", "Xrandr", "Xinerama", "Xcursor", "Xi" }
        linkoptions { "-Wl,-rpath,'$$ORIGIN'" }
        -- Ship the prebuilt Slang runtime libs next to libEchelon.so (its $ORIGIN
        -- rpath resolves them here). The editor copies them beside the exe too, but
        -- the exe may load the canonical libEchelon from this dir, so it needs them.
        postbuildcommands {
            "{COPYFILE} %{wks.location}/Vendor/slang/lib/libslang-compiler.so.0.2026.14.1 %{cfg.buildtarget.directory}",
            "{COPYFILE} %{wks.location}/Vendor/slang/lib/libslang-glslang-2026.14.1.so %{cfg.buildtarget.directory}",
            "{COPYFILE} %{wks.location}/Vendor/slang/lib/libslang-glsl-module-2026.14.1.so %{cfg.buildtarget.directory}",
            "{COPYFILE} %{wks.location}/Vendor/slang/lib/libslang-rt.so.0.2026.14.1 %{cfg.buildtarget.directory}",
        }

    -- macOS frameworks: Cocoa/IOKit/CoreVideo for GLFW, OpenGL for glad
    filter "system:macosx"
        links { "Cocoa", "IOKit", "CoreVideo", "OpenGL" }
        linkoptions { "-Wl,-rpath,@loader_path" }

    -- Shared libs need position-independent code
    filter "configurations:Debug"
        buildoptions { "-fPIC" }

    filter "configurations:Release"
        buildoptions { "-fPIC" }

    filter "configurations:Dist"
        buildoptions { "-fPIC" }

    -- -Wa,-mbig-obj is required on MinGW for translation units with heavy
    -- template usage (yaml-cpp + entt) that exceed the PE/COFF section limit.
    filter { "system:windows", "configurations:Debug" }
        buildoptions { "-Wa,-mbig-obj" }

    filter { "system:windows", "configurations:Release" }
        buildoptions { "-Wa,-mbig-obj" }

    filter { "system:windows", "configurations:Dist" }
        buildoptions { "-Wa,-mbig-obj" }

    filter {}
        