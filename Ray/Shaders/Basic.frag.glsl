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
