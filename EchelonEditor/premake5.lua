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
    }

    -- Linker: allow multiple definitions (needed when EntryPoint is shared)
    linkoptions { "-Wl,--allow-multiple-definition" }

    -- Copy engine + renderer DLLs next to the editor executable after build
    filter "system:windows"
        postbuildcommands
        {
            ("{COPYFILE} %{wks.location}/bin/" .. outputdir .. "/Echelon/Echelon.dll %{cfg.buildtarget.directory}"),
            ("{COPYFILE} %{wks.location}/bin/" .. outputdir .. "/Renderer/Renderer.dll %{cfg.buildtarget.directory}"),
            -- Copy shader files next to the executable so the renderer can find them
            "{MKDIR} %{cfg.buildtarget.directory}/Shaders",
            "{COPYFILE} %{wks.location}/Ray/Shaders/Flat.vert.glsl %{cfg.buildtarget.directory}/Shaders",
            "{COPYFILE} %{wks.location}/Ray/Shaders/Flat.frag.glsl %{cfg.buildtarget.directory}/Shaders",
            "{COPYFILE} %{wks.location}/Ray/Shaders/Basic.vert.glsl %{cfg.buildtarget.directory}/Shaders",
            "{COPYFILE} %{wks.location}/Ray/Shaders/Basic.frag.glsl %{cfg.buildtarget.directory}/Shaders",
        }

    filter {}