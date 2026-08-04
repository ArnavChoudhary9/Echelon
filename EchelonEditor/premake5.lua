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

    includedirs { "%{wks.location}", "%{wks.location}/Echelon" }
    UseDeps("spdlog", "glm", "entt", "yaml", "uuid", "tinyobjloader")

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

    -- Copy the engine + the selected renderer shared library next to the editor
    -- executable after build. Only the chosen renderer (`--renderer=<name>`) is
    -- copied, so a custom default keeps the package free of unused plugins.
    -- The starter "DefaultProject" template lives at the repo root and is seeded
    -- into the working directory next to the editor. This is an EDITOR asset only
    -- — the engine itself never depends on it and regenerates a project at runtime
    -- if none is present (see Application::InitializeProject). The copy is
    -- no-clobber: an existing working-dir project (with the user's edits) is left
    -- untouched, and it is re-seeded only after being deleted.
    filter "system:windows"
        postbuildcommands
        {
            ("{COPYFILE} %{wks.location}/bin/" .. outputdir .. "/Echelon/Echelon.dll %{cfg.buildtarget.directory}"),
            ("{COPYFILE} %{wks.location}/bin/" .. outputdir .. "/" .. defaultRenderer .. "/" .. defaultRenderer .. ".dll %{cfg.buildtarget.directory}"),
            -- NOTE: Windows requires its own vendored Slang binaries (slang.dll +
            -- slang-glslang.dll) copied here — the repo currently vendors Linux libs only.
            -- Copy Slang shader source next to the executable so the renderer can find them.
            -- Echelon.slang is the engine-owned shader constant system (import Echelon).
            "{MKDIR} %{cfg.buildtarget.directory}/Shaders",
            "{COPYFILE} %{wks.location}/Echelon/Shaders/Echelon.slang %{cfg.buildtarget.directory}/Shaders",
            "{COPYFILE} %{wks.location}/Echelon/Shaders/Flat.slang %{cfg.buildtarget.directory}/Shaders",
            "{COPYFILE} %{wks.location}/Echelon/Shaders/Error.slang %{cfg.buildtarget.directory}/Shaders",
            "{COPYFILE} %{wks.location}/Ray/Shaders/Basic.slang %{cfg.buildtarget.directory}/Shaders",
            -- Seed the DefaultProject template (only if the target does not exist)
            ("IF NOT EXIST \"%{cfg.buildtarget.directory}/DefaultProject\" xcopy /E /I /Q /Y \"%{wks.location}/DefaultProject\" \"%{cfg.buildtarget.directory}/DefaultProject\""),
        }

    filter "system:linux"
        postbuildcommands
        {
            ("{COPYFILE} %{wks.location}/bin/" .. outputdir .. "/Echelon/libEchelon.so %{cfg.buildtarget.directory}"),
            ("{COPYFILE} %{wks.location}/bin/" .. outputdir .. "/" .. defaultRenderer .. "/lib" .. defaultRenderer .. ".so %{cfg.buildtarget.directory}"),
            -- Copy the prebuilt Slang runtime libs next to the executable (beside
            -- libEchelon.so, which NEEDs libslang-compiler and dlopens glslang).
            "{COPYFILE} %{wks.location}/Vendor/slang/lib/libslang-compiler.so.0.2026.14.1 %{cfg.buildtarget.directory}",
            "{COPYFILE} %{wks.location}/Vendor/slang/lib/libslang-glslang-2026.14.1.so %{cfg.buildtarget.directory}",
            "{COPYFILE} %{wks.location}/Vendor/slang/lib/libslang-glsl-module-2026.14.1.so %{cfg.buildtarget.directory}",
            "{COPYFILE} %{wks.location}/Vendor/slang/lib/libslang-rt.so.0.2026.14.1 %{cfg.buildtarget.directory}",
            -- Copy Slang shader source next to the executable so the renderer can find them.
            -- Echelon.slang is the engine-owned shader constant system (import Echelon).
            "{MKDIR} %{cfg.buildtarget.directory}/Shaders",
            "{COPYFILE} %{wks.location}/Echelon/Shaders/Echelon.slang %{cfg.buildtarget.directory}/Shaders",
            "{COPYFILE} %{wks.location}/Echelon/Shaders/Flat.slang %{cfg.buildtarget.directory}/Shaders",
            "{COPYFILE} %{wks.location}/Echelon/Shaders/Error.slang %{cfg.buildtarget.directory}/Shaders",
            "{COPYFILE} %{wks.location}/Ray/Shaders/Basic.slang %{cfg.buildtarget.directory}/Shaders",
            -- Seed the DefaultProject template (only if the target does not exist)
            ("test -d \"%{cfg.buildtarget.directory}/DefaultProject\" || cp -r \"%{wks.location}/DefaultProject\" \"%{cfg.buildtarget.directory}/DefaultProject\""),
        }

    filter "system:macosx"
        postbuildcommands
        {
            ("{COPYFILE} %{wks.location}/bin/" .. outputdir .. "/Echelon/libEchelon.dylib %{cfg.buildtarget.directory}"),
            ("{COPYFILE} %{wks.location}/bin/" .. outputdir .. "/" .. defaultRenderer .. "/lib" .. defaultRenderer .. ".dylib %{cfg.buildtarget.directory}"),
            -- NOTE: macOS requires its own vendored Slang binaries (libslang.dylib +
            -- libslang-glslang.dylib) copied here — the repo currently vendors Linux libs only.
            -- Copy Slang shader source next to the executable so the renderer can find them.
            -- Echelon.slang is the engine-owned shader constant system (import Echelon).
            "{MKDIR} %{cfg.buildtarget.directory}/Shaders",
            "{COPYFILE} %{wks.location}/Echelon/Shaders/Echelon.slang %{cfg.buildtarget.directory}/Shaders",
            "{COPYFILE} %{wks.location}/Echelon/Shaders/Flat.slang %{cfg.buildtarget.directory}/Shaders",
            "{COPYFILE} %{wks.location}/Echelon/Shaders/Error.slang %{cfg.buildtarget.directory}/Shaders",
            "{COPYFILE} %{wks.location}/Ray/Shaders/Basic.slang %{cfg.buildtarget.directory}/Shaders",
            -- Seed the DefaultProject template (only if the target does not exist)
            ("test -d \"%{cfg.buildtarget.directory}/DefaultProject\" || cp -r \"%{wks.location}/DefaultProject\" \"%{cfg.buildtarget.directory}/DefaultProject\""),
        }

    filter {}