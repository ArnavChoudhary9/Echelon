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

    includedirs
    {
        "%{wks.location}",
        "%{wks.location}/Echelon",
        "%{IncludeDir.spdlog}",
        "%{IncludeDir.glm}",
        "%{IncludeDir.entt}",
        "%{IncludeDir.yaml}",
    }

    links
    {
        "Echelon",
    }

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
        }

    filter "system:linux"
        postbuildcommands
        {
            ("{COPYFILE} %{wks.location}/bin/" .. outputdir .. "/Echelon/libEchelon.so %{cfg.buildtarget.directory}"),
            ("{COPYFILE} %{wks.location}/bin/" .. outputdir .. "/" .. defaultRenderer .. "/lib" .. defaultRenderer .. ".so %{cfg.buildtarget.directory}"),
        }

    filter "system:macosx"
        postbuildcommands
        {
            ("{COPYFILE} %{wks.location}/bin/" .. outputdir .. "/Echelon/libEchelon.dylib %{cfg.buildtarget.directory}"),
            ("{COPYFILE} %{wks.location}/bin/" .. outputdir .. "/" .. defaultRenderer .. "/lib" .. defaultRenderer .. ".dylib %{cfg.buildtarget.directory}"),
        }

    filter {}