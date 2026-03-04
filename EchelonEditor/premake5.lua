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
    }

    links
    {
        "Echelon",
    }

    -- Linker: allow multiple definitions (needed when EntryPoint is shared)
    linkoptions { "-Wl,--allow-multiple-definition" }

    -- Copy the engine DLL next to the editor executable after build
    filter "system:windows"
        postbuildcommands
        {
            ("{COPYFILE} %{wks.location}/bin/" .. outputdir .. "/Echelon/Echelon.dll %{cfg.buildtarget.directory}")
        }

    filter {}