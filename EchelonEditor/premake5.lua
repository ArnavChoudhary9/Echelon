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
            -- Copy shader files next to the executable so the renderer can find them
            "{MKDIR} %{cfg.buildtarget.directory}/Shaders",
            "{COPYFILE} %{wks.location}/Ray/Shaders/Flat.vert.glsl %{cfg.buildtarget.directory}/Shaders",
            "{COPYFILE} %{wks.location}/Ray/Shaders/Flat.frag.glsl %{cfg.buildtarget.directory}/Shaders",
            "{COPYFILE} %{wks.location}/Ray/Shaders/Basic.vert.glsl %{cfg.buildtarget.directory}/Shaders",
            "{COPYFILE} %{wks.location}/Ray/Shaders/Basic.frag.glsl %{cfg.buildtarget.directory}/Shaders",
            -- Seed the DefaultProject template (only if the target does not exist)
            ("IF NOT EXIST \"%{cfg.buildtarget.directory}/DefaultProject\" xcopy /E /I /Q /Y \"%{wks.location}/DefaultProject\" \"%{cfg.buildtarget.directory}/DefaultProject\""),
        }

    filter "system:linux"
        postbuildcommands
        {
            ("{COPYFILE} %{wks.location}/bin/" .. outputdir .. "/Echelon/libEchelon.so %{cfg.buildtarget.directory}"),
            ("{COPYFILE} %{wks.location}/bin/" .. outputdir .. "/" .. defaultRenderer .. "/lib" .. defaultRenderer .. ".so %{cfg.buildtarget.directory}"),
            -- Copy shader files next to the executable so the renderer can find them
            "{MKDIR} %{cfg.buildtarget.directory}/Shaders",
            "{COPYFILE} %{wks.location}/Ray/Shaders/Flat.vert.glsl %{cfg.buildtarget.directory}/Shaders",
            "{COPYFILE} %{wks.location}/Ray/Shaders/Flat.frag.glsl %{cfg.buildtarget.directory}/Shaders",
            "{COPYFILE} %{wks.location}/Ray/Shaders/Basic.vert.glsl %{cfg.buildtarget.directory}/Shaders",
            "{COPYFILE} %{wks.location}/Ray/Shaders/Basic.frag.glsl %{cfg.buildtarget.directory}/Shaders",
            -- Seed the DefaultProject template (only if the target does not exist)
            ("test -d \"%{cfg.buildtarget.directory}/DefaultProject\" || cp -r \"%{wks.location}/DefaultProject\" \"%{cfg.buildtarget.directory}/DefaultProject\""),
        }

    filter "system:macosx"
        postbuildcommands
        {
            ("{COPYFILE} %{wks.location}/bin/" .. outputdir .. "/Echelon/libEchelon.dylib %{cfg.buildtarget.directory}"),
            ("{COPYFILE} %{wks.location}/bin/" .. outputdir .. "/" .. defaultRenderer .. "/lib" .. defaultRenderer .. ".dylib %{cfg.buildtarget.directory}"),
            -- Copy shader files next to the executable so the renderer can find them
            "{MKDIR} %{cfg.buildtarget.directory}/Shaders",
            "{COPYFILE} %{wks.location}/Ray/Shaders/Flat.vert.glsl %{cfg.buildtarget.directory}/Shaders",
            "{COPYFILE} %{wks.location}/Ray/Shaders/Flat.frag.glsl %{cfg.buildtarget.directory}/Shaders",
            "{COPYFILE} %{wks.location}/Ray/Shaders/Basic.vert.glsl %{cfg.buildtarget.directory}/Shaders",
            "{COPYFILE} %{wks.location}/Ray/Shaders/Basic.frag.glsl %{cfg.buildtarget.directory}/Shaders",
            -- Seed the DefaultProject template (only if the target does not exist)
            ("test -d \"%{cfg.buildtarget.directory}/DefaultProject\" || cp -r \"%{wks.location}/DefaultProject\" \"%{cfg.buildtarget.directory}/DefaultProject\""),
        }

    filter {}