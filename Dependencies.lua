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
-- stb is header-only: STB_IMAGE_IMPLEMENTATION is defined in a single TU
-- (TextureImporter.cpp), so there is no compiled lib to link — include only.
Dep["stb"]           = { include = "%{wks.location}/Vendor/stb" }
-- Dear ImGui (docking branch). Compiled to a static lib (Vendor/premake5.lua) that
-- holds ONLY the core (imgui*.cpp) — the GLFW/OpenGL3 backends are compiled straight
-- into libEchelon (next to GLFW/glad). The core lib is whole-archived into the engine
-- so every ImGui symbol is re-exported for the application/editor to call across the
-- shared-library boundary (keeping a single ImGui context). See Echelon/premake5.lua.
Dep["ImGUI"]         = { include = "%{wks.location}/Vendor/ImGUI",
                         backends = "%{wks.location}/Vendor/ImGUI/backends",
                         link = "ImGUI" }
-- Slang is a *prebuilt* SDK fetched per-platform by scripts/setup.py (NOT committed and
-- NOT compiled in Vendor/premake5.lua). setup.py lays out three dirs under Vendor/slang:
--   include/   headers               → 'include' below
--   lib/       link-time libs        → 'libdir' below (libslang.so / slang.lib, incl. symlinks)
--   runtime/   exactly the shared libs to ship beside the binaries (real files, no extras)
-- 'runtime' is a plain repo-relative path (no %{token}) because SlangRuntimeCopy() globs it
-- with os.matchfiles at generation time. See the copy helpers at the bottom of this file.
Dep["slang"]         = { include = "%{wks.location}/Vendor/slang/include",
                         libdir  = "%{wks.location}/Vendor/slang/lib",
                         runtime = "Vendor/slang/runtime",
                         link    = "slang" }

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

-- ============================================================
-- Postbuild copy helpers
-- ============================================================
-- premake's {COPYFILE}/{MKDIR} tokens are cross-platform, so runtime-dir assembly
-- (shared libs, shaders, editor resources) needs NO per-OS postbuild duplication.
-- File lists are discovered by glob at generation time, so adding a shader / runtime
-- lib / resource requires no build edits.
--
-- _MAIN_SCRIPT_DIR is the repo root (dir of the root premake5.lua), used to resolve
-- glob patterns to real files now; the emitted copy commands use %{wks.location}
-- (also the repo root) so paths resolve again at build time.

-- Copy every file matching `pattern` (repo-root-relative) into `destDir`.
-- Creates destDir first. Returns the number of files matched.
function CopyGlob(pattern, destDir)
    local files = os.matchfiles(_MAIN_SCRIPT_DIR .. "/" .. pattern)
    local cmds = { "{MKDIR} " .. destDir }
    for _, f in ipairs(files) do
        local rel = path.getrelative(_MAIN_SCRIPT_DIR, f)
        table.insert(cmds, "{COPYFILE} %{wks.location}/" .. rel .. " " .. destDir)
    end
    postbuildcommands(cmds)
    return #files
end

-- Ship the Slang runtime shared libs (populated by scripts/setup.py) next to `destDir`.
-- Version-agnostic: whatever setup.py curated into Vendor/slang/runtime is copied.
function SlangRuntimeCopy(destDir)
    if CopyGlob(Dep.slang.runtime .. "/*", destDir) == 0 then
        print("premake: WARNING '" .. Dep.slang.runtime ..
              "' is empty — run 'python3 scripts/setup.py' before building.")
    end
end

-- Copy shader assets (*.slang + *.ehmaterialtype) from each `dir` (repo-root-relative)
-- into <destDir>/Shaders. e.g. CopyShaders(dest, "Ray/Shaders", "EchelonEditor/Shaders")
function CopyShaders(destDir, ...)
    local shaderDir = destDir .. "/Shaders"
    for _, dir in ipairs({...}) do
        CopyGlob(dir .. "/*.slang", shaderDir)
        CopyGlob(dir .. "/*.ehmaterialtype", shaderDir)
    end
end
