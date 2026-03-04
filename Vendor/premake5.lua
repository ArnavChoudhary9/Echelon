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