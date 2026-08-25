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

    -- Dear ImGui backends are compiled straight into the engine (not the core
    -- static lib) so they bind to libEchelon's single GLFW + glad instance.
    files
    {
        "%{wks.location}/Vendor/ImGUI/backends/imgui_impl_glfw.cpp",
        "%{wks.location}/Vendor/ImGUI/backends/imgui_impl_opengl3.cpp",
    }

    includedirs { ".", "%{wks.location}" }
    includedirs { Dep.ImGUI.include, Dep.ImGUI.backends }
    UseDeps("spdlog", "glm", "entt", "GLFW", "glad", "yaml", "uuid", "tinyobjloader", "stb", "slang")
    LinkDeps("GLFW", "glad", "yaml", "slang")

    -- Build the ImGui core lib before the engine (it is pulled in via the
    -- whole-archive linkoption below, not through links{}, so declare the order).
    dependson { "ImGUI" }

    -- Slang is a prebuilt SDK: point the linker at its lib directory (libslang.so).
    libdirs { Dep.slang.libdir }

    -- Windows system libs needed by GLFW
    filter "system:windows"
        links { "gdi32", "opengl32" }
        -- NOTE: exporting the ImGui core from Echelon.dll for the editor requires
        -- IMGUI_API=__declspec(dllexport/dllimport); the Linux build re-exports it via
        -- --whole-archive below. Windows is not the primary target (see Slang note).
        links { "ImGUI" }
        defines { "ECHELON_BUILD_DLL", "YAML_CPP_STATIC_DEFINE" }

    -- Linux system libs: X11 for GLFW, dl for dlopen (RendererLoader).
    -- rpath=$ORIGIN lets libEchelon.so find its transitive dep libslang-*.so when
    -- both are copied into the runtime directory (DT_RUNPATH does not cascade from
    -- the executable to a library's own NEEDED deps, so the engine carries its own).
    filter "system:linux"
        links { "dl", "X11", "Xrandr", "Xinerama", "Xcursor", "Xi" }
        linkoptions { "-Wl,-rpath,'$$ORIGIN'" }
        -- Whole-archive the ImGui core so ALL its symbols (widgets, docking,
        -- DockBuilder, …) are pulled into libEchelon.so and re-exported. The
        -- editor exe calls ImGui:: directly and resolves those symbols here, so
        -- there is exactly ONE ImGui context (no per-DLL-boundary duplication).
        -- The core has no external link deps, so link order is irrelevant.
        linkoptions {
            "-Wl,--whole-archive",
            "%{wks.location}/bin/" .. outputdir .. "/ImGUI/libImGUI.a",
            "-Wl,--no-whole-archive",
        }
        -- (The Slang runtime libs are shipped beside libEchelon on every platform by
        --  the SlangRuntimeCopy block below.)

    -- macOS frameworks: Cocoa/IOKit/CoreVideo for GLFW, OpenGL for glad
    filter "system:macosx"
        links { "Cocoa", "IOKit", "CoreVideo", "OpenGL" }
        -- macOS has no --whole-archive; -all_load applies to all archives, so link
        -- ImGui via -force_load to re-export only its symbols from libEchelon.dylib.
        linkoptions { "-Wl,-force_load,%{wks.location}/bin/" .. outputdir .. "/ImGUI/libImGUI.a" }
        linkoptions { "-Wl,-rpath,@loader_path" }

    -- Ship the Slang runtime shared libs (fetched by scripts/setup.py) next to
    -- libEchelon on every platform: the engine has a transitive NEEDED on libslang and
    -- dlopens glslang/glsl-module, and the exe may load the canonical libEchelon from
    -- this dir (its $ORIGIN / @loader_path rpath resolves the runtime libs here).
    filter {}
    SlangRuntimeCopy("%{cfg.buildtarget.directory}")

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
        