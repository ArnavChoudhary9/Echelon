-- Dependencies.lua
-- Centralized dependency definitions for the entire workspace.
-- To add a new vendor library:
--   1. Add a Dep entry below (include path, and link name if it needs compilation)
--   2. Drop the library source into Vendor/
--   3. (Optional) Add a project entry in Vendor/premake5.lua if it needs compilation
--   4. Call UseDeps("name") in any project that needs the headers
--      Call LinkDeps("name") in any project that needs to link the compiled lib

Dep = {}
Dep["spdlog"]        = { include = "%{wks.location}/Vendor/spdlog/include" }
Dep["glm"]           = { include = "%{wks.location}/Vendor/glm" }
Dep["entt"]          = { include = "%{wks.location}/Vendor/entt/single_include" }
Dep["GLFW"]          = { include = "%{wks.location}/Vendor/GLFW/include",  link = "GLFW" }
Dep["glad"]          = { include = "%{wks.location}/Vendor/glad/include",  link = "glad" }
Dep["yaml"]          = { include = "%{wks.location}/Vendor/yaml/include",  link = "yaml-cpp" }
Dep["uuid"]          = { include = "%{wks.location}/Vendor/uuid/include" }
Dep["tinyobjloader"] = { include = "%{wks.location}/Vendor/tinyobjloader" }

-- IncludeDir kept for use in filter-scoped token expressions like %{IncludeDir.spdlog}
IncludeDir = {}
for k, v in pairs(Dep) do
    IncludeDir[k] = v.include
end

-- Add include directories for the named deps.
-- Example:  UseDeps("spdlog", "glm", "uuid")
function UseDeps(...)
    local includes = {}
    for _, name in ipairs({...}) do
        local dep = Dep[name]
        assert(dep, "Unknown dependency: " .. tostring(name))
        table.insert(includes, dep.include)
    end
    includedirs(includes)
end

-- Link the compiled libs for the named deps (those with a 'link' field).
-- Centralises the dep-key → lib-name mapping so it only lives in one place.
-- Example:  LinkDeps("GLFW", "glad", "yaml")  →  links { "GLFW", "glad", "yaml-cpp" }
function LinkDeps(...)
    local libs = {}
    for _, name in ipairs({...}) do
        local dep = Dep[name]
        assert(dep, "Unknown dependency: " .. tostring(name))
        assert(dep.link, "Dependency '" .. name .. "' has no compiled link target")
        table.insert(libs, dep.link)
    end
    links(libs)
end
