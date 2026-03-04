#pragma once

/**
 * @file RayShaders.hpp
 * @brief Embedded GLSL shader sources for the Ray PBR renderer.
 *
 * These are intentionally kept simple for initial bring-up.
 * A vertex shader that applies model-view-projection transforms,
 * and a fragment shader that outputs a solid colour modulated
 * by a basic diffuse lighting calculation.
 */

namespace Echelon::RayShaders {

    // ================================================================
    // Basic vertex shader  (layout: position, normal, texcoord)
    // ================================================================

    inline constexpr const char* BasicVertexShader = R"glsl(
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

    // ================================================================
    // Basic fragment shader  (simple directional light + base colour)
    // ================================================================

    inline constexpr const char* BasicFragmentShader = R"glsl(
#version 460 core

in vec3 v_WorldPos;
in vec3 v_Normal;
in vec2 v_TexCoord;

uniform vec4 u_BaseColor;        // RGBA base colour
uniform vec3 u_LightDir;         // direction TO the light (normalised)
uniform vec3 u_LightColor;       // light radiance
uniform vec3 u_CameraPos;        // camera world position

out vec4 FragColor;

void main()
{
    vec3 N = normalize(v_Normal);
    vec3 L = normalize(u_LightDir);
    vec3 V = normalize(u_CameraPos - v_WorldPos);
    vec3 H = normalize(L + V);

    // Ambient
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * u_LightColor;

    // Diffuse (Lambert)
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * u_LightColor;

    // Specular (Blinn-Phong)
    float spec = pow(max(dot(N, H), 0.0), 64.0);
    vec3 specular = spec * u_LightColor;

    vec3 result = (ambient + diffuse + specular) * u_BaseColor.rgb;
    FragColor = vec4(result, u_BaseColor.a);
}
)glsl";

    // ================================================================
    // Flat colour shader  (no lighting, just base colour + MVP)
    // ================================================================

    inline constexpr const char* FlatVertexShader = R"glsl(
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

    inline constexpr const char* FlatFragmentShader = R"glsl(
#version 460 core

in vec3 v_Color;

out vec4 FragColor;

void main()
{
    FragColor = vec4(v_Color, 1.0);
}
)glsl";

} // namespace Echelon::RayShaders
