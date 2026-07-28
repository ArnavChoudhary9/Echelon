#pragma once

/**
 * @file RayShaders.hpp
 * @brief Shader source loader for the Ray renderer.
 *
 * Loads GLSL shader sources from external files in the Shaders/ directory
 * next to the renderer DLL.  Falls back to embedded sources if the files
 * cannot be found (e.g. during testing or standalone builds).
 */

#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace Echelon::RayShaders {

    // ------------------------------------------------------------------
    // File loader helper
    // ------------------------------------------------------------------

    /**
     * @brief Read an entire text file into a std::string.
     * @return The file contents, or an empty string on failure.
     */
    inline std::string ReadFile(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::in);
        if (!file.is_open()) return {};
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    /**
     * @brief Try to load a shader file relative to the executable.
     *
     * Searches in:
     *   1. <cwd>/Shaders/<filename>          (shipped layout)
     *   2. <cwd>/../../../Ray/Shaders/<filename>  (dev source tree)
     *
     * @return File contents or empty string.
     */
    inline std::string LoadShaderFile(const std::string& filename) {
        // 1. Shipped layout — Shaders/ next to the exe (post-build copy)
        auto p1 = std::filesystem::path("Shaders") / filename;
        auto content = ReadFile(p1);
        if (!content.empty()) return content;

        // 2. Dev layout — running from bin/<Config>/<App>/ in the source tree
        auto p2 = std::filesystem::path("../../../Ray/Shaders") / filename;
        content = ReadFile(p2);
        if (!content.empty()) return content;

        return {};
    }

    // ==================================================================
    // Embedded fallbacks (compile-time constants)
    // ==================================================================

    inline constexpr const char* BasicVertexShaderFallback = R"glsl(
#version 460 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;

out vec3 v_WorldPos;
out vec3 v_Normal;
out vec2 v_TexCoord;

void main()
{
    vec4 worldPos = u_Model * vec4(a_Position, 1.0);
    v_WorldPos    = worldPos.xyz;
    v_Normal      = mat3(transpose(inverse(u_Model))) * a_Normal;
    v_TexCoord    = a_TexCoord;
    gl_Position   = u_Projection * u_View * worldPos;
}
)glsl";

    inline constexpr const char* BasicFragmentShaderFallback = R"glsl(
#version 460 core

in vec3 v_WorldPos;
in vec3 v_Normal;
in vec2 v_TexCoord;

uniform vec4 u_BaseColor;
uniform vec3 u_LightDir;
uniform vec3 u_LightColor;
uniform vec3 u_CameraPos;

out vec4 FragColor;

void main()
{
    vec3 N = normalize(v_Normal);
    vec3 L = normalize(u_LightDir);
    vec3 V = normalize(u_CameraPos - v_WorldPos);
    vec3 H = normalize(L + V);

    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * u_LightColor;

    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * u_LightColor;

    float spec = pow(max(dot(N, H), 0.0), 64.0);
    vec3 specular = spec * u_LightColor;

    vec3 result = (ambient + diffuse + specular) * u_BaseColor.rgb;
    FragColor = vec4(result, u_BaseColor.a);
}
)glsl";

    inline constexpr const char* FlatVertexShaderFallback = R"glsl(
#version 460 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Color;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;

out vec3 v_Color;

void main()
{
    v_Color     = a_Color;
    gl_Position = u_Projection * u_View * u_Model * vec4(a_Position, 1.0);
}
)glsl";

    inline constexpr const char* FlatFragmentShaderFallback = R"glsl(
#version 460 core

in vec3 v_Color;

out vec4 FragColor;

void main()
{
    FragColor = vec4(v_Color, 1.0);
}
)glsl";

    // ==================================================================
    // Public accessors — prefer file, fall back to embedded
    // ==================================================================

    inline std::string GetBasicVertexShader() {
        auto src = LoadShaderFile("Basic.vert.glsl");
        return src.empty() ? std::string(BasicVertexShaderFallback) : src;
    }

    inline std::string GetBasicFragmentShader() {
        auto src = LoadShaderFile("Basic.frag.glsl");
        return src.empty() ? std::string(BasicFragmentShaderFallback) : src;
    }

    inline std::string GetFlatVertexShader() {
        auto src = LoadShaderFile("Flat.vert.glsl");
        return src.empty() ? std::string(FlatVertexShaderFallback) : src;
    }

    inline std::string GetFlatFragmentShader() {
        auto src = LoadShaderFile("Flat.frag.glsl");
        return src.empty() ? std::string(FlatFragmentShaderFallback) : src;
    }

} // namespace Echelon::RayShaders
