// PBR.slang — the built-in RENDERER standard material shader (ships next to the
// executable, referenced as `shader:PBR.slang`). Physically based metallic-roughness
// shading: Cook-Torrance direct lighting over g_Lights (with range attenuation + spot
// cones + per-type shadow maps) and image-based ambient (irradiance + prefiltered
// specular + BRDF LUT). Outputs LINEAR HDR — the pipeline tonemaps at the end.
//
// Texture slots (all optional; a Use* flag gates each so the white fallback the material
// system binds for an absent map is never misinterpreted):
//   u_Albedo      rgb base color   (sRGB)
//   u_Normal      tangent-space normal
//   u_MetalRough  g = roughness, b = metallic   (glTF convention); r = occlusion (ORM)
//   u_Emissive    rgb emissive

import Echelon;   // g_Frame/g_Object/g_Lights + g_Shadows/g_Ibl + PCF helpers + VertexInput/Output

static const float PI = 3.14159265359;

struct MaterialParams
{
    float4 BaseColor;      // rgb albedo tint, a = alpha
    float4 Emissive;       // rgb emissive color (already scaled), a unused
    float  Metallic;       // scalar metallic  (× map.b when a metal-rough map is present)
    float  Roughness;      // scalar roughness (× map.g)
    float  Occlusion;      // constant AO when no ORM map (default 1)
    float  NormalScale;    // tangent-space normal strength
    float  UseAlbedoMap;
    float  UseNormalMap;
    float  UseMetalRoughMap;
    float  UseEmissiveMap;
};
ConstantBuffer<MaterialParams> g_Material;

Sampler2D u_Albedo;
Sampler2D u_Normal;
Sampler2D u_MetalRough;
Sampler2D u_Emissive;

[shader("vertex")]
VertexOutput vertexMain(VertexInput vin)
{
    VertexOutput o;
    float4 world = mul(g_Object.Model, float4(vin.Position, 1.0));
    o.WorldPos   = world.xyz;
    o.ClipPos    = mul(g_Frame.Projection, mul(g_Frame.View, world));
    o.WorldNrm   = mul((float3x3)g_Object.NormalMatrix, vin.Normal);
    o.TexCoord   = vin.TexCoord;
    return o;
}

// ---- BRDF terms ----
float DistributionGGX(float3 N, float3 H, float rough)
{
    float a  = rough * rough;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float d = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
    return a2 / max(PI * d * d, 1e-7);
}

float GeometrySchlickGGX(float NdotX, float rough)
{
    float r = rough + 1.0;
    float k = (r * r) / 8.0;                 // direct-lighting k
    return NdotX / (NdotX * (1.0 - k) + k);
}

float GeometrySmith(float NdotV, float NdotL, float rough)
{
    return GeometrySchlickGGX(NdotV, rough) * GeometrySchlickGGX(NdotL, rough);
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float rough)
{
    float3 Fr = max(float3(1.0 - rough), F0);
    return F0 + (Fr - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

// Derivative-based tangent frame → no per-vertex tangents needed.
float3 ApplyNormalMap(float3 N, float3 worldPos, float2 uv, float3 mapN)
{
    float3 dp1  = ddx(worldPos);
    float3 dp2  = ddy(worldPos);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);

    float3 dp2perp = cross(dp2, N);
    float3 dp1perp = cross(N, dp1);
    float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    float invmax = rsqrt(max(dot(T, T), dot(B, B)));
    return normalize(T * (mapN.x * invmax) + B * (mapN.y * invmax) + N * mapN.z);
}

[shader("fragment")]
float4 fragmentMain(VertexOutput fin) : SV_Target
{
    // ---- Surface inputs ----
    float3 albedo = g_Material.BaseColor.rgb;
    if (g_Material.UseAlbedoMap > 0.5)
        albedo *= u_Albedo.Sample(fin.TexCoord).rgb;

    float metallic  = g_Material.Metallic;
    float roughness = g_Material.Roughness;
    float ao        = g_Material.Occlusion;
    if (g_Material.UseMetalRoughMap > 0.5)
    {
        float3 mr  = u_MetalRough.Sample(fin.TexCoord).rgb;
        roughness *= mr.g;
        metallic  *= mr.b;
        ao        *= mr.r;                     // ORM occlusion in R (1.0 when unused)
    }
    roughness = clamp(roughness, 0.04, 1.0);
    metallic  = saturate(metallic);

    float3 N = normalize(fin.WorldNrm);
    if (g_Material.UseNormalMap > 0.5)
    {
        float3 mapN = u_Normal.Sample(fin.TexCoord).xyz * 2.0 - 1.0;
        mapN.xy *= g_Material.NormalScale;
        N = ApplyNormalMap(N, fin.WorldPos, fin.TexCoord, normalize(mapN));
    }

    float3 V = normalize(g_Frame.CameraPosition.xyz - fin.WorldPos);
    float NdotV = max(dot(N, V), 1e-4);
    float3 F0 = lerp(float3(0.04), albedo, metallic);

    // ---- Direct lighting (Cook-Torrance over all scene lights) ----
    int dirIdx   = int(g_Shadows.DirParams.x);
    int spotIdx  = int(g_Shadows.SpotParams.x);
    int pointIdx = int(g_Shadows.PointPosIndex.w);

    float3 Lo = float3(0.0);
    for (int i = 0; i < g_Lights.Count.x; ++i)
    {
        GpuLight Lgt = g_Lights.Lights[i];
        int type = int(Lgt.Position.w);          // 0 = dir, 1 = point, 2 = spot

        // Direction to light + distance attenuation.
        float3 L;
        float  atten = 1.0;
        if (type == 0)
        {
            L = normalize(-Lgt.Direction.xyz);
        }
        else
        {
            float3 toLight = Lgt.Position.xyz - fin.WorldPos;
            float  dist    = length(toLight);
            L = toLight / max(dist, 1e-4);
            float range = max(Lgt.Direction.w, 1e-3);
            float d2    = dist * dist;
            float win   = saturate(1.0 - pow(dist / range, 4.0));
            atten = (win * win) / max(d2, 1e-4);
        }

        // Spot cone falloff.
        if (type == 2)
        {
            float cosAng   = dot(normalize(-L), normalize(Lgt.Direction.xyz));
            float cosInner = Lgt.SpotParams.x;
            float cosOuter = Lgt.SpotParams.y;
            atten *= saturate((cosAng - cosOuter) / max(cosInner - cosOuter, 1e-4));
        }

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL <= 0.0 || atten <= 0.0) continue;

        // Per-type shadowing (one caster per type).
        float shadow = 1.0;
        if (type == 0 && i == dirIdx)
            shadow = lerp(1.0, PcfShadow2D(g_ShadowDir, g_Shadows.DirViewProj, fin.WorldPos, N, L,
                                           g_Shadows.DirParams.y, g_Shadows.DirParams.z, g_Shadows.MapParams.x),
                          g_Shadows.DirParams.w);
        else if (type == 2 && i == spotIdx)
            shadow = lerp(1.0, PcfShadow2D(g_ShadowSpot, g_Shadows.SpotViewProj, fin.WorldPos, N, L,
                                           g_Shadows.SpotParams.y, g_Shadows.SpotParams.z, g_Shadows.MapParams.y),
                          g_Shadows.SpotParams.w);
        else if (type == 1 && i == pointIdx)
        {
            float3 fromLight = fin.WorldPos - g_Shadows.PointPosIndex.xyz;
            float  curDist   = length(fromLight);
            shadow = lerp(1.0, PcfShadowCube(g_ShadowPoint, fromLight, curDist,
                                             g_Shadows.PointParams.x, g_Shadows.PointParams.y),
                          g_Shadows.PointParams.z);
        }

        float3 H = normalize(V + L);
        float  D = DistributionGGX(N, H, roughness);
        float  G = GeometrySmith(NdotV, NdotL, roughness);
        float3 Fr = FresnelSchlick(max(dot(H, V), 0.0), F0);

        float3 spec = (D * G * Fr) / max(4.0 * NdotV * NdotL, 1e-4);
        float3 kd   = (float3(1.0) - Fr) * (1.0 - metallic);
        float3 diffuse = kd * albedo / PI;

        float3 radiance = Lgt.Color.rgb * Lgt.Color.w * atten;
        Lo += (diffuse + spec) * radiance * NdotL * shadow;
    }

    // ---- Ambient / indirect ----
    float3 ambient;
    if (g_Ibl.Params.z > 0.5)
    {
        float3 F = FresnelSchlickRoughness(NdotV, F0, roughness);
        float3 kd = (float3(1.0) - F) * (1.0 - metallic);

        float3 irradiance = g_IrradianceMap.Sample(N).rgb;
        float3 diffuseIBL = irradiance * albedo;

        float3 R = reflect(-V, N);
        float  maxMip = g_Ibl.Params.x;
        float3 prefiltered = g_PrefilterMap.SampleLevel(R, roughness * maxMip).rgb;
        float2 brdf = g_BrdfLUT.Sample(float2(NdotV, roughness)).rg;
        float3 specularIBL = prefiltered * (F * brdf.x + brdf.y);

        ambient = (kd * diffuseIBL + specularIBL) * ao * g_Ibl.Params.y;
    }
    else
    {
        ambient = g_Lights.Ambient.rgb * albedo * ao;
    }

    // ---- Emissive ----
    float3 emissive = g_Material.Emissive.rgb;
    if (g_Material.UseEmissiveMap > 0.5)
        emissive *= u_Emissive.Sample(fin.TexCoord).rgb;

    float3 color = ambient + Lo + emissive;
    return float4(color, g_Material.BaseColor.a);
}
