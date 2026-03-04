-- Dependencies.lua
-- Centralized dependency include paths for the entire workspace.
-- To add a new vendor library:
--   1. Add it to the IncludeDir table below
--   2. Drop the library source into Vendor/
--   3. (Optional) Add a project entry in Vendor/premake5.lua if it needs compilation

IncludeDir = {}
IncludeDir["spdlog"]  = "%{wks.location}/Vendor/spdlog/include"
IncludeDir["glm"]     = "%{wks.location}/Vendor/glm"
IncludeDir["entt"]    = "%{wks.location}/Vendor/entt/single_include"
IncludeDir["GLFW"]    = "%{wks.location}/Vendor/GLFW/include"
IncludeDir["yaml"]    = "%{wks.location}/Vendor/yaml/include"
