-- ============================================================
-- EchelonEditor  (Console Application)
-- ============================================================

project "EchelonEditor"
    location "."
    kind "ConsoleApp"

    targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
    objdir ("../bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "**.h",
        "**.hpp",
        "**.cpp",
        "../Echelon/Application/EntryPoint.cpp",
    }

    includedirs { "%{wks.location}", "%{wks.location}/Echelon", "%{wks.location}/EchelonEditor" }
    includedirs { Dep.ImGUI.include, Dep.ImGUI.backends }
    UseDeps("spdlog", "glm", "entt", "yaml", "uuid", "tinyobjloader", "stb")

    -- The editor calls ImGui:: directly (dockspace + panels). The symbols live in
    -- libEchelon (whole-archived core + backends), so only the headers are needed
    -- here — do NOT link the ImGui archive again or a second context would appear.
    links { "Echelon" }

    -- libEchelon.so has a transitive NEEDED on libslang-compiler; the linker must be
    -- able to find it to resolve the engine's Slang symbols when linking this exe.
    -- (-rpath-link is link-time only; runtime resolution is via libEchelon's $ORIGIN
    -- rpath + the Slang libs copied next to the executable in postbuild.)
    filter { "system:linux" }
        linkoptions { "-Wl,-rpath-link,%{wks.location}/Vendor/slang/lib" }
    filter { "system:macosx" }
        linkoptions { "-Wl,-rpath,@loader_path" }
    filter {}

    defines
    {
        "YAML_CPP_STATIC_DEFINE",
        -- Bake the build-time default renderer name into the application so
        -- ApplicationConfig::DefaultRenderer picks it up (renderer-agnostic engine).
        ("ECHELON_DEFAULT_RENDERER=\"" .. defaultRenderer .. "\""),
    }

    -- ============================================================
    -- Assemble the runtime directory next to the editor executable
    -- ============================================================
    -- Everything platform-agnostic is copied once via cross-platform premake tokens
    -- (the copy helpers live in Dependencies.lua and discover their file lists by glob,
    -- so new shaders / runtime libs / resources ship with no build edits). Only the
    -- shared-library filenames and the no-clobber seeds differ per OS — those stay in
    -- the small per-system blocks below.
    filter {}
        -- Slang runtime libs beside the exe (libEchelon also carries its own copy).
        SlangRuntimeCopy("%{cfg.buildtarget.directory}")

        -- Shaders → <exe>/Shaders: the selected renderer's shaders (Ray.slang ABI,
        -- PBR + PBR.ehmaterialtype, Flat/Error fallbacks, shadow + IBL + Bloom/Tonemap)
        -- and the editor-owned object-id (EntityID) shader.
        CopyShaders("%{cfg.buildtarget.directory}",
                    defaultRenderer .. "/Shaders",
                    "EchelonEditor/Shaders")

        -- Editor resources: icons + fonts (always refreshed to match the build).
        CopyGlob("EchelonEditor/Resources/Icons/*.png",
                 "%{cfg.buildtarget.directory}/EditorResources/Icons")
        CopyGlob("EchelonEditor/Resources/Fonts/opensans/*.ttf",
                 "%{cfg.buildtarget.directory}/EditorResources/Fonts/opensans")

    -- ---- Per-OS: shared-library names + no-clobber seeds ----
    -- Copy the engine + the selected renderer shared library (OS-specific prefix/
    -- extension) beside the exe. Only the chosen renderer (`--renderer=<name>`) is
    -- copied, keeping the package free of unused plugins. DefaultProject and imgui.ini
    -- are seeded ONLY if absent, so a user's working-dir edits survive rebuilds (this
    -- "copy if missing" needs a per-OS conditional command with no premake token).
    filter "system:windows"
        postbuildcommands {
            ("{COPYFILE} %{wks.location}/bin/" .. outputdir .. "/Echelon/Echelon.dll %{cfg.buildtarget.directory}"),
            ("{COPYFILE} %{wks.location}/bin/" .. outputdir .. "/" .. defaultRenderer .. "/" .. defaultRenderer .. ".dll %{cfg.buildtarget.directory}"),
            ("IF NOT EXIST \"%{cfg.buildtarget.directory}/DefaultProject\" xcopy /E /I /Q /Y \"%{wks.location}/DefaultProject\" \"%{cfg.buildtarget.directory}/DefaultProject\""),
            ("IF NOT EXIST \"%{cfg.buildtarget.directory}\\imgui.ini\" copy /Y \"%{wks.location}\\EchelonEditor\\Resources\\imgui.ini\" \"%{cfg.buildtarget.directory}\\imgui.ini\""),
        }

    filter "system:linux"
        postbuildcommands {
            ("{COPYFILE} %{wks.location}/bin/" .. outputdir .. "/Echelon/libEchelon.so %{cfg.buildtarget.directory}"),
            ("{COPYFILE} %{wks.location}/bin/" .. outputdir .. "/" .. defaultRenderer .. "/lib" .. defaultRenderer .. ".so %{cfg.buildtarget.directory}"),
            ("test -d \"%{cfg.buildtarget.directory}/DefaultProject\" || cp -r \"%{wks.location}/DefaultProject\" \"%{cfg.buildtarget.directory}/DefaultProject\""),
            ("test -f \"%{cfg.buildtarget.directory}/imgui.ini\" || cp \"%{wks.location}/EchelonEditor/Resources/imgui.ini\" \"%{cfg.buildtarget.directory}/imgui.ini\""),
        }

    filter "system:macosx"
        postbuildcommands {
            ("{COPYFILE} %{wks.location}/bin/" .. outputdir .. "/Echelon/libEchelon.dylib %{cfg.buildtarget.directory}"),
            ("{COPYFILE} %{wks.location}/bin/" .. outputdir .. "/" .. defaultRenderer .. "/lib" .. defaultRenderer .. ".dylib %{cfg.buildtarget.directory}"),
            ("test -d \"%{cfg.buildtarget.directory}/DefaultProject\" || cp -r \"%{wks.location}/DefaultProject\" \"%{cfg.buildtarget.directory}/DefaultProject\""),
            ("test -f \"%{cfg.buildtarget.directory}/imgui.ini\" || cp \"%{wks.location}/EchelonEditor/Resources/imgui.ini\" \"%{cfg.buildtarget.directory}/imgui.ini\""),
        }

    filter {}
