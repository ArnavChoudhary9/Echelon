workspace "Echelon"
    architecture "x64"
    startproject "EchelonEditor"

    configurations
    {
        "Debug",
        "Release"
    }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- Include all sub-projects
include "Vendor"
include "Echelon"
include "EchelonEditor"